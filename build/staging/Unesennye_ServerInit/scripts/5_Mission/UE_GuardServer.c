// ============================================================
//  Unesennye_ServerInit — серверный аддон мода «унесённые».
//  ОБЯЗАТЕЛЕН для работы клиентского @Unesennye.
//
//  Защита мода (почему сторонний сервер без него крашится):
//   1. requiredAddons в Unesennye_Data/Unesennye_Scripts ссылается
//      на Unesennye_ServerInit — без него мод не загрузится;
//   2. при старте миссии этот скрипт рассылает всем клиентам
//      RPC-handshake "UE_Guard.OnServerHandshake";
//   3. клиентский UE_Guard ждёт handshake grace-период и при его
//      отсутствии аварийно останавливает сервер (TriggerShutdown).
// ============================================================

modded class MissionServer
{
    void SendGuardHandshake()
    {
        Param1<string> p = new Param1<string>("UNESSED-HS-V1-KRaTos");
        GetRPCManager().SendRPC("UE_Guard", "OnServerHandshake", p, true, null);
        Print("[UnesennyeServer] guard handshake разослан клиентам.");
    }

    override void OnInit()
    {
        super.OnInit();
        Print("[UnesennyeServer] серверный аддон загружен, v1.0.0");

        // handshake сразу + повторим через несколько секунд
        // (клиенты, ещё не дошедшие до OnInit, получат JIP-версию)
        SendGuardHandshake();
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(SendGuardHandshake, 5000, false);
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(SendGuardHandshake, 15000, false);
    }

    override void OnPlayerConnect(PlayerBase player)
    {
        super.OnPlayerConnect(player);
        // персональный повтор handshake для только что подключившегося
        PlayerIdentity ident = player.GetIdentity();
        if (ident)
        {
            Param1<string> p = new Param1<string>("UNESSED-HS-V1-KRaTos");
            GetRPCManager().SendRPC("UE_Guard", "OnServerHandshake", p, false, ident);
        }
    }
};
