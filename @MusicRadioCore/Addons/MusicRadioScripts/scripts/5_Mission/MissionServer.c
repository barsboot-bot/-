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
        Print("=== Мод MusicRadio: серверная инициализация ===");
    }

    override void OnInit()
    {
        super.OnInit();

        // 1) сетевой слой: регистрация RPC-обработчиков
        MR_NetworkHandler net = new MR_NetworkHandler;
        net.Register();

        // 2) параметры из config.cpp (MR_Config), скан внешней
        //    музыкальной библиотеки <Profile>/Music, Radio.txt
        MR_AudioManager.Instance().InitFromConfig();

        // 3) серверная защита (валидация команд, анти-спам)
        MR_Security.Instance();

        Print("[MusicRadio] сервер готов. Версия 1.1.0");
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
                MR_ModulePlayer.RPC_SendManifest, 5000, false, ident);
        }
    }

    override void OnMissionFinish()
    {
        // graceful shutdown: глушим все источники перед остановкой
        auto mgr = MR_AudioManager.Instance();
        for (int i = mgr.m_Sources.Count() - 1; i >= 0; i--)
        {
            MR_PlaybackState st = mgr.m_Sources.GetByIndex(i).Get2();
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
        MR_AudioManager.Instance().OnUpdate(timeDelta);
    }
};
