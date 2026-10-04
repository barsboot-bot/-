// ============================================================
//  ue_bridge.h — публичный API аудио-прослойки мода «унесённые»
//  UEAudioBridge.dll — мост между DayZ (EnforceScript) и BASS.
//
//  Возможности:
//   • воспроизведение файлов кассет/дисков (.ogg/.mp3/.wav);
//   • HTTP-радиопотоки ICY/Shoutcast (MP3/AAC) — Апекс, Европа+, Юмор FM;
//   • позиционирование и затухание по расстоянию;
//   • синхронизация позиции трека между игроками.
//
//  Все функции stdcall, экспорт по имени. Строки — UTF-8 (const char*).
//  Возврат TRUE = успех, FALSE = ошибка (текст: UE_GetLastError).
// ============================================================
#pragma once

#include <windows.h>

typedef DWORD UE_SOUND_HANDLE;   // идентификатор источника на клиенте

#ifdef __cplusplus
extern "C" {
#endif

// ---------- Жизненный цикл ----------
BOOL WINAPI UE_Initialize(float masterVolumeDb, float musicBusGain);
void WINAPI UE_Shutdown(void);
const char* WINAPI UE_GetVersion(void);     // например "1.0.0"
const char* WINAPI UE_GetLastError(void);   // "" если ошибок нет

// ---------- Файловое воспроизведение (кассеты / диски) ----------
// filePath — относительный путь от корня папки клиента
//            (например "mods/@Unesennye/Sounds/cassettes/rock01.ogg")
//            или абсолютный путь вне PBO. loop — зациклить.
// volume 0..1, posX/Y/Z — позиция источника в мире.
BOOL WINAPI UE_PlayFile(const char* filePath, BOOL loop, float volume,
                        float posX, float posY, float posZ,
                        UE_SOUND_HANDLE* outHandle);

// ---------- Радио-поток (HTTP ICY/Shoutcast) ----------
// url — адрес станции (белый список проверяется скриптом!).
// bufferMs — сетевой буфер, рекомендуется 3000..8000.
BOOL WINAPI UE_PlayStream(const char* url, float volume,
                          float posX, float posY, float posZ,
                          DWORD bufferMs, UE_SOUND_HANDLE* outHandle);

// ---------- Управление активным источником ----------
BOOL WINAPI UE_Stop(UE_SOUND_HANDLE handle);
BOOL WINAPI UE_SetVolume(UE_SOUND_HANDLE handle, float volume);          // 0..1
BOOL WINAPI UE_SetPosition(UE_SOUND_HANDLE handle, float x, float y, float z);
BOOL WINAPI UE_Pause(UE_SOUND_HANDLE handle, BOOL pause);

// ---------- Состояние ----------
// Позиция слушателя (игрока). Мост сам пересчитывает затухание всех источников.
BOOL WINAPI UE_SetListenerPosition(float x, float y, float z);
// outIsPlaying — играет ли; outPositionSec — позиция трека в секундах
// (для стрима 0). positionSec используется скриптом для синхронного
// старта трека у всех игроков.
BOOL WINAPI UE_GetState(UE_SOUND_HANDLE handle, BOOL* outIsPlaying, float* outPositionSec);
BOOL WINAPI UE_IsStreamConnected(UE_SOUND_HANDLE handle, BOOL* outConnected);

// ---------- Тест без DayZ ----------
// Прогрывает файл или URL N секунд с console-затуханием демо. 0 = ок.
int WINAPI UE_SelfTest(const char* fileOrUrl, int secondsToPlay);

#ifdef __cplusplus
}
#endif
