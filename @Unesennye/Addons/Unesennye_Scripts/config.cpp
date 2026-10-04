// ============================================================
//  Unesennye_Scripts — скриптовый аддон (Enforce Script)
//  Структура каталогов по стандарту Bohemia Interactive:
//    scripts/3_Game   — глобальные игровые определения
//    scripts/4_World  — классы предметов и экшены (worldScriptModule)
//    scripts/5_Mission— сетевой слой, менеджеры (missionScriptModule)
//  Регистрация модулей — в CfgMods аддона Unesennye_Data.
// ============================================================

class CfgPatches
{
    class Unesennye_Scripts
    {
        name = "Unesennye Scripts";
        author = "KRa Tos (Konstantin)";
        url = "https://github.com/KRaTos/Unessed-DayZ-Mod";
        version = "1.0.0";
        requiredVersion = 0.1;
        // защита мода: прямая зависимость от серверного аддона —
        // без @UnesennyeServer скрипты не загрузятся даже при обходе Data
        requiredAddons[] = {"DZ_Data", "DZ_Scripts", "Unesennye_Data", "Unesennye_ServerInit"};
        units[] = {};
        weapons[] = {};
    };
};
