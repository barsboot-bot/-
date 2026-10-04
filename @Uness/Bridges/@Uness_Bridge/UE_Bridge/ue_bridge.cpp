// ============================================================
//  ue_bridge.cpp — реализация аудио-прослойки (BASS)
//  Собирается в UEAudioBridge.dll (см. build/build_msvc.bat).
//  Зависимости: bass.dll, bass_aac.dll (лежат рядом с DLL).
// ============================================================
#include "../include/ue_bridge.h"
#include "bass.h"        // http://www.un4seen.com  (BASS 2.4+)
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <vector>
#include <mutex>

static const char* BRIDGE_VERSION = "1.0.0";

// ---------- внутреннее состояние ----------
struct UESource
{
    UE_SOUND_HANDLE id;
    HSTREAM        stream;      // BASS-поток (файл или интернет)
    BOOL           isStream;
    float          baseVolume;  // 0..1 из скрипта
    float          curVolume;   // после затухания
    float          pos[3];
    float          refDist;     // дистанция, на которой громкость = base
    float          maxDist;     // дальше — тишина
    bool           inUse;
};

static std::vector<UESource>    g_Sources(64);
static std::mutex               g_Lock;
static bool                     g_Init = false;
static char                     g_LastErr[256] = {0};
static DWORD                    g_NextId = 1;
static float                    g_MusicBusGain = 1.0f;

// параметры позиционирования (совпадают с UE_Config в config.cpp)
static const float REF_DIST = 5.0f;    // minVolumeDistance
static const float MAX_DIST = 150.0f;  // maxHearDistance
static const float CURVE    = 2.0f;    // fadeCurve

static void SetErr(const char* fmt, ...)
{
    va_list ap; va_start(ap, fmt);
#if defined(_MSC_VER)
    _snprintf_s(g_LastErr, sizeof(g_LastErr), _TRUNCATE, fmt, ap);
#else
    vsnprintf(g_LastErr, sizeof(g_LastErr), fmt, ap);
#endif
    va_end(ap);
}

// регистронезависимая проверка префикса http (портативно для MSVC/MinGW)
static bool IsHttpUrl(const char* s)
{
    if (!s) return false;
    const char* p = s; int n = 0;
    while (*p && n < 4) { char c = *p++; if (c >= 'A' && c <= 'Z') c += 32; if (c != "http"[n]) return false; ++n; }
    return n == 4;
}

static float CalcAttenuation(const float p[3], float listener[3])
{
    float dx = p[0]-listener[0], dy = p[1]-listener[1], dz = p[2]-listener[2];
    float d = sqrtf(dx*dx + dy*dy + dz*dz);
    if (d >= MAX_DIST) return 0.0f;
    if (d <= REF_DIST) return 1.0f;
    float r = REF_DIST / d;
    float atten = powf(r, (float)CURVE);
    float edge = (MAX_DIST - d) / (MAX_DIST - REF_DIST);
    atten *= fminf(edge * 1.5f, 1.0f);
    return atten;
}

// «слушатель» = камера/игрок. В DayZ позицию игрока присылает скрипт
// через UE_SetListenerPosition (расширение API ниже), пока — из BASS 3D.
static float g_Listener[3] = {0,0,0};

BOOL WINAPI UE_SetListenerPosition(float x, float y, float z)
{
    g_Listener[0]=x; g_Listener[1]=y; g_Listener[2]=z;
    // пересчёт громкостей всех источников
    std::lock_guard<std::mutex> lk(g_Lock);
    for (auto& s : g_Sources)
        if (s.inUse && s.stream)
        {
            float a = CalcAttenuation(s.pos, g_Listener);
            float v = s.baseVolume * a;
            BASS_ChannelSetAttribute(s.stream, BASS_ATTRIB_VOL, v);
            s.curVolume = v;
        }
    return TRUE;
}

// ---------- жизненный цикл ----------
BOOL WINAPI UE_Initialize(float masterVolumeDb, float musicBusGain)
{
    if (g_Init) return TRUE;
    if (!BASS_Init(-1, 48000, 0, GetDesktopWindow(), NULL))
    {
        SetErr("BASS_Init failed, err=%d", BASS_ErrorGetCode());
        return FALSE;
    }
    BASS_SetConfig(BASS_CONFIG_GVOL_STREAM,
                   (DWORD)(powf(10.0f, masterVolumeDb / 20.0f) * 100.0f));
    g_MusicBusGain = musicBusGain > 0 ? musicBusGain : 1.0f;
    BASS_Start();
    g_Init = true;
    return TRUE;
}

void WINAPI UE_Shutdown(void)
{
    if (!g_Init) return;
    std::lock_guard<std::mutex> lk(g_Lock);
    for (auto& s : g_Sources)
        if (s.inUse && s.stream) { BASS_StreamFree(s.stream); s.stream = 0; s.inUse = false; }
    BASS_Stop();
    BASS_Free();
    g_Init = false;
}

const char* WINAPI UE_GetVersion(void)  { return BRIDGE_VERSION; }
const char* WINAPI UE_GetLastError(void){ return g_LastErr; }

static UESource* AcquireSlot(UE_SOUND_HANDLE* outId)
{
    for (auto& s : g_Sources)
        if (!s.inUse)
        {
            s.inUse = true; s.id = g_NextId++; s.isStream = FALSE;
            s.refDist = REF_DIST; s.maxDist = MAX_DIST;
            *outId = s.id;
            return &s;
        }
    SetErr("no free source slots (max %d)", (int)g_Sources.size());
    return nullptr;
}

static UESource* FindSrc(UE_SOUND_HANDLE id)
{
    for (auto& s : g_Sources) if (s.inUse && s.id == id) return &s;
    return nullptr;
}

// авто-освобождение слота когда файл доиграл (не для стрима)
static void CALLBACK EndSync(HSYNC handle, DWORD channel, DWORD data, void* user)
{
    std::lock_guard<std::mutex> lk(g_Lock);
    UESource* s = (UESource*)user;
    if (s && s->inUse && s->stream == channel)
    {
        BASS_StreamFree(s->stream);
        s->stream = 0;
        s->inUse = false;
    }
}

// ---------- файлы ----------
BOOL WINAPI UE_PlayFile(const char* filePath, BOOL loop, float volume,
                        float posX, float posY, float posZ,
                        UE_SOUND_HANDLE* outHandle)
{
    if (!g_Init) { SetErr("not initialized"); return FALSE; }
    if (!filePath || !outHandle) { SetErr("bad args"); return FALSE; }

    std::lock_guard<std::mutex> lk(g_Lock);
    UESource* s = AcquireSlot(outHandle);
    if (!s) return FALSE;

    DWORD flags = BASS_SAMPLE_SOFTWARE | (loop ? BASS_SAMPLE_LOOP : 0);
    HSTREAM st = BASS_StreamCreateFile(FALSE, filePath, 0, 0, flags);
    if (!st)
        st = BASS_StreamCreateFile(TRUE, filePath, 0, 0, flags); // unicode-путь
    if (!st)
    {
        s->inUse = false;
        SetErr("BASS_StreamCreateFile('%s') failed, err=%d", filePath, BASS_ErrorGetCode());
        return FALSE;
    }
    s->stream = st;
    s->baseVolume = volume * g_MusicBusGain;
    s->pos[0]=posX; s->pos[1]=posY; s->pos[2]=posZ;

    float a = CalcAttenuation(s->pos, g_Listener);
    BASS_ChannelSetAttribute(st, BASS_ATTRIB_VOL, s->baseVolume * a);
    if (!loop)
        BASS_ChannelSetSync(st, BASS_SYNC_END, 0, EndSync, s);
    BASS_ChannelPlay(st, FALSE);
    return TRUE;
}

// ---------- радио-стримы ----------
// Колбэк ICY-метаданных — просто логируем название трека в debug-отладчик.
static void CALLBACK MetaProc(HSTREAM handle, int chunk, char* title, void* user)
{
    // chunk==0 -> конец потока; title может быть NULL
    // (для мода метаданные не обязательны, оставляем точку расширения)
    (void)handle; (void)chunk; (void)title; (void)user;
}

BOOL WINAPI UE_PlayStream(const char* url, float volume,
                          float posX, float posY, float posZ,
                          DWORD bufferMs, UE_SOUND_HANDLE* outHandle)
{
    if (!g_Init) { SetErr("not initialized"); return FALSE; }
    if (!url || !outHandle) { SetErr("bad args"); return FALSE; }
    // защита от file:// и прочей мути — только http
    if (!IsHttpUrl(url)) { SetErr("url must be http(s)"); return FALSE; }

    std::lock_guard<std::mutex> lk(g_Lock);
    UESource* s = AcquireSlot(outHandle);
    if (!s) return FALSE;

    if (bufferMs < 1000) bufferMs = 5000;
    BASS_SetConfig(BASS_CONFIG_NET_PLAYLIST, 1);   // обработать плейлист-редирект
    BASS_SetConfig(BASS_CONFIG_NET_PREBUF_WAIT, FALSE);
    BASS_SetConfigPtr(BASS_CONFIG_NET_AGENT, (void*)"Mozilla/5.0 (Uness Bridge)");

    HSTREAM st = BASS_StreamCreateURL((char*)url, 0,
                BASS_SAMPLE_LOOP | BASS_STREAM_STATUS, MetaProc, NULL);
    if (!st)
    {
        s->inUse = false;
        SetErr("BASS_StreamCreateURL('%s') failed, err=%d", url, BASS_ErrorGetCode());
        return FALSE;
    }
    // размер предбуфера
    BASS_StreamPutData(st, NULL, 0); // no-op, но гарантирует инициализацию
    QWORD pre = BASS_ChannelGetLength(st, BASS_POS_BYTE);
    (void)pre;

    s->stream = st;
    s->isStream = TRUE;
    s->baseVolume = volume * g_MusicBusGain;
    s->pos[0]=posX; s->pos[1]=posY; s->pos[2]=posZ;

    float a = CalcAttenuation(s->pos, g_Listener);
    BASS_ChannelSetAttribute(st, BASS_ATTRIB_VOL, s->baseVolume * a);
    BASS_ChannelPlay(st, FALSE);
    return TRUE;
}

// ---------- управление ----------
static UESource* LockFind(UE_SOUND_HANDLE h)
{
    if (!g_Init) { SetErr("not initialized"); return nullptr; }
    std::lock_guard<std::mutex> lk(g_Lock);
    UESource* s = FindSrc(h);
    if (!s) SetErr("unknown handle %u", h);
    return s;
}

BOOL WINAPI UE_Stop(UE_SOUND_HANDLE handle)
{
    if (!g_Init) return FALSE;
    std::lock_guard<std::mutex> lk(g_Lock);
    UESource* s = FindSrc(handle);
    if (!s) { SetErr("unknown handle %u", handle); return FALSE; }
    if (s->stream) { BASS_ChannelStop(s->stream); BASS_StreamFree(s->stream); }
    s->stream = 0; s->inUse = false;
    return TRUE;
}

BOOL WINAPI UE_SetVolume(UE_SOUND_HANDLE handle, float volume)
{
    if (!g_Init) return FALSE;
    std::lock_guard<std::mutex> lk(g_Lock);
    UESource* s = FindSrc(handle);
    if (!s) { SetErr("unknown handle %u", handle); return FALSE; }
    s->baseVolume = volume * g_MusicBusGain;
    float a = CalcAttenuation(s->pos, g_Listener);
    return BASS_ChannelSetAttribute(s->stream, BASS_ATTRIB_VOL, s->baseVolume * a);
}

BOOL WINAPI UE_SetPosition(UE_SOUND_HANDLE handle, float x, float y, float z)
{
    if (!g_Init) return FALSE;
    std::lock_guard<std::mutex> lk(g_Lock);
    UESource* s = FindSrc(handle);
    if (!s) { SetErr("unknown handle %u", handle); return FALSE; }
    s->pos[0]=x; s->pos[1]=y; s->pos[2]=z;
    float a = CalcAttenuation(s->pos, g_Listener);
    return BASS_ChannelSetAttribute(s->stream, BASS_ATTRIB_VOL, s->baseVolume * a);
}

BOOL WINAPI UE_Pause(UE_SOUND_HANDLE handle, BOOL pause)
{
    if (!g_Init) return FALSE;
    std::lock_guard<std::mutex> lk(g_Lock);
    UESource* s = FindSrc(handle);
    if (!s) { SetErr("unknown handle %u", handle); return FALSE; }
    return BASS_ChannelPause(s->stream) || !pause ? TRUE : FALSE;
}

BOOL WINAPI UE_GetState(UE_SOUND_HANDLE handle, BOOL* outIsPlaying, float* outPositionSec)
{
    if (!g_Init) return FALSE;
    std::lock_guard<std::mutex> lk(g_Lock);
    UESource* s = FindSrc(handle);
    if (!s) { SetErr("unknown handle %u", handle); return FALSE; }
    if (outIsPlaying)     *outIsPlaying = BASS_ChannelIsActive(s->stream) == BASS_ACTIVE_PLAYING;
    if (outPositionSec)
    {
        if (s->isStream) *outPositionSec = 0.0f;
        else
        {
            QWORD bps = BASS_ChannelGetLength(s->stream, BASS_POS_BYTE);
            QWORD pos = BASS_ChannelGetPosition(s->stream, BASS_POS_BYTE);
            double sec = BASS_ChannelBytes2Seconds(s->stream, pos);
            *outPositionSec = (float)sec;
            (void)bps;
        }
    }
    return TRUE;
}

BOOL WINAPI UE_IsStreamConnected(UE_SOUND_HANDLE handle, BOOL* outConnected)
{
    if (!g_Init) return FALSE;
    std::lock_guard<std::mutex> lk(g_Lock);
    UESource* s = FindSrc(handle);
    if (!s || !s->isStream) { SetErr("not a stream handle"); return FALSE; }
    if (outConnected)
        *outConnected = BASS_ChannelIsActive(s->stream) != BASS_ACTIVE_STOPPED;
    return TRUE;
}

// ---------- self-test ----------
int WINAPI UE_SelfTest(const char* fileOrUrl, int secondsToPlay)
{
    if (!UE_Initialize(-6.0f, 1.0f)) { printf("[UE] init fail: %s\n", UE_GetLastError()); return 1; }
    UE_SOUND_HANDLE h = 0;
    BOOL ok;
    if (IsHttpUrl(fileOrUrl))
        ok = UE_PlayStream(fileOrUrl, 1.0f, 0,0,0, 5000, &h);
    else
        ok = UE_PlayFile(fileOrUrl, TRUE, 1.0f, 0,0,0, &h);
    if (!ok) { printf("[UE] play fail: %s\n", UE_GetLastError()); UE_Shutdown(); return 2; }
    printf("[UE] playing '%s' for %d s...\n", fileOrUrl, secondsToPlay);
    for (int i = 0; i < secondsToPlay; ++i)
    {
        Sleep(1000);
        UE_SetListenerPosition(i * 10.0f, 0, 0); // удаляемся на 10 м/сек
        BOOL playing; float psec;
        UE_GetState(h, &playing, &psec);
        printf("[UE] t=%ds listener=%dm playing=%d pos=%.1fs\n",
               i+1, (i+1)*10, playing, psec);
    }
    UE_Stop(h);
    UE_Shutdown();
    return 0;
}

// ---------- экспорт из .def-файла тоже возможен; DllMain ----------
BOOL APIENTRY DllMain(HMODULE h, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_DETACH) UE_Shutdown();
    return TRUE;
}
