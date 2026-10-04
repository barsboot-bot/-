// ============================================================
//  mission.c — описание миссии мода «унесённые» (Chernarus/Enoch)
// ============================================================
class MissionClientUnesennye: MissionGamePlay
{
    ref UE_DownloadWatcher m_DownloadWatcher;   // докачка треков с HTTP-зеркала

    void MissionClientUnesennye()
    {
        m_DownloadWatcher = new UE_DownloadWatcher;
        // регистрируем асинхронный загрузчик файлов библиотеки
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Call(
            GetGame().CreateAsyncFileDownloader(), "Download",
            array<string>({"placeholder"}), "OnDownloadFinished", "OnDownloadFinished", CALL_STATE_OK);
    }

    void OnDownloadFinished(string arg, CallReturnCodes return_code, uint data)
    {
        if (m_DownloadWatcher) m_DownloadWatcher.OnDownloadFinished(arg, return_code, data);
    }
};

modded class ModuleManager
{
    override void Init()
    {
        super.Init();
        // сетевой слой мода: регистрируем RPC-обработчики (клиент + сервер)
        UE_NetworkHandler h = new UE_NetworkHandler;
        h.Register();
        Print("[унесённые] Сетевые обработчики зарегистрированы");
    }
};
