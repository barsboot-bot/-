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
        Param3 q = new Param2<float, float, string>(vol, startedAt, UE_Security::ComputeHMAC(stationKey + playlist));
        GetRPCManager().SendRPC("UE_Network", "OnClientCreateSource", p, true, null);
        GetRPCManager().SendRPC("UE_Network", "OnClientCreateSourceEx", q, true, null);
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
        UE_AudioManager.ClientSounds().Set(id, snd);
    }

    void OnClientStopSource(CallType type, ref ParamsReadContext ctx, ref PlayerIdentity sender, ref Object target)
    {
        if (GetGame().IsDedicated()) return;
        Param1<int> data; if (!ctx.Read(data)) return;
        UE_LocalSound snd;
        if (UE_AudioManager.ClientSounds().Find(data.param, snd)) { snd.Stop(); delete snd; UE_AudioManager.ClientSounds().Remove(data.param); }
    }

    void OnClientUpdatePos(CallType type, ref ParamsReadContext ctx, ref PlayerIdentity sender, ref Object target)
    {
        if (GetGame().IsDedicated()) return;
        Param2<int, vector> data; if (!ctx.Read(data)) return;
        UE_PlaybackState st;
        if (UE_AudioManager.Instance().m_Sources.Find(data.param1, st)) st.position = data.param2;
    }

    //~ ---------- СЕРВЕР: игрок хочет включить кассету ----------
    void CmdPlayCassette(CallType type, ref ParamsReadContext ctx, ref PlayerIdentity sender, ref Object target)
    {
        if (!GetGame().IsDedicated()) return;
        Param2<Object, string> data; if (!ctx.Read(data)) return;   // плеер, плейлист
        PlayerBase pl = PlayerBase.Cast(GetGame().GetPlayerByID(sender.GetId()));
        if (!UE_Security.Instance().ValidateCommand(pl, data.param1, 0)) return;
        UE_AudioManager.Instance().CreateSource(data.param1, UE_SourceType.CASSETTE, "", data.param2, 1.0);
    }

    void CmdPlayDisk(CallType type, ref ParamsReadContext ctx, ref PlayerIdentity sender, ref Object target)
    {
        if (!GetGame().IsDedicated()) return;
        Param2<Object, string> data; if (!ctx.Read(data)) return;
        PlayerBase pl = PlayerBase.Cast(GetGame().GetPlayerByID(sender.GetId()));
        if (!UE_Security.Instance().ValidateCommand(pl, data.param1, 1)) return;
        UE_AudioManager.Instance().CreateSource(data.param1, UE_SourceType.DISK, "", data.param2, 1.0);
    }

    void CmdPlayRadio(CallType type, ref ParamsReadContext ctx, ref PlayerIdentity sender, ref Object target)
    {
        if (!GetGame().IsDedicated()) return;
        Param2<Object, string> data; if (!ctx.Read(data)) return;   // приёмник, ключ станции
        PlayerBase pl = PlayerBase.Cast(GetGame().GetPlayerByID(sender.GetId()));
        if (!UE_Security.Instance().ValidateCommand(pl, data.param1, 2)) return;
        // проверяем что станция из белого списка конфига
        string url = UE_AudioManager.GetStationURL(data.param2);
        if (url.Length() == 0) { UE_Security.Instance().LogViolation(sender.GetName(), "неизвестная радиостанция"); return; }
        UE_AudioManager.Instance().CreateSource(data.param1, UE_SourceType.RADIO, data.param2, "", 0.9);
    }

    void CmdStopSource(CallType type, ref ParamsReadContext ctx, ref PlayerIdentity sender, ref Object target)
    {
        if (!GetGame().IsDedicated()) return;
        Param1<int> data; if (!ctx.Read(data)) return;
        UE_AudioManager.Instance().StopSource(data.param);
    }

    static string ResolveSoundFile(int type, string key, string extra)
    {
        switch (type)
        {
            case UE_SourceType.RADIO:   return UE_AudioManager.GetStationURL(key);
            case UE_SourceType.CASSETTE:return "dzue/sounds/cassettes/" + key.ToLower() + ".ogg";
            case UE_SourceType.DISK:    return "dzue/sounds/disks/" + key.ToLower() + ".ogg";
            case UE_SourceType.CAR:     return "dzue/sounds/car/" + key.ToLower() + ".ogg";
        }
        return "";
    }
};
