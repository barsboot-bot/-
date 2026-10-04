// ============================================================
//  UE_ModuleRadio — интернет-радио (стримы)
//  Апекс / Европа+ / Юмор FM. Стрим буферизуется на клиенте,
//  слышен всем рядом с приёмником, громкость падает с дистанцией.
// ============================================================

class ActionUE_TuneApex: ActionContinuousBase
{
    ActionUE_TuneApex() { m_CallbackClass = ActionUE_RadioCB; m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_EMOTEATEND; m_Text = "Настроить: Апекс ФМ"; }
    void CreateConditionParams(ref out array<ActionConditionParam> params) { params.Insert(AliveCondition); }
}
class ActionUE_TuneEuropa: ActionContinuousBase
{
    ActionUE_TuneEuropa() { m_CallbackClass = ActionUE_RadioCB; m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_EMOTEATEND; m_Text = "Настроить: Европа Плюс"; }
    void CreateConditionParams(ref out array<ActionConditionParam> params) { params.Insert(AliveCondition); }
}
class ActionUE_TuneHumor: ActionContinuousBase
{
    ActionUE_TuneHumor() { m_CallbackClass = ActionUE_RadioCB; m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_EMOTEATEND; m_Text = "Настроить: Юмор FM"; }
    void CreateConditionParams(ref out array<ActionConditionParam> params) { params.Insert(AliveCondition); }
}
class ActionUE_StopRadio: ActionContinuousBase
{
    ActionUE_StopRadio() { m_CallbackClass = ActionUE_StopRadioCB; m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_EMOTEATEND; m_Text = "Выключить радио"; }
    void CreateConditionParams(ref out array<ActionConditionParam> params) { params.Insert(AliveCondition); }
}

// общий CB — ключ станции передаётся через m_Text разбор или отдельное поле
class ActionUE_RadioCB: ActionContinuousCallbackBase
{
    float timeTotal; float timePhase;
    static string pendingStation = "";   // заполняется в OnActionEvaluate по классу экшена

    void ActionUE_RadioCB(out ActionContinuousBase aCB)
    {
        aCB.GetDurationByPhase(timeTotal, timePhase);
        aCB.SetPhaseVisible(0, true);    // крутим колесо настройки
    }

    bool OnActionEvaluate(ActionContinuousData data)
    {
        // определяем станцию по названию экшена
        string txt = data.m_Action.GetText();
        if (txt.Contains("Апекс"))       pendingStation = "Apex";
        else if (txt.Contains("Европа")) pendingStation = "EuropaPlus";
        else if (txt.Contains("Юмор"))   pendingStation = "HumorFM";
        return pendingStation.Length() > 0;
    }

    void OnActionA(ActionContinuousData data, float t) {}

    void OnActionB(ActionContinuousData data, float t)
    {
        if (GetGame().IsDedicated()) return;
        PlayerBase pl = data.m_player;
        Object rx = pl.GetInventory().FindItem("UE_RadioReceiver");
        if (!rx) return;
        Param2<Object, string> p = new Param2<Object, string>(rx, pendingStation);
        GetRPCManager().SendRPC("UE_Network", "CmdPlayRadio", p, true, null);
        Print("[унесённые] Радио: " + UE_AudioManager.GetStationName(pendingStation));
    }
};

class ActionUE_StopRadioCB: ActionContinuousCallbackBase
{
    float timeTotal; float timePhase;
    void ActionUE_StopRadioCB(out ActionContinuousBase aCB) { aCB.GetDurationByPhase(timeTotal, timePhase); }
    bool OnActionEvaluate(ActionContinuousData data) { return true; }
    void OnActionB(ActionContinuousData data, float t)
    {
        if (GetGame().IsDedicated()) return;
        // просим сервер остановить источник этого приёмника
        Object rx = data.m_player.GetInventory().FindItem("UE_RadioReceiver");
        if (!rx) return;
        int id = UE_FindSourceByObject(rx);
        if (id >= 0)
        {
            Param1<int> p = new Param1<int>(id);
            GetRPCManager().SendRPC("UE_Network", "CmdStopSource", p, true, null);
        }
    }
};

static int UE_FindSourceByObject(Object o)
{
    auto mgr = UE_AudioManager.Instance();
    for (int i = 0; i < mgr.m_Sources.Count(); i++)
    {
        UE_PlaybackState st = mgr.m_Sources.GetByIndex(i).Get2();
        if (st.object == o) return st.id;
    }
    return -1;
}

modded class ActionHandlers
{
    static ref array<ref ActionBase> CreateActionsUE_RadioReceiver(out ActionBase actions[], Object object, int slot, ItemType type)
    {
        actions.Insert(new ActionUE_TuneApex());
        actions.Insert(new ActionUE_TuneEuropa());
        actions.Insert(new ActionUE_TuneHumor());
        actions.Insert(new ActionUE_StopRadio());
        return actions;
    }
};
