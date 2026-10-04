// ============================================================
//  UE_AudioManager — ядро звуковой системы мода «унесённые»
//  Платит/останавливает источники звука, считает затухание по
//  расстоянию, синхронизирует всех игроков.
// ============================================================

class UE_SourceType
{
    CASSETTE = 0;   // кассетный плеер
    DISK = 1;       // дисковый проигрыватель
    RADIO = 2;      // интернет-радио (стрим)
    CAR = 3;        // автомобильная магнитола
};

class UE_PlaybackState
{
    int id;             // уникальный id источника на сервере
    string className;   // класс объекта-носителя
    Object object;      // ссылка на объект (плеер/машина)
    vector position;    // позиция источника (для машин обновляется)
    float volume;       // базовая громкость 0..1
    bool isPlaying;
    int type;           // UE_SourceType
    string stationKey;  // ключ радиостанции (для типа RADIO)
    string playlist;    // имя плейлиста (кассета/диск)
    float startedAt;    // время старта (серверное), для синхронизации трека
};

class UE_AudioManager: ScriptModule
{
    // все активные источники id -> стейт
    ref map<int, ref UE_PlaybackState> m_Sources;
    int m_NextId = 1;
    private float m_MaxHearDist = 150.0;
    private float m_MinVolDist = 5.0;
    private float m_FadeCurve = 2.0;
    private float m_TickInterval = 0.5;
    private float m_LastTickTime = 0;
    private bool m_IsServer;

    void UE_AudioManager()
    {
        m_Sources = new map<int, ref UE_PlaybackState>;
        m_IsServer = GetGame().IsDedicated();
    }

    //~ ---------------------------------------------------------
    //~  Инициализация параметров из config.cpp (UE_Config)
    //~ ---------------------------------------------------------
    void InitFromConfig()
    {
        ParamConvert parse;
        // читаем значения напрямую из конфига миссии
        float v;
        if (GetGame().ConfigGetFloat("UE_Config maxHearDistance", v)) m_MaxHearDist = v;
        if (GetGame().ConfigGetFloat("UE_Config minVolumeDistance", v)) m_MinVolDist = v;
        if (GetGame().ConfigGetFloat("UE_Config fadeCurve", v)) m_FadeCurve = v;
    }

    //~ ---------------------------------------------------------
    //~  Расчёт громкости с учётом расстояния (затухание)
    //~  volume = base * clamp( (d0/d)^curve , 0..1 )
    //~  d <= m_MinVolDist -> максимум, d >= m_MaxHearDist -> 0
    //~ ---------------------------------------------------------
    float CalcAttenuation(vector srcPos, float baseVol)
    {
        PlayerBase pl = PlayerBase.Cast(GetGame().GetPlayer());
        if (!pl) return 0;
        vector plPos = pl.GetPosition();
        float dist = vector.Distance(srcPos, plPos);
        if (dist >= m_MaxHearDist) return 0.0;
        if (dist <= m_MinVolDist) return baseVol;
        float ratio = m_MinVolDist / dist;              // <1 при отдалении
        float atten = Math.Pow(ratio, m_FadeCurve);     // квадратичное затухание
        // дополнительно мягко режем хвост у границы слышимости
        float edgeFade = (m_MaxHearDist - dist) / (m_MaxHearDist - m_MinVolDist);
        atten = atten * Math.Clamp(edgeFade * 1.5, 0, 1);
        return baseVol * atten;
    }

    //~ ---------------------------------------------------------
    //~  Создание источника (вызывается ТОЛЬКО на сервере после
    //~  валидации через UE_Security). Рассылает всем клиентам.
    //~ ---------------------------------------------------------
    int CreateSource(Object obj, int type, string stationKey, string playlist, float vol)
    {
        if (!obj) return -1;
        UE_PlaybackState st = new UE_PlaybackState;
        st.id = m_NextId++;
        st.className = obj.GetType();
        st.object = obj;
        st.position = obj.GetPosition();
        st.volume = Math.Clamp(vol, 0.0, 1.0);
        st.isPlaying = true;
        st.type = type;
        st.stationKey = stationKey;
        st.playlist = playlist;
        st.startedAt = GetGame().GetTime() * 0.001;
        m_Sources.Set(st.id, st);

        // широковещательная команда всем игрокам
        array<ref ObjNetObject> netObjs = {};
        Ref<Object> o = st.object;
        netObjs.Insert(o);
        RPC_CreateSource(st.id, type, stationKey, playlist, st.position, st.volume, st.startedAt);
        DebugPrint("unestennye: источник #" + st.id + " создан (" + GetStationName(stationKey) + ")");
        return st.id;
    }

    void StopSource(int id)
    {
        UE_PlaybackState st;
        if (!m_Sources.Find(id, st)) return;
        st.isPlaying = false;
        RPC_StopSource(id);
        m_Sources.Remove(id);
        DebugPrint("unestennye: источник #" + id + " остановлен");
    }

    void StopAllByObject(Object obj)
    {
        int toRemove[16]; int n = 0;
        for (int i = 0; i < m_Sources.Count(); i++)
        {
            auto kv = m_Sources.GetByIndex(i);
            UE_PlaybackState st = kv.Get2();
            if (st.object == obj) { toRemove[n] = st.id; n++; }
        }
        for (int k = 0; k < n; k++) StopSource(toRemove[k]);
    }

    //~ ---------------------------------------------------------
    //~  Периодический тик: пересчёт громкости у клиентов,
    //~  обновление позиции движущихся источников (машины).
    //~ ---------------------------------------------------------
    void OnUpdate(float timeDelta)
    {
        float now = GetGame().GetTime() * 0.001;
        if (now - m_LastTickTime < m_TickInterval) return;
        m_LastTickTime = now;

        for (int i = 0; i < m_Sources.Count(); i++)
        {
            UE_PlaybackState st = m_Sources.GetByIndex(i).Get2();
            if (!st || !st.isPlaying) continue;

            // у машин позиция меняется — обновляем и ретранслируем
            if (st.type == UE_SourceType.CAR && st.object)
            {
                vector p = st.object.GetPosition();
                if (vector.Distance(p, st.position) > 2.0)
                {
                    st.position = p;
                    if (m_IsServer) RPC_UpdatePosition(st.id, p);
                }
            }

            // клиентский пересчёт громкости локально
            if (!m_IsServer) ApplyLocalVolume(st);
        }
    }

    void ApplyLocalVolume(UE_PlaybackState st)
    {
        float v = CalcAttenuation(st.position, st.volume);
        // применяем к локальному звуковому источнику клиента
        UE_LocalSound snd;
        if (ClientSounds().Find(st.id, snd))
        {
            snd.SetVolume(v);
        }
    }

    static UE_AudioManager Instance()
    {
        static UE_AudioManager s_Inst;
        if (!s_Inst) s_Inst = new UE_AudioManager;
        return s_Inst;
    }

    static string GetStationName(string key)
    {
        if (key == "Apex") return "Апекс ФМ";
        if (key == "EuropaPlus") return "Европа Плюс";
        if (key == "HumorFM") return "Юмор FM";
        return "";
    }

    static string GetStationURL(string key)
    {
        if (key == "Apex") return "http://62.152.59.3:8000/nkz";
        if (key == "EuropaPlus") return "http://online-2.gkvr.ru:8000/europa_nkz_64.aac";
        if (key == "HumorFM") return "http://62.231.184.253:8000/humor";
        return "";
    }

    // карта локальных звуковых источников клиента
    static ref map<int, ref UE_LocalSound> ClientSounds()
    {
        static map<int, ref UE_LocalSound> s_Map;
        if (!s_Map) s_Map = new map<int, ref UE_LocalSound>;
        return s_Map;
    }
};

// Локальный проигрываемый звук на клиенте
class UE_LocalSound
{
    SoundSource m_Src;
    float m_BaseVol;
    int m_Id;
    string m_FileOrStream;
    bool m_Loop;

    void Play(string fileOrStream, vector pos, float vol, bool loop)
    {
        m_Id = 0; m_BaseVol = vol; m_FileOrStream = fileOrStream; m_Loop = loop;
        Object o = GetGame().CreateSoundSource(pos, fileOrStream);
        m_Src = SoundSource.Cast(o);
        if (m_Src) { m_Src.SetVolume(vol); if (loop) m_Src.PlayLoop(); else m_Src.Play(); }
    }
    void SetVolume(float v) { if (m_Src) m_Src.SetVolume(v); }
    void Stop() { if (m_Src) { m_Src.Stop(); delete m_Src; m_Src = null; } }
};
