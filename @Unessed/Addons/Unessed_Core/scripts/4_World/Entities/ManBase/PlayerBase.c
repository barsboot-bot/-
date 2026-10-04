modded class PlayerBase
{
    override void OnRPC(PlayerIdentity identity, int rpc_type, ParamsReadContext ctx)
    {
        super.OnRPC(identity, rpc_type, ctx);

        if (rpc_type == ERPCs.RPC_UNESSED_PLAY_SOUND)
        {
            Param3<string, vector, int> data;
            if (ctx.Read(data))
            {
                string trackId = data.param1;
                vector pos = data.param2;
                int sourceType = data.param3;

                PlaySoundAtLocation(trackId, pos);
            }
        }

        if (rpc_type == ERPCs.RPC_UNESSED_SERVER_CHECK)
        {
            // Проверка наличия серверного мода на клиенте
            if (!GetGame().IsServer())
            {
                Error("[UNESSED] Client detected missing server mod!");
            }
        }
    }

    private void PlaySoundAtLocation(string trackId, vector pos)
    {
        string soundSet = GetUnessedSoundMap(trackId);
        UnessedAudioManager.PlaySoundAtLocation(soundSet, pos);
    }

    private string GetUnessedSoundMap(string trackId)
    {
        // Маппинг ID трека на SoundSet
        return "Unessed_Cassette_SoundSet";
    }
};
