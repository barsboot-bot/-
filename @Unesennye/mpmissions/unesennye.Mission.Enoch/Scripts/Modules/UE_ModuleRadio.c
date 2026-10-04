// ============================================================
//  UE_ModuleRadio — интернет-радио (стримы)
//  Апекс / Европа+ / Юмор FM. Стрим буферизуется на клиенте,
//  слышен всем рядом с приёмником, громкость падает с дистанцией.
// ============================================================

class ActionUE_TuneApex: ActionContinuousBase
{
    ActionUE_TuneApex() { m_CallbackClass = ActionUE_RadioCB; m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_EMOTEATEND; m_Text = "Настроить: Апекс ФМ"; }
    void CreateConditionParams(out array<ActionConditionParam> params) { params.Insert(AliveCondition); }
}
class ActionUE_TuneEuropa: ActionContinuousBase
{
    ActionUE_TuneEuropa() { m_CallbackClass = ActionUE_RadioCB; m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_EMOTEATEND; m_Text = "Настроить: Европа Плюс"; }
    void CreateConditionParams(out array<ActionConditionParam> params) { params.Insert(AliveCondition); }
}
class ActionUE_TuneHumor: ActionContinuousBase
{
    ActionUE_TuneHumor() { m_CallbackClass = ActionUE_RadioCB; m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_EMOTEATEND; m_Text = "Настроить: Юмор FM"; }
    void CreateConditionParams(out array<ActionConditionParam> params) { params.Insert(AliveCondition); }
}
class ActionUE_StopRadio: ActionContinuousBase
{
    ActionUE_StopRadio() { m_CallbackClass = ActionUE_StopRadioCB; m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_EMOTEATEND; m_Text = "Выключить радио"; }
    void CreateConditionParams(out array<ActionConditionParam> params) { params.Insert(AliveCondition); }
}

// общий CB — ключ станции хранится в самом экшене (поле m_Station),
// а не в статике: статика между параллельными инстансами экшенов гоняет data race
class ActionUE_TuneBase: ActionContinuousBase
{
    string m_Station;   // Apex | EuropaPlus | HumorFM

    void SetupTune(string station, string text)
    {
        m_CallbackClass = ActionUE_RadioCB;
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_EMOTEATEND;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_CROUCH | DayZPlayerConstants.STANCEMASK_ERECT;
        m_FullAction = "full";
        m_Station = station;
        m_Text = text;
    }

    string GetStationKey() { return m_Station; }

    void CreateConditionParams(out array<ActionConditionParam> params)
    {
        params.Insert(AliveCondition);
    }
};

class ActionUE_TuneApex: ActionUE_TuneBase
{
    ActionUE_TuneApex() { SetupTune("Apex", "Настроить: Апекс ФМ"); }
}
class ActionUE_TuneEuropa: ActionUE_TuneBase
{
    ActionUE_TuneEuropa() { SetupTune("EuropaPlus", "Настроить: Европа Плюс"); }
}
class ActionUE_TuneHumor: ActionUE_TuneBase
{
    ActionUE_TuneHumor() { SetupTune("HumorFM", "Настроить: Юмор FM"); }
}
class ActionUE_StopRadio: ActionContinuousBase
{
    ActionUE_StopRadio() { m_CallbackClass = ActionUE_StopRadioCB; m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_EMOTEATEND; m_Text = "Выключить радио"; }
    void CreateConditionParams(out array<ActionConditionParam> params) { params.Insert(AliveCondition); }
}

class ActionUE_RadioCB: ActionContinuousCallbackBase
{
    float timeTotal; float timePhase;

    void ActionUE_RadioCB(out ActionContinuousBase aCB)
    {
        aCB.GetDurationByPhase(timeTotal, timePhase);
        aCB.SetPhaseVisible(0, true);    // крутим колесо настройки
    }

    bool OnActionEvaluate(ActionContinuousData data)
    {
        ActionUE_TuneBase tune = ActionUE_TuneBase.Cast(data.m_Action);
        return tune && tune.GetStationKey().Length() > 0;
    }

    void OnActionA(ActionContinuousData data, float t) {}

    void OnActionB(ActionContinuousData data, float t)
    {
        if (GetGame().IsDedicated()) return;
        PlayerBase pl = data.m_player;
        if (!pl || !pl.GetInventory()) return;
        ActionUE_TuneBase tune = ActionUE_TuneBase.Cast(data.m_Action);
        if (!tune) return;
        Object rx = pl.GetInventory().FindEntity("UE_RadioReceiver");
        if (!rx) return;
        Param2<Object, string> p = new Param2<Object, string>(rx, tune.GetStationKey());
        GetRPCManager().SendRPC("UE_Network", "CmdPlayRadio", p, true, null);
        Print("[унесённые] Радио: " + UE_AudioManager.GetStationName(tune.GetStationKey()));
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
        PlayerBase pl = data.m_player;
        if (!pl || !pl.GetInventory()) return;
        Object rx = pl.GetInventory().FindEntity("UE_RadioReceiver");
        if (!rx) return;
        int id = UE_AudioManager.FindSourceByObject(rx);
        if (id >= 0)
        {
            Param1<int> p = new Param1<int>(id);
            GetRPCManager().SendRPC("UE_Network", "CmdStopSource", p, true, null);
        }
    }
};

modded class ActionBuilders
{
    static void AddActionUE_RadioReceiver(ref array<ActionBase> actions)
    {
        actions.Insert(new ActionUE_TuneApex());
        actions.Insert(new ActionUE_TuneEuropa());
        actions.Insert(new ActionUE_TuneHumor());
        actions.Insert(new ActionUE_StopRadio());
    }
};
