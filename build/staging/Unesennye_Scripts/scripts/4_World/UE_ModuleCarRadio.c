// ============================================================
//  UE_ModuleCarRadio — музыка в автомобиле
//  Игрок садится в машину с магнитолой (ueCarRadio=1), включает
//  радио/плеер — звук идёт из машины и слышен всем вокруг.
//  Громкость затухает по мере удаления от автомобиля.
// ============================================================

class ActionUE_CarRadioOn: ActionContinuousBase
{
    ActionUE_CarRadioOn() { m_CallbackClass = ActionUE_CarRadioCB; m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONVEHICLE; m_Text = "Включить музыку в машине"; }
    void CreateConditionParams(out array<ActionConditionParam> params) { params.Insert(AliveCondition); }
    bool ConditionAction(ActionData action_data)
    {
        Car car = Car.Cast(action_data.m_target_object);
        if (!car) return false;
        int flag = 0;
        GetGame().ConfigGetInt(car.GetType() + ".ueCarRadio", flag);
        return flag == 1;
    }
};

class ActionUE_CarRadioCB: ActionContinuousCallbackBase
{
    float timeTotal; float timePhase;
    void ActionUE_CarRadioCB(out ActionContinuousBase aCB) { aCB.GetDurationByPhase(timeTotal, timePhase); aCB.SetPhaseVisible(0, true); }
    bool OnActionEvaluate(ActionContinuousData data) { return true; }
    void OnActionB(ActionContinuousData data, float t)
    {
        if (GetGame().IsDedicated()) return;
        Object veh = data.m_target_object;
        if (!veh) return;
        // включаем стрим Апекс как бортовое радио (можно расширить выбором)
        // сервер определит отправителя по identity RPC (sender), цель — машина
        Param2<Object, string> p = new Param2<Object, string>(veh, "Apex");
        GetRPCManager().SendRPC("UE_Network", "CmdPlayRadio", p, true, null);
        Print("[унесённые] Магнитола авто включена");
    }
};

class ActionUE_CarRadioOff: ActionContinuousBase
{
    ActionUE_CarRadioOff() { m_CallbackClass = ActionUE_CarRadioOffCB; m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONVEHICLE; m_Text = "Выключить музыку в машине"; }
    void CreateConditionParams(out array<ActionConditionParam> params) { params.Insert(AliveCondition); }
};

class ActionUE_CarRadioOffCB: ActionContinuousCallbackBase
{
    float timeTotal; float timePhase;
    void ActionUE_CarRadioOffCB(out ActionContinuousBase aCB) { aCB.GetDurationByPhase(timeTotal, timePhase); }
    bool OnActionEvaluate(ActionContinuousData data) { return true; }
    void OnActionB(ActionContinuousData data, float t)
    {
        if (GetGame().IsDedicated()) return;
        int id = UE_AudioManager.FindSourceByObject(data.m_target_object);
        if (id >= 0)
        {
            Param1<int> p = new Param1<int>(id);
            GetRPCManager().SendRPC("UE_Network", "CmdStopSource", p, true, null);
        }
    }
};

modded class ActionBuilders
{
    // добавляем экшен «музыка в машине» ко всем транспортным средствам —
    // внутри ConditionAction проверяется флаг ueCarRadio
    static void AddActionCar(ref array<ActionBase> actions)
    {
        actions.Insert(new ActionUE_CarRadioOn());
        actions.Insert(new ActionUE_CarRadioOff());
    }
};

// при удалении машины останавливаем её источник на сервере
modded class Car
{
    override void EEDelete(EntityBase parent)
    {
        if (GetGame().IsDedicated())
            UE_AudioManager.Instance().StopAllByObject(this);
        super.EEDelete(parent);
    }
};
