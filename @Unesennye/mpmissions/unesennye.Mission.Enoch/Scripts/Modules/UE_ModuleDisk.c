// ============================================================
//  UE_ModuleDisk — проигрыватель виниловых/CD дисков
// ============================================================

class ActionUE_PlayDisk: ActionContinuousBase
{
    ActionUE_PlayDisk()
    {
        m_CallbackClass = ActionUE_PlayDiskCB;
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_EMOTEATEND;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_CROUCH | DayZPlayerConstants.STANCEMASK_ERECT;
        m_FullAction = "full";
        m_Text = "Включить музыку (диск)";
    }

    void CreateConditionParams(out array<ActionConditionParam> params)
    {
        params.Insert(AliveCondition);
        params.Insert(IsNotBusyCondition);
    }

    bool ConditionAction(ActionData action_data)
    {
        return HaveInInventory(action_data.m_main_player, "UE_Item_Disk_Classic") ||
               HaveInInventory(action_data.m_main_player, "UE_Item_Disk_Dance");
    }

    private bool HaveInInventory(PlayerBase pl, string cls)
    {
        if (!pl || !pl.GetInventory()) return false;
        return pl.GetInventory().FindEntity(cls) != null;
    }
};

class ActionUE_PlayDiskCB: ActionContinuousCallbackBase
{
    float timeTotal;
    float timePhase;

    void ActionUE_PlayDiskCB(out ActionContinuousBase aCB)
    {
        aCB.GetDurationByPhase(timeTotal, timePhase);
        aCB.SetPhaseVisible(0, true);   // положить диск на деку
        aCB.SetPhaseVisible(1, true);   // опустить иглу / нажать Play
    }

    bool OnActionEvaluate(ActionContinuousData data) { return true; }

    void OnActionA(ActionContinuousData data, float t) {}

    void OnActionB(ActionContinuousData data, float t)
    {
        if (GetGame().IsDedicated()) return;
        PlayerBase pl = data.m_player;
        if (!pl || !pl.GetInventory()) return;
        EntityAI disk = pl.GetInventory().FindEntity("UE_Item_Disk_Classic");
        if (!disk) disk = pl.GetInventory().FindEntity("UE_Item_Disk_Dance");
        if (!disk) return;

        string playlist = disk.GetType() == "UE_Item_Disk_Classic" ? "Classic" : "Dance";
        Object deck = pl.GetInventory().FindEntity("UE_DiskPlayer");
        if (!deck) return;

        Param2<Object, string> p = new Param2<Object, string>(deck, playlist);
        GetRPCManager().SendRPC("UE_Network", "CmdPlayDisk", p, true, null);
        Print("[унесённые] Запрос на воспроизведение диска: " + playlist);
    }
};

modded class ActionBuilders
{
    static void AddActionUE_DiskPlayer(ref array<ActionBase> actions)
    {
        actions.Insert(new ActionUE_PlayDisk());
    }
};
