modded class MissionServer
{
    protected bool m_ServerModLoaded = false;

    override void OnInit()
    {
        super.OnInit();
        Print("[UNESSED] Server module initialized.");
        m_ServerModLoaded = true;

        // Проверка целостности при старте
        if (!ValidateServerMod())
        {
            Error("[UNESSED] CRITICAL: Server mod validation failed!");
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(CrashServer, 1000, false);
        }
    }

    protected bool ValidateServerMod()
    {
        // Проверка наличия серверного мода
        return m_ServerModLoaded;
    }

    protected void CrashServer()
    {
        // Преднамеренный краш при отсутствии серверного мода
        Error("[UNESSED] Server crash: Unessed_Server.pbo not found or invalid!");
        GetGame().EndGame();
    }

    void OnRequestPlayMusic(PlayerIdentity identity, Object sourceObj, string trackId)
    {
        PlayerBase player = PlayerBase.Cast(identity.GetPlayer());
        if (!player) return;

        // Проверка наличия предмета
        if (!ValidatePlayerHasItem(player, trackId))
        {
            Print("[UNESSED SECURITY] Player " + identity.GetName() + " tried to play fake track!");
            return;
        }

        // Синхронизация с nearby игроками
        array<Object> nearbyPlayers = new array<Object>;
        GetGame().GetObjectsAtPosition3D(sourceObj.GetPosition(), UnessedConstants.RADIO_HEARING_DISTANCE, nearbyPlayers, null);

        foreach (Object obj : nearbyPlayers)
        {
            PlayerBase p = PlayerBase.Cast(obj);
            if (p && p.GetIdentity())
            {
                Param3<string, vector, int> data = new Param3<string, vector, int>(trackId, sourceObj.GetPosition(), sourceObj.GetType());
                GetGame().RPCSingleParam(p, ERPCs.RPC_UNESSED_PLAY_SOUND, data, true);
            }
        }
    }

    protected bool ValidatePlayerHasItem(PlayerBase player, string trackId)
    {
        // Проверка инвентаря игрока
        ItemBase itemInHands = player.GetItemInHands();
        if (itemInHands && itemInHands.GetType().Contains("AudioTape"))
        {
            return true;
        }

        // Проверка вставленного предмета в устройство
        // ... дополнительная логика

        return false;
    }

    // Команда для админов: радио
    void ServerCommand_Radio(string trackId)
    {
        // Только для админов
        // Вещание на весь сервер
    }
};
