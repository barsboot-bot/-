// ============================================================
//  UE_Init.c — точка входа миссии мода «унесённые»
//  Загружает скрипты, регистрирует RPC, стартует менеджеры.
// ============================================================

class MissionHandlerUnesennye: MissionServer
{
    void MissionHandlerUnesennye()
    {
        // загрузка всех скриптов мода (компиляция EnforceScript)
        Print("=== Мод «унесённые»: инициализация ===");
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
        // библиотеки (<Profile>/Music: Type/, CD/, Radio.txt) и
        // синхронизируем активные источники
        if (GetGame().IsDedicated())
        {
            UE_ModulePlayer.RPC_SendManifest(player.GetIdentity());
            auto mgr = UE_AudioManager.Instance();
            for (int i = 0; i < mgr.m_Sources.Count(); i++)
            {
                UE_PlaybackState st = mgr.m_Sources.GetByIndex(i).Get2();
                if (st.isPlaying)
                    UE_ModulePlayer.RPC_CreateSource(st.id, st.type, st.stationKey, st.playlist, st.position, st.volume, st.startedAt);
            }
        }
    }
};

// регистрация модулей на клиенте и сервере
[RegisterModule("UE_Network", "1")]
class UE_Register: ModuleBase
{
    override void Init()
    {
        UE_NetworkHandler h = new UE_NetworkHandler;
        h.Register();
        GetRPCManager().AddDRPC("UE_Network", "OnClientCreateSourceEx", this, FunccType.serverbc);
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
