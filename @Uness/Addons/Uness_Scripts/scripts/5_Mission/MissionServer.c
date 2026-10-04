// ============================================================
//  MissionServer — точка входа сервера (missionScriptModule)
//  По стандарту BI: в аддоне создаётся класс, наследующий
//  MissionServer; движок инстанцирует его при старте миссии.
//  Здесь инициализируются аудио-менеджер, библиотека и защита.
// ============================================================

modded class MissionServer
{
    void MissionServer()
    {
        Print("=== Мод «унесённые»: серверная инициализация ===");
    }

    override void OnInit()
    {
        super.OnInit();

        // 1) сетевой слой: регистрация RPC-обработчиков
        UE_NetworkHandler net = new UE_NetworkHandler;
        net.Register();

        // 2) параметры из config.cpp (UE_Config), скан внешней
        //    музыкальной библиотеки <Profile>/Music, Radio.txt
        UE_AudioManager.Instance().InitFromConfig();

        // 3) серверная защита (валидация команд, анти-спам)
        UE_Security.Instance();

        Print("[унесённые] сервер готов. Версия 1.0.0");
    }

    override void OnPlayerConnect(PlayerBase player)
    {
        super.OnPlayerConnect(player);
        // отправляем подключившемуся игроку манифест музыкальной
        // библиотеки (плейлисты Music/Type, Music/CD, станции Radio.txt).
        // Отложенный вызов — даём клиенту закончить загрузку сценария.
        PlayerIdentity ident = player.GetIdentity();
        if (ident)
        {
            GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(
                UE_ModulePlayer.RPC_SendManifest, 5000, false, ident);
        }
    }

    override void OnMissionFinish()
    {
        // graceful shutdown: глушим все источники перед остановкой
        auto mgr = UE_AudioManager.Instance();
        for (int i = mgr.m_Sources.Count() - 1; i >= 0; i--)
        {
            UE_PlaybackState st = mgr.m_Sources.GetByIndex(i).Get2();
            if (st) mgr.StopSource(st.id);
        }
        super.OnMissionFinish();
    }
};

// глобальный тик аудио-менеджера (пересчёт громкости у клиентов,
// обновление позиций движущихся источников на сервере)
modded class DayZGame
{
    override void OnUpdate(float timeDelta)
    {
        super.OnUpdate(timeDelta);
        UE_AudioManager.Instance().OnUpdate(timeDelta);
    }
};
