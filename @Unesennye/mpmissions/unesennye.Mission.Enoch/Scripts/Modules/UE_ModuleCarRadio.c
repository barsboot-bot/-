// ============================================================
//  UE_ModuleCarRadio — музыка в автомобиле
//  Игрок садится в машину с магнитолой (ueCarRadio=1), включает
//  радио/плеер — звук идёт из машины и слышен всем вокруг.
//  Громкость затухает по мере удаления от автомобиля.
// ============================================================

class ActionUE_CarRadioOn: ActionContinuousBase
{
    ActionUE_CarRadioOn() { m_CallbackClass = ActionUE_CarRadioCB; m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONVEHICLE; m_Text = "Включить музыку в машине"; }
    void CreateConditionParams(ref out array<ActionConditionParam> params) { params.Insert(AliveCondition); }
    bool ConditionAction(ActionData action_data)
    {
        Car car = Car.Cast(action_data.m_target_object);
        if (!car) return false;
        int flag = GetGame().ConfigGetInt(car.GetType(), "ueCarRadio");
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
        Param2<Object, string> p = new Param2<Object, string>(veh, "Apex");
        GetRPCManager().SendRPC("UE_Network", "CmdPlayRadio", p, true, null);
        Print("[унесённые] Магнитола авто включена");
    }
};

class ActionUE_CarRadioOff: ActionContinuousBase
{
    ActionUE_CarRadioOff() { m_CallbackClass = ActionUE_CarRadioOffCB; m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONVEHICLE; m_Text = "Выключить музыку в машине"; }
    void CreateConditionParams(ref out array<ActionConditionParam> params) { params.Insert(AliveCondition); }
};

class ActionUE_CarRadioOffCB: ActionContinuousCallbackBase
{
    float timeTotal; float timePhase;
    void ActionUE_CarRadioOffCB(out ActionContinuousBase aCB) { aCB.GetDurationByPhase(timeTotal, timePhase); }
    bool OnActionEvaluate(ActionContinuousData data) { return true; }
    void OnActionB(ActionContinuousData data, float t)
    {
        if (GetGame().IsDedicated()) return;
        int id = UE_FindSourceByObject(data.m_target_object);
        if (id >= 0)
        {
            Param1<int> p = new Param1<int>(id);
            GetRPCManager().SendRPC("UE_Network", "CmdStopSource", p, true, null);
        }
    }
};

modded class ActionHandlers
{
    static ref array<ref ActionBase> CreateActionsCar(out ActionBase actions[], Object object, int slot, ItemType type)
    {
        Car car = Car.Cast(object);
        if (car && GetGame().ConfigGetInt(car.GetType(), "ueCarRadio") == 1)
        {
            actions.Insert(new ActionUE_CarRadioOn());
            actions.Insert(new ActionUE_CarRadioOff());
        }
        return actions;
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
