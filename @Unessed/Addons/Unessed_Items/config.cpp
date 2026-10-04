class CfgPatches
{
    class Unessed_Items
    {
        units[] = {};
        weapons[] = {};
        requiredVersion = 0.1;
        requiredAddons[] = {"DZ_Data", "DZ_Scripts", "Unessed_Core"};
        author = "KRa Tos";
        version = "1.0.0";
        fileName = "Unessed_Items.pbo";
    };
};

class CfgVehicles
{
    // Аудиокассета
    class Inventory_Base;
    class Unessed_AudioTape: Inventory_Base
    {
        scope = 2;
        displayName = "$STR_Unessed_AudioTape";
        descriptionShort = "$STR_Unessed_AudioTape_Desc";
        model = "\Unessed_Items\models\cassette.p3d";
        weight = 50;
        itemSize[] = {2,1};
        class DamageSystem
        {
            class GlobalHealth
            {
                class Health
                {
                    hitpoints = 100;
                };
            };
        };
    };

    // Виниловый диск
    class Unessed_VinylDisc: Inventory_Base
    {
        scope = 2;
        displayName = "$STR_Unessed_VinylDisc";
        descriptionShort = "$STR_Unessed_VinylDisc_Desc";
        model = "\Unessed_Items\models\vinyl.p3d";
        weight = 150;
        itemSize[] = {3,3};
        class DamageSystem
        {
            class GlobalHealth
            {
                class Health
                {
                    hitpoints = 100;
                };
            };
        };
    };

    // Кассетный плеер
    class Unessed_CassettePlayer: Inventory_Base
    {
        scope = 2;
        displayName = "$STR_Unessed_CassettePlayer";
        descriptionShort = "$STR_Unessed_CassettePlayer_Desc";
        model = "\Unessed_Items\models\player.p3d";
        weight = 500;
        itemSize[] = {3,2};
        attachments[] = {"Unessed_AudioTape"};
        class DamageSystem
        {
            class GlobalHealth
            {
                class Health
                {
                    hitpoints = 200;
                };
            };
        };
    };
};

class CfgNonAIVehicles
{
    class Radio;
    class Unessed_RadioReceiver: Radio
    {
        scope = 2;
        displayName = "$STR_Unessed_RadioReceiver";
        descriptionShort = "$STR_Unessed_RadioReceiver_Desc";
        model = "\Unessed_Items\models\radio.p3d";
        weight = 300;
        itemSize[] = {2,2};
    };
};
