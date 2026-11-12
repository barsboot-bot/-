// ============================================================
//  MR_ModuleRadio — интернет-радио (стримы)
//  Апекс / Европа+ / Юмор FM. Стрим буферизуется на клиенте,
//  слышен всем рядом с приёмником, громкость падает с дистанцией.
// ============================================================

class ActionMR_TuneApex: ActionContinuousBase
{
    ActionMR_TuneApex() { m_CallbackClass = ActionMR_RadioCB; m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_EMOTEATEND; m_Text = "Настроить: Апекс ФМ"; }
    void CreateConditionParams(out array<ActionConditionParam> params) { params.Insert(AliveCondition); }
}
class ActionMR_TuneEuropa: ActionContinuousBase
{
    ActionMR_TuneEuropa() { m_CallbackClass = ActionMR_RadioCB; m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_EMOTEATEND; m_Text = "Настроить: Европа Плюс"; }
    void CreateConditionParams(out array<ActionConditionParam> params) { params.Insert(AliveCondition); }
}
class ActionMR_TuneHumor: ActionContinuousBase
{
    ActionMR_TuneHumor() { m_CallbackClass = ActionMR_RadioCB; m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_EMOTEATEND; m_Text = "Настроить: Юмор FM"; }
    void CreateConditionParams(out array<ActionConditionParam> params) { params.Insert(AliveCondition); }
}
class ActionMR_StopRadio: ActionContinuousBase
{
    ActionMR_StopRadio() { m_CallbackClass = ActionMR_StopRadioCB; m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_EMOTEATEND; m_Text = "Выключить радио"; }
    void CreateConditionParams(out array<ActionConditionParam> params) { params.Insert(AliveCondition); }
}

// общий CB — ключ станции хранится в самом экшене (поле m_Station),
// а не в статике: статика между параллельными инстансами экшенов гоняет data race
class ActionMR_TuneBase: ActionContinuousBase
{
    string m_Station;   // Apex | EuropaPlus | HumorFM

    void SetupTune(string station, string text)
    {
        m_CallbackClass = ActionMR_RadioCB;
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

class ActionMR_TuneApex: ActionMR_TuneBase
{
    ActionMR_TuneApex() { SetupTune("Apex", "Настроить: Апекс ФМ"); }
}
class ActionMR_TuneEuropa: ActionMR_TuneBase
{
    ActionMR_TuneEuropa() { SetupTune("EuropaPlus", "Настроить: Европа Плюс"); }
}
class ActionMR_TuneHumor: ActionMR_TuneBase
{
    ActionMR_TuneHumor() { SetupTune("HumorFM", "Настроить: Юмор FM"); }
}
class ActionMR_StopRadio: ActionContinuousBase
{
    ActionMR_StopRadio() { m_CallbackClass = ActionMR_StopRadioCB; m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_EMOTEATEND; m_Text = "Выключить радио"; }
    void CreateConditionParams(out array<ActionConditionParam> params) { params.Insert(AliveCondition); }
}

class ActionMR_RadioCB: ActionContinuousCallbackBase
{
    float timeTotal; float timePhase;

    void ActionMR_RadioCB(out ActionContinuousBase aCB)
    {
        aCB.GetDurationByPhase(timeTotal, timePhase);
        aCB.SetPhaseVisible(0, true);    // крутим колесо настройки
    }

    bool OnActionEvaluate(ActionContinuousData data)
    {
        ActionMR_TuneBase tune = ActionMR_TuneBase.Cast(data.m_Action);
        return tune && tune.GetStationKey().Length() > 0;
    }

    void OnActionA(ActionContinuousData data, float t) {}

    void OnActionB(ActionContinuousData data, float t)
    {
        if (GetGame().IsDedicated()) return;
        PlayerBase pl = data.m_player;
        if (!pl || !pl.GetInventory()) return;
        ActionMR_TuneBase tune = ActionMR_TuneBase.Cast(data.m_Action);
        if (!tune) return;
        Object rx = pl.GetInventory().FindEntity("MR_RadioReceiver");
        if (!rx) return;
        Param2<Object, string> p = new Param2<Object, string>(rx, tune.GetStationKey());
        GetRPCManager().SendRPC("MR_Network", "CmdPlayRadio", p, true, null);
        Print("[MusicRadio] Радио: " + MR_AudioManager.GetStationName(tune.GetStationKey()));
    }
};

class ActionMR_StopRadioCB: ActionContinuousCallbackBase
{
    float timeTotal; float timePhase;
    void ActionMR_StopRadioCB(out ActionContinuousBase aCB) { aCB.GetDurationByPhase(timeTotal, timePhase); }
    bool OnActionEvaluate(ActionContinuousData data) { return true; }
    void OnActionB(ActionContinuousData data, float t)
    {
        if (GetGame().IsDedicated()) return;
        // просим сервер остановить источник этого приёмника
        PlayerBase pl = data.m_player;
        if (!pl || !pl.GetInventory()) return;
        Object rx = pl.GetInventory().FindEntity("MR_RadioReceiver");
        if (!rx) return;
        int id = MR_AudioManager.FindSourceByObject(rx);
        if (id >= 0)
        {
            Param1<int> p = new Param1<int>(id);
            GetRPCManager().SendRPC("MR_Network", "CmdStopSource", p, true, null);
        }
    }
};

modded class ActionBuilders
{
    static void AddActionMR_RadioReceiver(ref array<ActionBase> actions)
    {
        actions.Insert(new ActionMR_TuneApex());
        actions.Insert(new ActionMR_TuneEuropa());
        actions.Insert(new ActionMR_TuneHumor());
        actions.Insert(new ActionMR_StopRadio());
    }
};
