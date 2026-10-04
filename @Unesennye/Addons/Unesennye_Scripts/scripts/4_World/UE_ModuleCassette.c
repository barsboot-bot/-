// ============================================================
//  UE_ModuleCassette — кассетный плеер (предмет в мире)
//  Игрок вставляет кассету, нажимает «Воспроизвести» — запрос
//  уходит на сервер, проходит проверку и играет ВСЕМ рядом.
// ============================================================

modded class ActionInsertCassette
{
    // базовая акция из DayZ: добавляем свой хук после установки
}

class ActionUE_PlayCassette: ActionContinuousBase
{
    ActionUE_PlayCassette()
    {
        m_CallbackClass = ActionUE_PlayCassetteCB;
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_EMOTEATEND;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_CROUCH | DayZPlayerConstants.STANCEMASK_ERECT;
        m_FullAction = "full";
        m_Text = "Включить музыку (кассета)";
    }

    void CreateConditionParams(out array<ActionConditionParam> params)
    {
        params.Insert(AliveCondition);
        params.Insert(HasAlsoRequiredItems);
        params.Insert(IsNotBusyCondition);
    }

    bool ConditionAction(ActionData action_data)
    {
        // нужен сам плеер + кассета в инвентаре
        return HaveInInventory(action_data.m_main_player, "UE_Item_Cassette_Rock") ||
               HaveInInventory(action_data.m_main_player, "UE_Item_Cassette_Pop");
    }

    private bool HaveInInventory(PlayerBase pl, string cls)
    {
        if (!pl || !pl.GetInventory()) return false;
        return pl.GetInventory().FindEntity(cls) != null;
    }
};

class ActionUE_PlayCassetteCB: ActionContinuousCallbackBase
{
    float timeTotal;
    float timePhase;

    void ActionUE_PlayCassetteCB(out ActionContinuousBase aCB)
    {
        aCB.GetDurationByPhase(timeTotal, timePhase);
        aCB.SetPhaseVisible(0, true);   // фаза 1 — вставить кассету
        aCB.SetPhaseVisible(1, true);   // фаза 2 — включить
    }

    bool OnActionEvaluate(ActionContinuousData data)
    {
        return true;
    }

    void OnActionA(ActionContinuousData data, float t)
    {
        // анимация вставки кассеты
    }

    void OnActionB(ActionContinuousData data, float t)
    {
        if (GetGame().IsDedicated()) return;
        PlayerBase pl = data.m_player;
        if (!pl || !pl.GetInventory()) return;
        EntityAI cassette = pl.GetInventory().FindEntity("UE_Item_Cassette_Rock");
        if (!cassette) cassette = pl.GetInventory().FindEntity("UE_Item_Cassette_Pop");
        if (!cassette) return;

        string playlist = cassette.GetType() == "UE_Item_Cassette_Rock" ? "Rock" : "Pop";
        // источник звука привязываем к самому плееру игрока (он же и носитель)
        Object playerObj = pl.GetInventory().FindEntity("UE_CassettePlayer");
        if (!playerObj) return;

        // отправляем запрос НА СЕРВЕР (там всё проверит UE_Security)
        Param2<Object, string> p = new Param2<Object, string>(playerObj, playlist);
        GetRPCManager().SendRPC("UE_Network", "CmdPlayCassette", p, true, null);
        Print("[унесённые] Запрос на воспроизведение кассеты: " + playlist);
    }
};

// регистрация акций у предмета-плеера
modded class UE_CassettePlayer
{
    void UE_CassettePlayer()
    {
        // конструктор предмета
    }

    override void EEInit()
    {
        super.EEInit();
    }
};

modded class ActionBuilders
{
    static void AddActionUE_CassettePlayer(ref array<ActionBase> actions)
    {
        actions.Insert(new ActionUE_PlayCassette());
    }
};
