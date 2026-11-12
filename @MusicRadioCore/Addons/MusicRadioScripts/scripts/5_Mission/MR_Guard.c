// ============================================================
//  MR_Guard — техническая защита мода MusicRadio
//  Сторонние серверы не могут использовать мод без обязательного
//  серверного аддона @MusicRadioServer (MusicRadioServerInit).
//
//  Двухуровневая защита:
//   УРОВЕНЬ 1 (config.cpp, надёжный): MusicRadioCore и
//     MusicRadioScripts объявляют requiredAddons[] = {...,
//     "MusicRadioServerInit"}. Без @MusicRadioServer движок не
//     может загрузить клиентский мод -> миссия не стартует.
//   УРОВЕНЬ 2 (скриптовый handshake): серверный аддон при старте
//     шлёт всем клиентам RPC-приветствие с контрольной строкой.
//     Клиентская часть проверяет его получение; если приветствия
//     нет в течение grace-периода (MR_Config::mrGraceSeconds) —
//     сервер аварийно останавливается (TriggerShutdown).
// ============================================================

class MR_Guard: ScriptModule
{
    static const string HANDSHAKE_TAG = "UNESSED-HS-V1-KRaTos"; // подпись handshake

    bool m_HandshakeOK;        // получено ли приветствие от серверного аддона
    float m_Deadline;          // ReTime(), после которого считаем провал

    void MR_Guard()
    {
        m_HandshakeOK  = false;
        m_Deadline     = 0;
    }

    //~ ставит guard на боевой взвод (grace-период из MR_Config)
    void Arm()
    {
        int grace = 30;
        if (!GetGame().ConfigGetInt("MR_Config mrGraceSeconds", grace)) grace = 30;
        if (grace <= 0) grace = 30;
        m_Deadline = ReTime() + grace;
        Print("[MusicRadio][guard] ожидание handshake серверного мода, grace=" + grace + "s");
    }

    //~ вызывается на клиенте при получении приветствия
    void OnHandshake(string tag)
    {
        if (tag == HANDSHAKE_TAG)
        {
            m_HandshakeOK = true;
            Print("[MusicRadio][guard] handshake серверного мода получен — защита активна.");
        }
    }

    bool IsVerified()
    {
        return m_HandshakeOK;
    }

    static MR_Guard Instance()
    {
        static MR_Guard s;
        if (!s) s = new MR_Guard;
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
        Print("[MusicRadio][guard] TIMEOUT: handshake не получен.");
        TriggerShutdown();
    }

    static void DoCheck()
    {
        MR_Guard.Instance().Check();
    }

    //~ ----------------------------------------------------------
    //~  Принудительная остановка сервера при нарушении защиты.
    //~  Реализована через разыменование заведомо нулевой ссылки:
    //~  Enforce фиксирует критическую ошибку скрипта и процесс
    //~  сервера завершается (стандартный приём anti-piracy).
    //~  Для мягкой версии поставьте mrGraceSeconds = -1 (тогда
    //~  выполняется EndMission вместо краша).
    //~ ----------------------------------------------------------
    void TriggerShutdown()
    {
        Print("[MusicRadio][FATAL] Серверный мод @MusicRadioServer не найден!");
        Print("[MusicRadio][FATAL] Использование мода на сторонних серверах запрещено (см. LICENSE).");
        Error("UNESSED SECURITY: server mod validation failed, halting.");
        int grace = 30;
        GetGame().ConfigGetInt("MR_Config mrGraceSeconds", grace);
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
        MR_Guard.Instance().Arm();
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(MR_Guard.DoCheck, 5000, false);
    }
};

// ---------- RPC обработчик handshake (клиент) ----------
// Обработчик зарегистрирован в MR_NetworkHandler.Register()
// (AddRPC "MR_Guard" -> "OnServerHandshake", FunccType.serverbc).
modded class MR_NetworkHandler
{
    void OnServerHandshake(CallType type, ref ParamsReadContext ctx, ref PlayerIdentity sender, ref Object target)
    {
        if (GetGame().IsDedicated()) return;       // только на клиенте
        Param1<string> data;
        if (!ctx.Read(data)) return;
        MR_Guard.Instance().OnHandshake(data.param1);
    }
};

