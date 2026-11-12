// ============================================================
//  MR_ModuleCarRadio — музыка в автомобиле
//  Игрок садится в машину с магнитолой (mrCarRadio=1), включает
//  радио/плеер — звук идёт из машины и слышен всем вокруг.
//  Громкость затухает по мере удаления от автомобиля.
// ============================================================

class ActionMR_CarRadioOn: ActionContinuousBase
{
    ActionMR_CarRadioOn() { m_CallbackClass = ActionMR_CarRadioCB; m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONVEHICLE; m_Text = "Включить музыку в машине"; }
    void CreateConditionParams(out array<ActionConditionParam> params) { params.Insert(AliveCondition); }
    bool ConditionAction(ActionData action_data)
    {
        Car car = Car.Cast(action_data.m_target_object);
        if (!car) return false;
        int flag = 0;
        GetGame().ConfigGetInt(car.GetType() + ".mrCarRadio", flag);
        return flag == 1;
    }
};

class ActionMR_CarRadioCB: ActionContinuousCallbackBase
{
    float timeTotal; float timePhase;
    void ActionMR_CarRadioCB(out ActionContinuousBase aCB) { aCB.GetDurationByPhase(timeTotal, timePhase); aCB.SetPhaseVisible(0, true); }
    bool OnActionEvaluate(ActionContinuousData data) { return true; }
    void OnActionB(ActionContinuousData data, float t)
    {
        if (GetGame().IsDedicated()) return;
        Object veh = data.m_target_object;
        if (!veh) return;
        // включаем стрим Апекс как бортовое радио (можно расширить выбором)
        // сервер определит отправителя по identity RPC (sender), цель — машина
        Param2<Object, string> p = new Param2<Object, string>(veh, "Apex");
        GetRPCManager().SendRPC("MR_Network", "CmdPlayRadio", p, true, null);
        Print("[MusicRadio] Магнитола авто включена");
    }
};

class ActionMR_CarRadioOff: ActionContinuousBase
{
    ActionMR_CarRadioOff() { m_CallbackClass = ActionMR_CarRadioOffCB; m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONVEHICLE; m_Text = "Выключить музыку в машине"; }
    void CreateConditionParams(out array<ActionConditionParam> params) { params.Insert(AliveCondition); }
};

class ActionMR_CarRadioOffCB: ActionContinuousCallbackBase
{
    float timeTotal; float timePhase;
    void ActionMR_CarRadioOffCB(out ActionContinuousBase aCB) { aCB.GetDurationByPhase(timeTotal, timePhase); }
    bool OnActionEvaluate(ActionContinuousData data) { return true; }
    void OnActionB(ActionContinuousData data, float t)
    {
        if (GetGame().IsDedicated()) return;
        int id = MR_AudioManager.FindSourceByObject(data.m_target_object);
        if (id >= 0)
        {
            Param1<int> p = new Param1<int>(id);
            GetRPCManager().SendRPC("MR_Network", "CmdStopSource", p, true, null);
        }
    }
};

modded class ActionBuilders
{
    // добавляем экшен «музыка в машине» ко всем транспортным средствам —
    // внутри ConditionAction проверяется флаг mrCarRadio
    static void AddActionCar(ref array<ActionBase> actions)
    {
        actions.Insert(new ActionMR_CarRadioOn());
        actions.Insert(new ActionMR_CarRadioOff());
    }
};

// при удалении машины останавливаем её источник на сервере
modded class Car
{
    override void EEDelete(EntityBase parent)
    {
        if (GetGame().IsDedicated())
            MR_AudioManager.Instance().StopAllByObject(this);
        super.EEDelete(parent);
    }
};
