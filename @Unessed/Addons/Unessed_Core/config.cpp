class CfgPatches
{
    class Unessed_Core
    {
        units[] = {};
        weapons[] = {};
        requiredVersion = 0.1;
        requiredAddons[] = {"DZ_Data", "DZ_Scripts"};
        author = "KRa Tos";
        authorUrl = "https://github.com/KRaTos/Unessed-DayZ-Mod";
        version = "1.0.0";
        fileName = "Unessed_Core.pbo";
    };
};

class CfgMods
{
    class Unessed
    {
        dir = "@Unessed";
        name = "Unessed";
        action = "https://github.com/KRaTos/Unessed-DayZ-Mod";
        author = "KRa Tos";
        version = "1.0.0";
        extra = 0;
        type = "mod";
        dependencies[] = {"Game", "World", "Mission"};

        class defs
        {
            class gameScriptModule
            {
                value = "";
                files[] = {"Unessed_Core/scripts/1_Game"};
            };
            class worldScriptModule
            {
                value = "";
                files[] = {"Unessed_Core/scripts/4_World"};
            };
            class missionScriptModule
            {
                value = "";
                files[] = {"Unessed_Core/scripts/5_Mission"};
            };
        };
    };
};
