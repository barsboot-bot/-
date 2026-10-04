// ============================================================
//  Uness_Scripts — скриптовый аддон (Enforce Script)
//  Структура каталогов по стандарту Bohemia Interactive:
//    scripts/3_Game   — глобальные игровые определения
//    scripts/4_World  — классы предметов и экшены (worldScriptModule)
//    scripts/5_Mission— сетевой слой, менеджеры (missionScriptModule)
//  Регистрация модулей — в CfgMods аддона Uness_Data.
// ============================================================

class CfgPatches
{
    class Uness_Scripts
    {
        name = "Uness Scripts";
        author = "KRa Tos (Konstantin)";
        url = "https://github.com/KRaTos/Unessed-DayZ-Mod";
        version = "1.0.0";
        requiredVersion = 0.1;
        // защита мода: прямая зависимость от серверного аддона —
        // без @KRa_TosServer скрипты не загрузятся даже при обходе Data
        requiredAddons[] = {"DZ_Data", "DZ_Scripts", "Uness_Data", "KRa_TosServerInit"};
        units[] = {};
        weapons[] = {};
    };
};
