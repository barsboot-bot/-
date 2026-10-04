// ============================================================
//  UE_Init.c — точка входа миссии мода «унесённые»
//  Загружает скрипты, регистрирует RPC, стартует менеджеры.
// ============================================================

class MissionHandlerUnesennye: MissionServer
{
    ref UE_DownloadWatcher m_DownloadWatcher;   // клиентские докачки (безвредно на сервере)

    void MissionHandlerUnesennye()
    {
        // загрузка всех скриптов мода (компиляция EnforceScript)
        Print("=== Мод «унесённые»: инициализация ===");
        m_DownloadWatcher = new UE_DownloadWatcher;
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Call(
            GetGame().CreateAsyncFileDownloader(), "Download",
            array<string>({"placeholder"}), "OnDownloadFinished", "OnDownloadFinished", CALL_STATE_OK);
    }

    override void OnInit()
    {
        super.OnInit();
        // серверная защита включается первой
        UE_Security.Instance();
        UE_AudioManager.Instance().InitFromConfig();
    }

    override void OnPlayerConnect(PlayerBase player)
    {
        super.OnPlayerConnect(player);
        // при подключении игрока отправляем манифест внешней музыкальной
        // библиотеки (<Profile>/Music: Type/, CD/, Radio.txt)
        if (GetGame().IsDedicated())
        {
            PlayerIdentity ident = player.GetIdentity();
            // даём клиенту закончить загрузку — шлём отложенным вызовом
            GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(
                UE_ModulePlayer.RPC_SendManifest, 5000, false, ident);
        }
    }

    // синхронизация активных источников одному клиенту (JIP-пакеты
    // уже покрыты флагом jip=true у RPC_CreateSource; этот метод —
    // резервный путь для ручного вызова)
    static void SyncSourcesToPlayer(PlayerBase player)
    {
        auto mgr = UE_AudioManager.Instance();
        for (int i = 0; i < mgr.m_Sources.Count(); i++)
        {
            UE_PlaybackState st = mgr.m_Sources.GetByIndex(i).Get2();
            if (st && st.isPlaying)
                UE_ModulePlayer.RPC_CreateSource(st.id, st.type, st.stationKey, st.playlist, st.position, st.volume, st.startedAt);
        }
    }

    void OnDownloadFinished(string arg, CallReturnCodes return_code, uint data)
    {
        // проксируем событие докачки в наблюдатель загрузок
        if (m_DownloadWatcher) m_DownloadWatcher.OnDownloadFinished(arg, return_code, data);
    }
};

// регистрация модулей на клиенте и сервере
modded class ModuleManager
{
    override void Init()
    {
        super.Init();
        // сетевой слой мода: регистрируем RPC-обработчики
        UE_NetworkHandler h = new UE_NetworkHandler;
        h.Register();
        Print("[унесённые] Сетевые обработчики зарегистрированы");
    }
};

modded class DayZGame
{
    // глобальный тик аудио-менеджера
    override void OnUpdate(float timeDelta)
    {
        super.OnUpdate(timeDelta);
        UE_AudioManager.Instance().OnUpdate(timeDelta);
    }
};
