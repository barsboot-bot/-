// ============================================================
//  UE_Security — серверная защита мода «унесённые»
//  Назначение: не давать читерам/подменённым клиентам рассылать
//  поддельные команды (запуск музыки из ниоткуда, спам стримами).
//  Все действия с аудио проходят через валидацию НА СЕРВЕРЕ.
// ============================================================

class UE_Security: ScriptModule
{
    // --- лимиты и анти-спам ---
    private const int MAX_ACTIONS_PER_MIN = 20;      // команд на игрока в минуту
    private const float MIN_ACTION_INTERVAL = 1.5;   // мин. интервал между командами
    private const int MAX_ACTIVE_SOURCES = 64;       // всего источников на сервере
    private const float MAX_SOURCE_RADIUS = 300;     // м — радиус допустимого взаимодействия

    struct FPlayerBucket { float lastAction; int count; int windowStart; };

    ref map<string, ref FPlayerBucket> m_Buckets;    // profileID -> бакет
    bool m_IsServer;

    void UE_Security()
    {
        m_Buckets = new map<string, ref FPlayerBucket>;
        m_IsServer = GetGame().IsDedicated();
    }

    //~ ---------------------------------------------------------
    //~  ГЛАВНАЯ ТОЧКА ВХОДА: сервер принимает команду от клиента
    //~  и проверяет её легитимность ПЕРЕД исполнением.
    //~ ---------------------------------------------------------
    bool ValidateCommand(PlayerBase player, Object targetObj, int actionType)
    {
        if (!m_IsServer) return false;                 // только сервер решает
        if (!player) return false;

        string pid = player.GetID();                   // ID соединения/профиля

        // 1) игрок должен существовать и быть живым
        if (player.IsDead()) { LogViolation(pid, "мёртвый игрок пытается управлять"); return false; }

        // 2) цель существует и это legit-объект мода
        if (!targetObj || !IsAllowedObject(targetObj)) { LogViolation(pid, "цель не является объектом мода"); return false; }

        // 3) расстояние: нельзя включать плеер на другом конце карты
        vector pPos = player.GetPosition();
        vector oPos = targetObj.GetPosition();
        if (vector.Distance(pPos, oPos) > MAX_SOURCE_RADIUS)
        {
            LogViolation(pid, "слишком далеко до цели"); return false;
        }

        // 4) предмет должен быть в инвентаре/рядом с игроком (владелец или в зоне)
        if (!HasAccess(player, targetObj)) { LogViolation(pid, "нет доступа к объекту"); return false; }

        // 5) анти-спам по времени
        if (!RateLimitOk(pid)) { LogViolation(pid, "флуд командами"); return false; }

        // 6) лимит активных источников на сервере
        if (UE_AudioManager.Instance().m_Sources.Count() >= MAX_ACTIVE_SOURCES)
        {
            LogViolation(pid, "предел источников"); return false;
        }

        return true;
    }

    bool IsAllowedObject(Object o)
    {
        string t = o.GetType();
        return t == "UE_CassettePlayer" || t == "UE_DiskPlayer" ||
               t == "UE_RadioReceiver"  || t == "UE_CarRadioUnit" ||
               IsVehicleWithRadio(o);
    }

    bool IsVehicleWithRadio(Object o)
    {
        Car car = Car.Cast(o);
        if (!car) return false;
        // читаем флаг ueCarRadio из config.cpp CfgVehicles данного класса
        int flag = GetGame().ConfigGetInt(car.GetType(), "ueCarRadio");
        return flag == 1;
    }

    bool HasAccess(PlayerBase pl, Object o)
    {
        // объект в руках/инвентаре игрока?
        if (o == pl) return true;
        if (pl.CanReachObject(o)) return true;
        // объект лежит рядом (< 5 м)
        if (vector.Distance(pl.GetPosition(), o.GetPosition()) < 5.0) return true;
        // машина, в которой едет игрок
        if (pl.GetParentOfAttachment() == o || pl.InVehicle() == Vehicle.Cast(o)) return true;
        return false;
    }

    bool RateLimitOk(string pid)
    {
        int nowSec = (GetGame().GetTime() / 1000);
        FPlayerBucket b;
        if (!m_Buckets.Find(pid, b)) { b = new FPlayerBucket; b.windowStart = nowSec; b.count = 0; m_Buckets.Set(pid, b); }
        if (nowSec - b.windowStart > 60) { b.windowStart = nowSec; b.count = 0; }
        float nowF = GetGame().GetTime() * 0.001;
        if (nowF - b.lastAction < MIN_ACTION_INTERVAL) return false;
        b.count++;
        b.lastAction = nowF;
        if (b.count > MAX_ACTIONS_PER_MIN) return false;
        return true;
    }

    //~ ---------------------------------------------------------
    //~  Проверка подписи пакета RPC: сверяем «секрет» мода,
    //~  который знает только клиент с установленным @Unesennye.
    //~  Иначе любой может отправить запрос без мода.
    //~ ---------------------------------------------------------
    static string ComputeHMAC(string payload)
    {
        // простой солёный дайджест (в проде — движковый SHA)
        const string SALT = "UNESENNYE_2026_AUDIO_KEY";
        string src = SALT + payload + SALT;
        uint h = 2166136261;
        for (int i = 0; i < src.Length(); i++)
        {
            h ^= src.Get(i);
            h *= 16777619;
        }
        return "" + h;
    }

    bool VerifyPayloadSignature(string payload, string sig)
    {
        return ComputeHMAC(payload) == sig;
    }

    void LogViolation(string pid, string reason)
    {
        Print("[UE_Security] НАРУШЕНИЕ: игрок " + pid + " — " + reason);
        // здесь можно интегрировать бан-систему сервера
    }

    static UE_Security Instance()
    {
        static UE_Security s;
        if (!s) s = new UE_Security;
        return s;
    }
};
