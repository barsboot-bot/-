// ============================================================
//  UE_Network — сетевой слой мода «унесённые»
//  RPC-обёртки над движковым RemoteExec. Все команды идут
//  через серверную валидацию (UE_Security) и имеют подпись.
// ============================================================

class UE_ModulePlayer: ScriptModule
{
    // вызывается на сервере, рассылает всем клиентам команду создания источника
    static void RPC_CreateSource(int id, int type, string stationKey, string playlist, vector pos, float vol, float startedAt)
    {
        if (!GetGame().IsDedicated()) return;
        Param4 p = new Param4<int, int, string, vector>(id, type, stationKey + "|" + playlist, pos);
        GetRPCManager().SendRPC("UE_Network", "OnClientCreateSource", p, true, null);
        Param2 q = new Param2<float, float>(vol, startedAt);
        GetRPCManager().SendRPC("UE_Network", "OnClientCreateSourceEx", q, true, null);
    }

    //~ --- рассылка манифеста музыкальной библиотеки одному клиенту ---
    static void RPC_SendManifest(PlayerIdentity ident)
    {
        if (!GetGame().IsDedicated()) return;
        string m = UE_MusicLibrary.s_Manifest;
        Param1 begin = new Param1<string>("B:" + UE_MusicLibrary.ManifestChunks() + ":" + UE_MusicLibrary.ManifestChunk(0));
        GetRPCManager().SendRPC("UE_Network", "OnLibraryManifest", begin, true, ident);
        for (int i = 1; i < UE_MusicLibrary.ManifestChunks(); i++)
        {
            Param1 ch = new Param1<string>("C:" + UE_MusicLibrary.ManifestChunk(i));
            GetRPCManager().SendRPC("UE_Network", "OnLibraryManifest", ch, true, ident);
        }
        Param1 endp = new Param1<string>("E");
        GetRPCManager().SendRPC("UE_Network", "OnLibraryManifest", endp, true, ident);
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
        GetRPCManager().AddRPC("UE_Network", "OnClientCreateSourceEx", this, FunccType.serverbc);
        GetRPCManager().AddRPC("UE_Network", "OnClientStopSource", this, FunccType.serverbc);
        GetRPCManager().AddRPC("UE_Network", "OnClientUpdatePos", this, FunccType.serverbc);
        GetRPCManager().AddRPC("UE_Network", "OnLibraryManifest", this, FunccType.serverown);
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

        UE_LocalSound snd = new UE_LocalSound;
        string file = ResolveSoundFile(type_, stationOrPlaylist, extra);
        snd.Play(file, pos, 1.0, true);
        snd.SetId(id);
        UE_AudioManager.ClientSounds().Set(id, snd);

        // держим локальную копию состояния: тик пересчитает громкость
        UE_PlaybackState st = new UE_PlaybackState;
        st.id = id;
        st.type = type_;
        st.position = pos;
        st.volume = 1.0;
        st.isPlaying = true;
        st.stationKey = stationOrPlaylist;
        st.playlist = extra;
        if (!UE_AudioManager.s_ClientMirror) UE_AudioManager.s_ClientMirror = new map<int, ref UE_PlaybackState>;
        UE_AudioManager.s_ClientMirror.Set(id, st);
    }

    // второй пакет — громкость/время старта (синхронизация трека)
    void OnClientCreateSourceEx(CallType type, ref ParamsReadContext ctx, ref PlayerIdentity sender, ref Object target)
    {
        if (GetGame().IsDedicated()) return;
        Param2<float, float> data;
        if (!ctx.Read(data)) return;
        // базовая громкость и startedAt применяются менеджером при следующем тике
        UE_AudioManager.Instance().SetLastCreateParams(data.param1, data.param2);
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
        Param2<Object, string> data; if (!ctx.Read(data)) return;
        PlayerBase pl = PlayerBase.Cast(GetGame().GetPlayerByID(sender.GetId()));
        if (!UE_Security.Instance().ValidateCommand(pl, data.param1, 1)) return;
        if (!UE_Security.Instance().ValidatePlaylistKey(data.param2, "CD", sender.GetName())) return;
        UE_AudioManager.Instance().CreateSource(data.param1, UE_SourceType.DISK, "", data.param2, 1.0);
    }

    void CmdPlayRadio(CallType type, ref ParamsReadContext ctx, ref PlayerIdentity sender, ref Object target)
    {
        if (!GetGame().IsDedicated()) return;
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
        Param1<int> data; if (!ctx.Read(data)) return;
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
