modded class Unessed_AudioTape
{
    protected string m_TrackID;
    protected string m_TrackName;

    void SetTrackData(string trackId, string trackName)
    {
        m_TrackID = trackId;
        m_TrackName = trackName;
    }

    string GetTrackID()
    {
        return m_TrackID;
    }

    string GetTrackName()
    {
        return m_TrackName;
    }

    override void OnRPC(PlayerIdentity identity, int rpc_type, ParamsReadContext ctx)
    {
        super.OnRPC(identity, rpc_type, ctx);

        if (rpc_type == ERPCs.RPC_UNESSED_PLAY_SOUND)
        {
            // Клиент получает команду воспроизвести звук
            // Фактическое воспроизведение обрабатывается в PlayerBase
        }
    }
}
