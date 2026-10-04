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
        if (pid.Length() == 0) pid = "unknown";

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

    //~ ---------------------------------------------------------
    //~  Валидация ключа плейлиста против ФИЗИЧЕСКОЙ библиотеки
    //~  сервера (папка Music/Type, Music/CD). Читер не сможет
    //~  включить то, чего нет на диске. Legacy-имена (Rock/Pop/
    //~  Classic/Dance) разрешаем — они играют из PBO.
    //~ ---------------------------------------------------------
    static ref array<string> LEGACY_PLAYLISTS;
    bool ValidatePlaylistKey(string key, string dirName, string pid)
    {
        if (!LEGACY_PLAYLISTS)
        {
            LEGACY_PLAYLISTS = new array<string>;
            LEGACY_PLAYLISTS.Insert("rock");
            LEGACY_PLAYLISTS.Insert("pop");
            LEGACY_PLAYLISTS.Insert("classic");
            LEGACY_PLAYLISTS.Insert("dance");
        }
        // полный ключ библиотеки ("Type/MyMix") или имя папки?
        string probe = key;
        if (!(probe.StartsWith("Type/") || probe.StartsWith("CD/")))
            probe = dirName + "/" + key;
        else if (probe.SubstringWithLimit(0, probe.IndexOf("/")) != dirName)
        {
            LogViolation(pid, "плейлист из чужой категории: " + key);
            return false;
        }

        if (UE_MusicLibrary.HasPlaylist(probe)) return true;

        // legacy-ключи PBO допускаются только если библиотека вообще пуста
        // (иначе админ явно перешёл на внешние папки)
        string base = probe.SubstringWithLimit(probe.IndexOf("/") + 1, probe.Length()).ToLower();
        if (UE_MusicLibrary.s_SerPlaylists.Count() == 0 && LEGACY_PLAYLISTS.Find(base) >= 0) return true;

        LogViolation(pid, "несуществующий плейлист: " + key);
        return false;
    }

    bool IsVehicleWithRadio(Object o)
    {
        Car car = Car.Cast(o);
        if (!car) return false;
        // читаем флаг ueCarRadio из config.cpp CfgVehicles данного класса
        int flag = 0;
        GetGame().ConfigGetInt(car.GetType() + ".ueCarRadio", flag);
        return flag == 1;
    }

    bool HasAccess(PlayerBase pl, Object o)
    {
        if (!pl || !o) return false;
        // объект в руках/инвентаре игрока?
        if (o == pl) return true;
        if (pl.GetInventory() && pl.GetInventory().FindItem(o.GetType()) == o) return true;
        // объект лежит рядом (< 5 м)
        if (vector.Distance(pl.GetPosition(), o.GetPosition()) < 5.0) return true;
        // машина, в которой едет игрок
        Car c = Car.Cast(o);
        if (c && pl.IsInVehicle(c)) return true;
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
    //~  который знает только клиент с установленным @Uness.
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
            h ^= (uint)src.Get(i);
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
