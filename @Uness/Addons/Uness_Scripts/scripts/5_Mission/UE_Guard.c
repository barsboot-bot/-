// ============================================================
//  UE_Guard — техническая защита мода «унесённые»
//  Сторонние серверы не могут использовать мод без обязательного
//  серверного аддона @KRa_TosServer (KRa_TosServerInit).
//
//  Двухуровневая защита:
//   УРОВЕНЬ 1 (config.cpp, надёжный): Uness_Data и
//     Uness_Scripts объявляют requiredAddons[] = {...,
//     "KRa_TosServerInit"}. Без @KRa_TosServer движок не
//     может загрузить клиентский мод -> миссия не стартует.
//   УРОВЕНЬ 2 (скриптовый handshake): серверный аддон при старте
//     шлёт всем клиентам RPC-приветствие с контрольной строкой.
//     Клиентская часть проверяет его получение; если приветствия
//     нет в течение grace-периода (UE_Config::ueGraceSeconds) —
//     сервер аварийно останавливается (TriggerShutdown).
// ============================================================

class UE_Guard: ScriptModule
{
    static const string HANDSHAKE_TAG = "UNESSED-HS-V1-KRaTos"; // подпись handshake

    bool m_HandshakeOK;        // получено ли приветствие от серверного аддона
    float m_Deadline;          // ReTime(), после которого считаем провал

    void UE_Guard()
    {
        m_HandshakeOK  = false;
        m_Deadline     = 0;
    }

    //~ ставит guard на боевой взвод (grace-период из UE_Config)
    void Arm()
    {
        int grace = 30;
        if (!GetGame().ConfigGetInt("UE_Config ueGraceSeconds", grace)) grace = 30;
        if (grace <= 0) grace = 30;
        m_Deadline = ReTime() + grace;
        Print("[унесённые][guard] ожидание handshake серверного мода, grace=" + grace + "s");
    }

    //~ вызывается на клиенте при получении приветствия
    void OnHandshake(string tag)
    {
        if (tag == HANDSHAKE_TAG)
        {
            m_HandshakeOK = true;
            Print("[унесённые][guard] handshake серверного мода получен — защита активна.");
        }
    }

    bool IsVerified()
    {
        return m_HandshakeOK;
    }

    static UE_Guard Instance()
    {
        static UE_Guard s;
        if (!s) s = new UE_Guard;
        return s;
    }

    //~ периодическая проверка: переармируется пока не подтверждено;
    //~ по истечении срока — остановка сервера (нарушение лицензии)
    void Check()
    {
        if (m_HandshakeOK) return;                 // всё в порядке
        if (ReTime() < m_Deadline)
        {
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(DoCheck, 2000, false);
            return;                                // ещё в grace-периоде
        }
        Print("[унесённые][guard] TIMEOUT: handshake не получен.");
        TriggerShutdown();
    }

    static void DoCheck()
    {
        UE_Guard.Instance().Check();
    }

    //~ ----------------------------------------------------------
    //~  Принудительная остановка сервера при нарушении защиты.
    //~  Реализована через разыменование заведомо нулевой ссылки:
    //~  Enforce фиксирует критическую ошибку скрипта и процесс
    //~  сервера завершается (стандартный приём anti-piracy).
    //~  Для мягкой версии поставьте ueGraceSeconds = -1 (тогда
    //~  выполняется EndMission вместо краша).
    //~ ----------------------------------------------------------
    void TriggerShutdown()
    {
        Print("[унесённые][FATAL] Серверный мод @KRa_TosServer не найден!");
        Print("[унесённые][FATAL] Использование мода на сторонних серверах запрещено (см. LICENSE).");
        Error("UNESSED SECURITY: server mod validation failed, halting.");
        int grace = 30;
        GetGame().ConfigGetInt("UE_Config ueGraceSeconds", grace);
        if (grace < 0)
        {
            GetGame().EndMission();                // graceful shutdown
            return;
        }
        Object o = null;
        o.Invoke("DoCrash");                       // intentional crash — null deref
    }
};

// ---------- клиентская сторона: ждём handshake ----------
modded class MissionClient
{
    override void OnInit()
    {
        super.OnInit();
        UE_Guard.Instance().Arm();
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(UE_Guard.DoCheck, 5000, false);
    }
};

// ---------- RPC обработчик handshake (клиент) ----------
// Обработчик зарегистрирован в UE_NetworkHandler.Register()
// (AddRPC "UE_Guard" -> "OnServerHandshake", FunccType.serverbc).
modded class UE_NetworkHandler
{
    void OnServerHandshake(CallType type, ref ParamsReadContext ctx, ref PlayerIdentity sender, ref Object target)
    {
        if (GetGame().IsDedicated()) return;       // только на клиенте
        Param1<string> data;
        if (!ctx.Read(data)) return;
        UE_Guard.Instance().OnHandshake(data.param1);
    }
};

