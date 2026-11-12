// ============================================================
//  MusicRadioScripts — скриптовый аддон (Enforce Script)
//  Структура каталогов по стандарту Bohemia Interactive:
//    scripts/3_Game   — глобальные игровые определения
//    scripts/4_World  — классы предметов и экшены (worldScriptModule)
//    scripts/5_Mission— сетевой слой, менеджеры (missionScriptModule)
//  Регистрация модулей — в CfgMods аддона MusicRadioCore.
// ============================================================

class CfgPatches
{
    class MusicRadioScripts
    {
        name = "MusicRadio Scripts";
        author = "KRa Tos (Konstantin)";
        url = "https://github.com/KRaTos/Unessed-DayZ-Mod";
        version = "1.1.0";
        requiredVersion = 0.1;
        // защита мода: прямая зависимость от серверного аддона —
        // без @MusicRadioServer скрипты не загрузятся даже при обходе Data
        requiredAddons[] = {"DZ_Data", "DZ_Scripts", "MusicRadioCore", "MusicRadioServerInit"};
        units[] = {};
        weapons[] = {};
    };
};
