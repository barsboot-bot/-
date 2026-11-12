// ============================================================
//  MR_Network — сетевой слой мода MusicRadio
//  RPC-обёртки над движковым RemoteExec. Все команды идут
//  через серверную валидацию (MR_Security) и имеют подпись.
// ============================================================

class MR_ModulePlayer: ScriptModule
{
    // вызывается на сервере, рассылает всем клиентам команду создания источника.
    // ВАЖНО: весь пакет — в ОДНОМ RPC (Param4), т.к. два последовательных
    // broadcast пакета могли бы доставляться вразнобой разным клиентам.
    static void RPC_CreateSource(int id, int type, string stationKey, string playlist, vector pos, float vol, float startedAt)
    {
        if (!GetGame().IsDedicated()) return;
        Param4<int, int, string, vector> p = new Param4<int, int, string, vector>(
            id, type, stationKey + "|" + playlist + "|" + vol + "|" + startedAt, pos);
        GetRPCManager().SendRPC("MR_Network", "OnClientCreateSource", p, true, null);
    }

    //~ --- рассылка манифеста музыкальной библиотеки одному клиенту ---
    static void RPC_SendManifest(PlayerIdentity ident)
    {
        if (!GetGame().IsDedicated()) return;
        if (!ident) return;
        string m = MR_MusicLibrary.s_Manifest;
        int chunks = MR_MusicLibrary.ManifestChunks();
        if (chunks == 0) return;   // библиотека пуста — нечего слать
        Param1<string> begin = new Param1<string>("B:" + chunks + ":" + MR_MusicLibrary.ManifestChunk(0));
        GetRPCManager().SendRPC("MR_Network", "OnLibraryManifest", begin, false, ident);
        for (int i = 1; i < chunks; i++)
        {
            Param1<string> ch = new Param1<string>("C:" + MR_MusicLibrary.ManifestChunk(i));
            GetRPCManager().SendRPC("MR_Network", "OnLibraryManifest", ch, false, ident);
        }
        Param1<string> endp = new Param1<string>("E");
        GetRPCManager().SendRPC("MR_Network", "OnLibraryManifest", endp, false, ident);
    }

    static void RPC_StopSource(int id)
    {
        if (!GetGame().IsDedicated()) return;
        Param1<int> p = new Param1<int>(id);
        GetRPCManager().SendRPC("MR_Network", "OnClientStopSource", p, true, null);
    }

    static void RPC_UpdatePosition(int id, vector pos)
    {
        if (!GetGame().IsDedicated()) return;
        Param2<int, vector> p = new Param2<int, vector>(id, pos);
        GetRPCManager().SendRPC("MR_Network", "OnClientUpdatePos", p, true, null);
    }
};

class MR_NetworkHandler: ModuleBase
{
    void Register()
    {
        // клиентские обработчики
        GetRPCManager().AddRPC("MR_Network", "OnClientCreateSource", this, FunccType.serverbc);
        GetRPCManager().AddRPC("MR_Network", "OnClientStopSource", this, FunccType.serverbc);
        GetRPCManager().AddRPC("MR_Network", "OnClientUpdatePos", this, FunccType.serverbc);
        GetRPCManager().AddRPC("MR_Network", "OnLibraryManifest", this, FunccType.serverown);
        // handshake серверного аддона (@MusicRadioServer) — защита мода
        GetRPCManager().AddRPC("MR_Guard", "OnServerHandshake", this, FunccType.serverbc);
        // серверные обработчики входящих запросов от игроков
        GetRPCManager().AddRPC("MR_Network", "CmdPlayCassette", this, FunccType.clientown);
        GetRPCManager().AddRPC("MR_Network", "CmdPlayDisk", this, FunccType.clientown);
        GetRPCManager().AddRPC("MR_Network", "CmdPlayRadio", this, FunccType.clientown);
        GetRPCManager().AddRPC("MR_Network", "CmdStopSource", this, FunccType.clientown);
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

        MR_LocalSound snd = new MR_LocalSound;
        string file = ResolveSoundFile(type_, stationOrPlaylist, extra);
        snd.Play(file, pos, vol, true);
        snd.SetId(id);
        MR_AudioManager.ClientSounds().Set(id, snd);

        // держим локальную копию состояния: тик пересчитает громкость
        MR_PlaybackState st = new MR_PlaybackState;
        st.id = id;
        st.type = type_;
        st.position = pos;
        st.volume = vol;
        st.isPlaying = true;
        st.stationKey = stationOrPlaylist;
        st.playlist = extra;
        if (!MR_AudioManager.s_ClientMirror) MR_AudioManager.s_ClientMirror = new map<int, ref MR_PlaybackState>;
        MR_AudioManager.s_ClientMirror.Set(id, st);

        // если трек ещё не скачан — докачка асинхронная; после завершения
        // (OnDownloadFinished) перезапустим звук уже из локального файла
        MR_DownloadWatcher.Watch(id, type_, stationOrPlaylist, extra, pos, vol);
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
            MR_MusicLibrary.ClientOnManifestBegin(pkt.SubstringWithLimit(sep + 1, pkt.Length()));
        }
        else if (pkt.StartsWith("C:"))
        {
            MR_MusicLibrary.s_Manifest.Append(pkt.SubstringWithLimit(2, pkt.Length()));
        }
        else if (pkt == "E")
        {
            MR_MusicLibrary.ClientOnManifestEnd();
        }
    }

    void OnClientStopSource(CallType type, ref ParamsReadContext ctx, ref PlayerIdentity sender, ref Object target)
    {
        if (GetGame().IsDedicated()) return;
        Param1<int> data; if (!ctx.Read(data)) return;
        MR_LocalSound snd;
        if (MR_AudioManager.ClientSounds().Find(data.param, snd)) { snd.Stop(); delete snd; MR_AudioManager.ClientSounds().Remove(data.param); }
        if (MR_AudioManager.s_ClientMirror) MR_AudioManager.s_ClientMirror.Remove(data.param);
    }

    void OnClientUpdatePos(CallType type, ref ParamsReadContext ctx, ref PlayerIdentity sender, ref Object target)
    {
        if (GetGame().IsDedicated()) return;
        Param2<int, vector> data; if (!ctx.Read(data)) return;
        MR_PlaybackState st;
        if (MR_AudioManager.s_ClientMirror && MR_AudioManager.s_ClientMirror.Find(data.param1, st))
            st.position = data.param2;
    }

    //~ ---------- СЕРВЕР: игрок хочет включить кассету ----------
    void CmdPlayCassette(CallType type, ref ParamsReadContext ctx, ref PlayerIdentity sender, ref Object target)
    {
        if (!GetGame().IsDedicated()) return;
        if (!sender) return;
        Param2<Object, string> data; if (!ctx.Read(data)) return;   // плеер, плейлист
        PlayerBase pl = PlayerBase.Cast(GetGame().GetPlayerByID(sender.GetId()));
        if (!MR_Security.Instance().ValidateCommand(pl, data.param1, 0)) return;
        // ключ должен существовать в библиотеке сервера (или быть legacy-плейлистом)
        if (!MR_Security.Instance().ValidatePlaylistKey(data.param2, "Type", sender.GetName())) return;
        MR_AudioManager.Instance().CreateSource(data.param1, MR_SourceType.CASSETTE, "", data.param2, 1.0);
    }

    void CmdPlayDisk(CallType type, ref ParamsReadContext ctx, ref PlayerIdentity sender, ref Object target)
    {
        if (!GetGame().IsDedicated()) return;
        if (!sender) return;
        Param2<Object, string> data; if (!ctx.Read(data)) return;
        PlayerBase pl = PlayerBase.Cast(GetGame().GetPlayerByID(sender.GetId()));
        if (!MR_Security.Instance().ValidateCommand(pl, data.param1, 1)) return;
        if (!MR_Security.Instance().ValidatePlaylistKey(data.param2, "CD", sender.GetName())) return;
        MR_AudioManager.Instance().CreateSource(data.param1, MR_SourceType.DISK, "", data.param2, 1.0);
    }

    void CmdPlayRadio(CallType type, ref ParamsReadContext ctx, ref PlayerIdentity sender, ref Object target)
    {
        if (!GetGame().IsDedicated()) return;
        if (!sender) return;
        Param2<Object, string> data; if (!ctx.Read(data)) return;   // приёмник, ключ станции
        PlayerBase pl = PlayerBase.Cast(GetGame().GetPlayerByID(sender.GetId()));
        if (!MR_Security.Instance().ValidateCommand(pl, data.param1, 2)) return;
        // станция должна быть в белом списке: config.cpp ИЛИ Music/Radio.txt
        if (!MR_MusicLibrary.HasStation(data.param2))
        {
            MR_Security.Instance().LogViolation(sender.GetName(), "неизвестная радиостанция: " + data.param2);
            return;
        }
        MR_AudioManager.Instance().CreateSource(data.param1, MR_SourceType.RADIO, data.param2, "", 0.9);
    }

    void CmdStopSource(CallType type, ref ParamsReadContext ctx, ref PlayerIdentity sender, ref Object target)
    {
        if (!GetGame().IsDedicated()) return;
        if (!sender) return;
        Param1<int> data; if (!ctx.Read(data)) return;
        // стоп разрешён только тем же правилам доступа, что и старт:
        // найдём объект-носитель источника и проверим игрока
        MR_PlaybackState st;
        if (!MR_AudioManager.Instance().m_Sources.Find(data.param, st)) return;
        PlayerBase pl = PlayerBase.Cast(GetGame().GetPlayerByID(sender.GetId()));
        if (!MR_Security.Instance().ValidateCommand(pl, st.object, st.type)) return;
        MR_AudioManager.Instance().StopSource(data.param);
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
            case MR_SourceType.RADIO:
            {
                string url = MR_MusicLibrary.ClientStationUrl(key);          // Radio.txt / манифест
                if (url.Length() == 0) url = MR_AudioManager.GetStationURL(key); // белый список config.cpp
                return url;
            }
            case MR_SourceType.CASSETTE:
            case MR_SourceType.DISK:
            case MR_SourceType.CAR:
            {
                // 1) внешний файл из папки Music (.Type/CD), скачанный с зеркала
                string ext = (type == MR_SourceType.CASSETTE) ? "Type" : ((type == MR_SourceType.DISK) ? "CD" : "Type");
                string libKey = MakeLibKey(ext, key);
                string local = MR_MusicLibrary.ResolveLocalFile(libKey);
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
            case MR_SourceType.CASSETTE:return "dzue/sounds/cassettes/" + p + ".ogg";
            case MR_SourceType.DISK:    return "dzue/sounds/disks/" + p + ".ogg";
            case MR_SourceType.CAR:     return "dzue/sounds/car/" + p + ".ogg";
        }
        return "";
    }
};

// ============================================================
//  MR_DownloadWatcher — клиент: перезапуск звука после того,
//  как движок завершил докачку трека с HTTP-зеркала библиотеки.
//  (DayZ Game API: OnDownloadFinished вызывается на клиенте.)
// ============================================================
class MR_PendingDownload
{
    int srcId;
    int type;
    string key;      // станция/плейлист из RPC
    string extra;
    vector pos;
    float vol;
};

class MR_DownloadWatcher: ScriptCallbackBase
{
    static ref map<string, ref MR_PendingDownload> s_Waiting;  // локальный путь -> ждущий источник

    static void Watch(int srcId, int type, string stationKey, string playlist, vector pos, float vol)
    {
        if (!s_Waiting) s_Waiting = new map<string, ref MR_PendingDownload>;
        if (type == MR_SourceType.RADIO) return;   // стрим качать не нужно
        // ключ библиотеки для плейлиста: "<dir>/<folder>" (Type/Rock, CD/Dance...)
        string libKey = ResolveLibKey(type, stationKey, playlist);
        if (libKey.Length() == 0) return;
        // если файл уже в кэше — ничего не делаем (звук уже играет им)
        if (MR_MusicLibrary.HasCachedFile(libKey)) return;
        // регистрируем ожидание по каждому возможному расширению
        string probe = MR_MusicLibrary.CacheDir() + libKey;
        Register(probe + ".ogg", srcId, type, stationKey, playlist, pos, vol);
        Register(probe + ".mp3", srcId, type, stationKey, playlist, pos, vol);
        Register(probe + ".wav", srcId, type, stationKey, playlist, pos, vol);
        // инициируем докачку (асинхронно, без блокировки тика)
        MR_MusicLibrary.TryDownloadAsync(libKey);
    }

    //~ соответствие legacy-плейлиста ("Rock"/"Pop") -> ключ внешней библиотеки
    static string ResolveLibKey(int type, string stationKey, string playlist)
    {
        string dir = (type == MR_SourceType.DISK) ? "CD" : "Type";
        string name = (playlist.Length() > 0) ? playlist : stationKey;
        if (name.Length() == 0) return "";
        // точное совпадение с зарегистрированным ключом
        string full = dir + "/" + name;
        if (MR_MusicLibrary.ClientHasPlaylist(full)) return full;
        return "";
    }

    static void Register(string localPath, int srcId, int type, string key, string extra, vector pos, float vol)
    {
        MR_PendingDownload pd = new MR_PendingDownload;
        pd.srcId = srcId; pd.type = type; pd.key = key; pd.extra = extra; pd.pos = pos; pd.vol = vol;
        s_Waiting.Set(localPath.ToLower(), pd);
    }

    override void OnDownloadFinished(string arg, CallReturnCodes return_code, uint data)
    {
        if (!s_Waiting) return;
        string probe = arg.ToLower();
        MR_PendingDownload pd;
        if (!s_Waiting.Find(probe, pd)) return;
        // убираем все записи этого источника (другие расширения больше не нужны)
        for (int k = s_Waiting.Count() - 1; k >= 0; k--)
        {
            if (s_Waiting.GetByIndex(k).Get2().srcId == pd.srcId)
                s_Waiting.Remove(s_Waiting.GetByIndex(k).Get1());
        }
        if (return_code != CallReturnCodes.PROCESS_DONE) return;
        // файл скачан — перезапускаем локальный звук этого источника
        MR_LocalSound snd;
        if (MR_AudioManager.ClientSounds().Find(pd.srcId, snd))
        {
            snd.Stop();
            snd.Play(arg, pd.pos, pd.vol, true);
            Print("[MusicRadio] трек докачан, возобновлено воспроизведение #" + pd.srcId);
        }
    }
};
