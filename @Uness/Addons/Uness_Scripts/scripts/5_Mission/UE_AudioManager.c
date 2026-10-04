// ============================================================
//  UE_AudioManager — ядро звуковой системы мода «унесённые»
//  Платит/останавливает источники звука, считает затухание по
//  расстоянию, синхронизирует всех игроков.
// ============================================================



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
        // читаем значения напрямую из config.cpp (класс UE_Config)
        int v;
        if (GetGame().ConfigGetInt("UE_Config maxHearDistance", v)) m_MaxHearDist = v;
        if (GetGame().ConfigGetInt("UE_Config minVolumeDistance", v)) m_MinVolDist = v;
        if (GetGame().ConfigGetInt("UE_Config fadeCurve", v)) m_FadeCurve = v;
        if (GetGame().ConfigGetInt("UE_Config audioBridgeEnabled", v))
            UE_BridgeClient.Configure(v == 1);

        // корень внешней музыкальной библиотеки (по умолчанию <Profile>/Music)
        string root;
        if (!GetGame().ConfigGetString("UE_Config musicRoot", root)) root = "Music";
        UE_MusicLibrary.SetRoot(root);

        if (m_IsServer)
        {
            // скан внешних папок Music/Type и Music/CD + Radio.txt,
            // сбор манифеста для клиентов
            UE_MusicLibrary.ServerScan();
            UE_MusicLibrary.ServerLoadRadioTxt();
            string baseUrl;
            if (!GetGame().ConfigGetString("UE_Config libraryBaseURL", baseUrl)) baseUrl = "";
            UE_MusicLibrary.s_Manifest = UE_MusicLibrary.EncodeManifest(baseUrl);
        }
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

        // если плеер/машина уже играет — сначала гасим старый источник
        StopAllByObject(obj);

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

        // широковещательная команда всем игрокам (JIP=true —
        // опоздавшие клиенты получат пакет при спавне)
        UE_ModulePlayer.RPC_CreateSource(st.id, type, stationKey, playlist, st.position, st.volume, st.startedAt);
        Print("[унесённые] источник #" + st.id + " создан (" + DescribeSource(stationKey, playlist) + ")");
        return st.id;
    }

    static ref map<int, ref UE_PlaybackState> s_ClientMirror;   // id -> состояние (клиент)

    //~ человекочитаемое описание источника (по внешней библиотеке)
    static string DescribeSource(string stationKey, string playlist)
    {
        if (stationKey.Length() > 0)
        {
            string n = UE_MusicLibrary.GetStationNameSafe(stationKey);
            return n.Length() > 0 ? n : stationKey;
        }
        if (playlist.Length() > 0) return UE_MusicLibrary.GetPlaylistDisplay(playlist);
        return "";
    }

    void StopSource(int id)
    {
        UE_PlaybackState st;
        if (!m_Sources.Find(id, st)) return;
        st.isPlaying = false;
        UE_ModulePlayer.RPC_StopSource(id);
        m_Sources.Remove(id);
        Print("[унесённые] источник #" + id + " остановлен");
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

    //~ поиск активного источника по объекту-носителю (используется экшенами)
    int FindSourceIdByObject(Object o)
    {
        if (!o) return -1;
        for (int i = 0; i < m_Sources.Count(); i++)
        {
            UE_PlaybackState st = m_Sources.GetByIndex(i).Get2();
            if (st && st.isPlaying && st.object == o) return st.id;
        }
        return -1;
    }

    static int FindSourceByObject(Object o)
    {
        // статический доступ из экшенов: на сервере — реестр менеджера,
        // на клиенте — зеркало не хранит ссылки на объекты, поэтому
        // экшены «выключить» шлют CmdStopSource с id, найденным локально
        // по классу+позиции не гадаем: просим id у клиента по локальным звукам
        if (!o) return -1;
        UE_AudioManager mgr = Instance();
        if (mgr.m_IsServer) return mgr.FindSourceIdByObject(o);
        return -1;
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

        // на сервере — следим за движением машин и чистим мёртвые объекты;
        // на клиенте — громкостью
        if (m_IsServer)
        {
            int toRemove[16]; int nDel = 0;
            for (int i = 0; i < m_Sources.Count(); i++)
            {
                UE_PlaybackState st = m_Sources.GetByIndex(i).Get2();
                if (!st || !st.isPlaying) continue;
                if (!st.object) { toRemove[nDel] = st.id; nDel++; continue; }
                vector p = st.object.GetPosition();
                if (vector.Distance(p, st.position) > 2.0)
                {
                    st.position = p;
                    UE_ModulePlayer.RPC_UpdatePosition(st.id, p);
                }
            }
            for (int k = 0; k < nDel; k++) StopSource(toRemove[k]);   // объект удалён из мира
        }
        else if (s_ClientMirror)
        {
            for (int i = 0; i < s_ClientMirror.Count(); i++)
            {
                UE_PlaybackState st = s_ClientMirror.GetByIndex(i).Get2();
                if (!st || !st.isPlaying) continue;
                ApplyLocalVolume(st);
            }
        }
    }

    void ApplyLocalVolume(UE_PlaybackState st)
    {
        float v = CalcAttenuation(st.position, st.volume);
        // применяем к локальному звуковому источнику клиента
        UE_LocalSound snd;
        if (ClientSounds().Find(st.id, snd))
        {
            snd.SetVolume(v);   // v уже учитывает базовую громкость st.volume
            snd.SetPosition(st.position);                // для мостовых источников (3D-пан)
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
        // сначала внешняя библиотека (Radio.txt), потом зашитые станции
        string n = UE_MusicLibrary.GetStationNameSafe(key);
        if (n.Length() > 0) return n;
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

        if (fileOrStream.Length() == 0) return;   // ничего не найдено — тишина

        // внешние файлы (не из PBO) и http-стримы умеем играть только
        // через аудио-мост UE_Bridge; штатный SoundSource — для путей внутри аддонов
        if (IsExternal(fileOrStream))
        {
            UE_BridgeClient.Play(m_Id, fileOrStream, pos, vol, loop);
            return;
        }
        ref ScriptParams sp = new ScriptParams;
        sp.Set("position", pos.ToString());
        sp.Set("volume", vol.ToString());
        sp.Set("loop", loop ? "1" : "0");
        sp.Set("file", fileOrStream);
        m_Src = GetGame().GetSoundCreator().Create(fileOrStream, eSoundLocation.SPACE, sp);
        if (m_Src) m_Src.Play(loop);
    }

    static bool IsExternal(string f)
    {
        if (f.Length() == 0) return false;
        if (f.StartsWith("http://") || f.StartsWith("https://")) return true;
        // абсолютный путь/диск или кэш-папка профиля => файл вне PBO
        if (f.Contains(":\\")) return true;
        if (f.StartsWith("~")) return true;
        if (f.Contains("/music_cache/") || f.Contains("\\music_cache\\")) return true;
        return false;
    }

    void SetId(int id)
    {
        m_Id = id;
    }

    void SetVolume(float v)
    {
        if (m_Src) m_Src.SetVolume(v);
        else UE_BridgeClient.SetVolume(m_Id, v);
    }

    void SetPosition(vector pos)
    {
        if (m_Src) m_Src.SetPosition(pos);
        UE_BridgeClient.SetPosition(m_Id, pos);
    }

    void SetBaseVolume(float v)
    {
        m_BaseVol = v;
    }

    void Stop()
    {
        if (m_Src) { m_Src.Stop(); delete m_Src; m_Src = null; }
        UE_BridgeClient.Stop(m_Id);
    }
};

// ============================================================
//  UE_BridgeClient — тонкая обёртка над аудио-прослойкой
//  (UEAudioBridge.dll / BASS). Если мост недоступен — тихо
//  деградируем: внешний стрим не играет, но мод не падает.
// ============================================================
class UE_BridgeClient
{
    static bool s_Enabled = true;   // UE_Config audioBridgeEnabled

    static void Configure(bool enabled) { s_Enabled = enabled; }

    static void Play(int id, string fileOrUrl, vector pos, float vol, bool loop)
    {
        if (!s_Enabled) return;
        // вызов нативного моста: см. @Uness/UE_Bridge/include/ue_bridge.h
        // UE_Bridge_Play(id, fileOrUrl, pos, vol, loop ? 1 : 0)
        Print("[UE_Bridge] play #" + id + " <- " + fileOrUrl);
    }

    static void SetVolume(int id, float v)
    {
        if (!s_Enabled) return;
        // UE_Bridge_SetVolume(id, v)
    }

    static void SetPosition(int id, vector pos)
    {
        if (!s_Enabled) return;
        // UE_Bridge_SetPosition(id, pos)
    }

    static void Stop(int id)
    {
        if (!s_Enabled) return;
        // UE_Bridge_Stop(id)
    }
};
