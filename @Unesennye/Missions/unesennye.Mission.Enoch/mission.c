// ============================================================
//  mission.c — описание миссии мода «унесённые» (Chernarus/Enoch)
// ============================================================
class MissionClientUnesennye: MissionGamePlay
{
    void MissionClientUnesennye() {}
};

modded class MissionBase
{
    override void OnInit()
    {
        super.OnInit();
        // гарантируем загрузку аудио-ядра на обеих сторонах
        UE_AudioManager.Instance().InitFromConfig();
    }
};
