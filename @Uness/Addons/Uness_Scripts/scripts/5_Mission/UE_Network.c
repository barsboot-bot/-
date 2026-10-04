// ============================================================
//  UE_Network — сетевой слой мода «унесённые»
//  RPC-обёртки над движковым RemoteExec. Все команды идут
//  через серверную валидацию (UE_Security) и имеют подпись.
// ============================================================

class UE_ModulePlayer: ScriptModule
{
    // вызывается на сервере, рассылает всем клиентам команду создания источника.
    // ВАЖНО: весь пакет — в ОДНОМ RPC (Param4), т.к. два последовательных
    // broadcast пакета могли бы доставляться вразнобой разным клиентам.
    static void RPC_CreateSource(int id, int type, string stationKey, string playlist, vector pos, float vol, float startedAt)
    {
        if (!GetGame().IsDedicated()) return;
        Param4<int, int, string, vector> p = new Param4<int, int, string, vector>(
            id, type, stationKey + "|" + playlist + "|" + vol + "|" + startedAt, pos);
        GetRPCManager().SendRPC("UE_Network", "OnClientCreateSource", p, true, null);
    }

    //~ --- рассылка манифеста музыкальной библиотеки одному клиенту ---
    static void RPC_SendManifest(PlayerIdentity ident)
    {
        if (!GetGame().IsDedicated()) return;
        if (!ident) return;
        string m = UE_MusicLibrary.s_Manifest;
        int chunks = UE_MusicLibrary.ManifestChunks();
        if (chunks == 0) return;   // библиотека пуста — нечего слать
        Param1<string> begin = new Param1<string>("B:" + chunks + ":" + UE_MusicLibrary.ManifestChunk(0));
        GetRPCManager().SendRPC("UE_Network", "OnLibraryManifest", begin, false, ident);
        for (int i = 1; i < chunks; i++)
        {
            Param1<string> ch = new Param1<string>("C:" + UE_MusicLibrary.ManifestChunk(i));
            GetRPCManager().SendRPC("UE_Network", "OnLibraryManifest", ch, false, ident);
        }
        Param1<string> endp = new Param1<string>("E");
        GetRPCManager().SendRPC("UE_Network", "OnLibraryManifest", endp, false, ident);
    }

    static void RPC_StopSource(int id)
    {
        if (!GetGame().IsDedicated()) return;
        Param1<int> p = new Param1<int>(id);
        GetRPCManager().SendRPC("UE_Network", "OnClientStopSource", p, true, null);
    }

    static void RPC_UpdatePosition(int id, vector pos)
    {
        if (!GetGame().IsDedicated()) return;
        Param2<int, vector> p = new Param2<int, vector>(id, pos);
        GetRPCManager().SendRPC("UE_Network", "OnClientUpdatePos", p, true, null);
    }
};

class UE_NetworkHandler: ModuleBase
{
    void Register()
    {
        // клиентские обработчики
        GetRPCManager().AddRPC("UE_Network", "OnClientCreateSource", this, FunccType.serverbc);
        GetRPCManager().AddRPC("UE_Network", "OnClientStopSource", this, FunccType.serverbc);
        GetRPCManager().AddRPC("UE_Network", "OnClientUpdatePos", this, FunccType.serverbc);
        GetRPCManager().AddRPC("UE_Network", "OnLibraryManifest", this, FunccType.serverown);
        // handshake серверного аддона (@KRa_TosServer) — защита мода
        GetRPCManager().AddRPC("UE_Guard", "OnServerHandshake", this, FunccType.serverbc);
        // серверные обработчики входящих запросов от игроков
        GetRPCManager().AddRPC("UE_Network", "CmdPlayCassette", this, FunccType.clientown);
        GetRPCManager().AddRPC("UE_Network", "CmdPlayDisk", this, FunccType.clientown);
        GetRPCManager().AddRPC("UE_Network", "CmdPlayRadio", this, FunccType.clientown);
        GetRPCManager().AddRPC("UE_Network", "CmdStopSource", this, FunccType.clientown);
    }

    //~ ---------- КЛИЕНТ: создать локальный звук ----------
    void OnClientCreateSource(CallType type, ref ParamsReadContext ctx, ref PlayerIdentity sender, ref Object target)
    {
        if (GetGame().IsDedicated()) return;
        Param4<int, int, string, vector> data;
        if (!ctx.Read(data)) return;
        int id = data.param1; int type_ = data.param2; string meta = data.param3; vector pos = data.param4;

        array<string> parts = {}; meta.Split("|", parts);
        string stationOrPlaylist = parts.Get(0);
        string extra = parts.Count() > 1 ? parts.Get(1) : "";
        float vol = parts.Count() > 2 ? parts.Get(2).ToFloat() : 1.0;   // базовая громкость с сервера

        UE_LocalSound snd = new UE_LocalSound;
        string file = ResolveSoundFile(type_, stationOrPlaylist, extra);
        snd.Play(file, pos, vol, true);
        snd.SetId(id);
        UE_AudioManager.ClientSounds().Set(id, snd);

        // держим локальную копию состояния: тик пересчитает громкость
        UE_PlaybackState st = new UE_PlaybackState;
        st.id = id;
        st.type = type_;
        st.position = pos;
        st.volume = vol;
        st.isPlaying = true;
        st.stationKey = stationOrPlaylist;
        st.playlist = extra;
        if (!UE_AudioManager.s_ClientMirror) UE_AudioManager.s_ClientMirror = new map<int, ref UE_PlaybackState>;
        UE_AudioManager.s_ClientMirror.Set(id, st);

        // если трек ещё не скачан — докачка асинхронная; после завершения
        // (OnDownloadFinished) перезапустим звук уже из локального файла
        UE_DownloadWatcher.Watch(id, type_, stationOrPlaylist, extra, pos, vol);
    }

    //~ ---------- КЛИЕНТ: манифест музыкальной библиотеки (чанками) ----------
    void OnLibraryManifest(CallType type, ref ParamsReadContext ctx, ref PlayerIdentity sender, ref Object target)
    {
        if (GetGame().IsDedicated()) return;
        Param1<string> data; if (!ctx.Read(data)) return;
        string pkt = data.param;
        if (pkt.StartsWith("B:"))
        {
            int sep = pkt.IndexOf(":", 2);
            UE_MusicLibrary.ClientOnManifestBegin(pkt.SubstringWithLimit(sep + 1, pkt.Length()));
        }
        else if (pkt.StartsWith("C:"))
        {
            UE_MusicLibrary.s_Manifest.Append(pkt.SubstringWithLimit(2, pkt.Length()));
        }
        else if (pkt == "E")
        {
            UE_MusicLibrary.ClientOnManifestEnd();
        }
    }

    void OnClientStopSource(CallType type, ref ParamsReadContext ctx, ref PlayerIdentity sender, ref Object target)
    {
        if (GetGame().IsDedicated()) return;
        Param1<int> data; if (!ctx.Read(data)) return;
        UE_LocalSound snd;
        if (UE_AudioManager.ClientSounds().Find(data.param, snd)) { snd.Stop(); delete snd; UE_AudioManager.ClientSounds().Remove(data.param); }
        if (UE_AudioManager.s_ClientMirror) UE_AudioManager.s_ClientMirror.Remove(data.param);
    }

    void OnClientUpdatePos(CallType type, ref ParamsReadContext ctx, ref PlayerIdentity sender, ref Object target)
    {
        if (GetGame().IsDedicated()) return;
        Param2<int, vector> data; if (!ctx.Read(data)) return;
        UE_PlaybackState st;
        if (UE_AudioManager.s_ClientMirror && UE_AudioManager.s_ClientMirror.Find(data.param1, st))
            st.position = data.param2;
    }

    //~ ---------- СЕРВЕР: игрок хочет включить кассету ----------
    void CmdPlayCassette(CallType type, ref ParamsReadContext ctx, ref PlayerIdentity sender, ref Object target)
    {
        if (!GetGame().IsDedicated()) return;
        if (!sender) return;
        Param2<Object, string> data; if (!ctx.Read(data)) return;   // плеер, плейлист
        PlayerBase pl = PlayerBase.Cast(GetGame().GetPlayerByID(sender.GetId()));
        if (!UE_Security.Instance().ValidateCommand(pl, data.param1, 0)) return;
        // ключ должен существовать в библиотеке сервера (или быть legacy-плейлистом)
        if (!UE_Security.Instance().ValidatePlaylistKey(data.param2, "Type", sender.GetName())) return;
        UE_AudioManager.Instance().CreateSource(data.param1, UE_SourceType.CASSETTE, "", data.param2, 1.0);
    }

    void CmdPlayDisk(CallType type, ref ParamsReadContext ctx, ref PlayerIdentity sender, ref Object target)
    {
        if (!GetGame().IsDedicated()) return;
        if (!sender) return;
        Param2<Object, string> data; if (!ctx.Read(data)) return;
        PlayerBase pl = PlayerBase.Cast(GetGame().GetPlayerByID(sender.GetId()));
        if (!UE_Security.Instance().ValidateCommand(pl, data.param1, 1)) return;
        if (!UE_Security.Instance().ValidatePlaylistKey(data.param2, "CD", sender.GetName())) return;
        UE_AudioManager.Instance().CreateSource(data.param1, UE_SourceType.DISK, "", data.param2, 1.0);
    }

    void CmdPlayRadio(CallType type, ref ParamsReadContext ctx, ref PlayerIdentity sender, ref Object target)
    {
        if (!GetGame().IsDedicated()) return;
        if (!sender) return;
        Param2<Object, string> data; if (!ctx.Read(data)) return;   // приёмник, ключ станции
        PlayerBase pl = PlayerBase.Cast(GetGame().GetPlayerByID(sender.GetId()));
        if (!UE_Security.Instance().ValidateCommand(pl, data.param1, 2)) return;
        // станция должна быть в белом списке: config.cpp ИЛИ Music/Radio.txt
        if (!UE_MusicLibrary.HasStation(data.param2))
        {
            UE_Security.Instance().LogViolation(sender.GetName(), "неизвестная радиостанция: " + data.param2);
            return;
        }
        UE_AudioManager.Instance().CreateSource(data.param1, UE_SourceType.RADIO, data.param2, "", 0.9);
    }

    void CmdStopSource(CallType type, ref ParamsReadContext ctx, ref PlayerIdentity sender, ref Object target)
    {
        if (!GetGame().IsDedicated()) return;
        if (!sender) return;
        Param1<int> data; if (!ctx.Read(data)) return;
        // стоп разрешён только тем же правилам доступа, что и старт:
        // найдём объект-носитель источника и проверим игрока
        UE_PlaybackState st;
        if (!UE_AudioManager.Instance().m_Sources.Find(data.param, st)) return;
        PlayerBase pl = PlayerBase.Cast(GetGame().GetPlayerByID(sender.GetId()));
        if (!UE_Security.Instance().ValidateCommand(pl, st.object, st.type)) return;
        UE_AudioManager.Instance().StopSource(data.param);
    }

    //~ ----------------------------------------------------------
    //~  КЛИЕНТ: во что превращается ключ источника.
    //~  Приоритет: внешняя библиотека Music/ (кэш/докачка) ->
    //~  fallback на звуки из PBO (старые ключи Rock/Pop/...).
    //~ ----------------------------------------------------------
    static string ResolveSoundFile(int type, string key, string extra)
    {
        switch (type)
        {
            case UE_SourceType.RADIO:
            {
                string url = UE_MusicLibrary.ClientStationUrl(key);          // Radio.txt / манифест
                if (url.Length() == 0) url = UE_AudioManager.GetStationURL(key); // белый список config.cpp
                return url;
            }
            case UE_SourceType.CASSETTE:
            case UE_SourceType.DISK:
            case UE_SourceType.CAR:
            {
                // 1) внешний файл из папки Music (.Type/CD), скачанный с зеркала
                string ext = (type == UE_SourceType.CASSETTE) ? "Type" : ((type == UE_SourceType.DISK) ? "CD" : "Type");
                string libKey = MakeLibKey(ext, key);
                string local = UE_MusicLibrary.ResolveLocalFile(libKey);
                if (local) return local;
                // 2) legacy — упакованные в PBO треки
                return LegacyPboPath(type, key);
            }
        }
        return "";
    }

    static string MakeLibKey(string dirName, string keyOrPlaylist)
    {
        // уже полный ключ вида "Type/MyMix"?
        if (keyOrPlaylist.StartsWith("Type/") || keyOrPlaylist.StartsWith("CD/"))
            return keyOrPlaylist;
        // legacy имя плейлиста ("Rock") -> папка Music/<dir>/Rock
        return dirName + "/" + keyOrPlaylist;
    }

    static string LegacyPboPath(int type, string playlist)
    {
        string p = playlist.ToLower();
        int slash = p.IndexOf("/");
        if (slash >= 0) p = p.SubstringWithLimit(slash + 1, p.Length());   // "Type/rock" -> "rock"
        switch (type)
        {
            case UE_SourceType.CASSETTE:return "dzue/sounds/cassettes/" + p + ".ogg";
            case UE_SourceType.DISK:    return "dzue/sounds/disks/" + p + ".ogg";
            case UE_SourceType.CAR:     return "dzue/sounds/car/" + p + ".ogg";
        }
        return "";
    }
};

// ============================================================
//  UE_DownloadWatcher — клиент: перезапуск звука после того,
//  как движок завершил докачку трека с HTTP-зеркала библиотеки.
//  (DayZ Game API: OnDownloadFinished вызывается на клиенте.)
// ============================================================
class UE_PendingDownload
{
    int srcId;
    int type;
    string key;      // станция/плейлист из RPC
    string extra;
    vector pos;
    float vol;
};

class UE_DownloadWatcher: ScriptCallbackBase
{
    static ref map<string, ref UE_PendingDownload> s_Waiting;  // локальный путь -> ждущий источник

    static void Watch(int srcId, int type, string stationKey, string playlist, vector pos, float vol)
    {
        if (!s_Waiting) s_Waiting = new map<string, ref UE_PendingDownload>;
        if (type == UE_SourceType.RADIO) return;   // стрим качать не нужно
        // ключ библиотеки для плейлиста: "<dir>/<folder>" (Type/Rock, CD/Dance...)
        string libKey = ResolveLibKey(type, stationKey, playlist);
        if (libKey.Length() == 0) return;
        // если файл уже в кэше — ничего не делаем (звук уже играет им)
        if (UE_MusicLibrary.HasCachedFile(libKey)) return;
        // регистрируем ожидание по каждому возможному расширению
        string probe = UE_MusicLibrary.CacheDir() + libKey;
        Register(probe + ".ogg", srcId, type, stationKey, playlist, pos, vol);
        Register(probe + ".mp3", srcId, type, stationKey, playlist, pos, vol);
        Register(probe + ".wav", srcId, type, stationKey, playlist, pos, vol);
        // инициируем докачку (асинхронно, без блокировки тика)
        UE_MusicLibrary.TryDownloadAsync(libKey);
    }

    //~ соответствие legacy-плейлиста ("Rock"/"Pop") -> ключ внешней библиотеки
    static string ResolveLibKey(int type, string stationKey, string playlist)
    {
        string dir = (type == UE_SourceType.DISK) ? "CD" : "Type";
        string name = (playlist.Length() > 0) ? playlist : stationKey;
        if (name.Length() == 0) return "";
        // точное совпадение с зарегистрированным ключом
        string full = dir + "/" + name;
        if (UE_MusicLibrary.ClientHasPlaylist(full)) return full;
        return "";
    }

    static void Register(string localPath, int srcId, int type, string key, string extra, vector pos, float vol)
    {
        UE_PendingDownload pd = new UE_PendingDownload;
        pd.srcId = srcId; pd.type = type; pd.key = key; pd.extra = extra; pd.pos = pos; pd.vol = vol;
        s_Waiting.Set(localPath.ToLower(), pd);
    }

    override void OnDownloadFinished(string arg, CallReturnCodes return_code, uint data)
    {
        if (!s_Waiting) return;
        string probe = arg.ToLower();
        UE_PendingDownload pd;
        if (!s_Waiting.Find(probe, pd)) return;
        // убираем все записи этого источника (другие расширения больше не нужны)
        for (int k = s_Waiting.Count() - 1; k >= 0; k--)
        {
            if (s_Waiting.GetByIndex(k).Get2().srcId == pd.srcId)
                s_Waiting.Remove(s_Waiting.GetByIndex(k).Get1());
        }
        if (return_code != CallReturnCodes.PROCESS_DONE) return;
        // файл скачан — перезапускаем локальный звук этого источника
        UE_LocalSound snd;
        if (UE_AudioManager.ClientSounds().Find(pd.srcId, snd))
        {
            snd.Stop();
            snd.Play(arg, pd.pos, pd.vol, true);
            Print("[унесённые] трек докачан, возобновлено воспроизведение #" + pd.srcId);
        }
    }
};
