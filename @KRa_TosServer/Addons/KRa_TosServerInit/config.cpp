// ============================================================
//  KRa_TosServerInit — серверный аддон мода «унесённые»
//  Обязателен: без него сервер не поднимет аудио-RPC.
//  Проверяет, что у всех подключающихся игроков установлен
//  клиентский мод (по наличию обработчиков UE_Network).
// ============================================================

class CfgPatches
{
    class KRa_TosServerInit
    {
        name = "KRa Tos Server Init";
        author = "KRa Tos (Konstantin)";
        url = "https://github.com/KRaTos/Unessed-DayZ-Mod";
        version = "1.0.0";
        requiredVersion = 0.1;
        requiredAddons[] = {"DZ_Data", "DZ_Scripts"};
        units[] = {};
        weapons[] = {};
    };
};

// ---------- Регистрация скриптового модуля серверного аддона ----------
// Скрипты UE_GuardServer.c (5_Mission) компилируются как отдельный
// missionScriptModule — по стандарту BI «Creating a mod».
class CfgMods
{
    class KRa_TosServer
    {
        id = "KRa_TosServer";
        dir = "@KRa_TosServer";
        name = "Uness Server";
        picture = "";
        action = "https://github.com/KRaTos/Unessed-DayZ-Mod";
        description = "Server-side guard addon for the Uness music mod";
        version = "1.0.0";
        author = "KRa Tos";
        extra = 0;
        type = "mod";
        dependencies[] = {"Mission"};

        class defs
        {
            class missionScriptModule
            {
                value = "";
                files[] = {"KRa_TosServerInit/scripts/5_Mission"};
            };
        };
    };
};

// ---------- Разрешённый удалённый исполняемый код ----------
// Сервер принимает ТОЛЬКО наши RPC-функции; всё остальное
// блокируется движком (защита от RemoteExec-инъекций).
class CfgRemoteExec
{
    class Functions
    {
        class F_UE_Network_RPC
        {
            allowed = 1;
            jip = 1;
        };
        class F_UE_Library_Manifest
        {
            allowed = 1;
            jip = 0;
        };
        // handshake защиты: сервер -> все клиенты (JIP обязателен,
        // чтобы поздние подключения тоже получили приветствие)
        class F_UE_Guard_Handshake
        {
            allowed = 1;
            jip = 1;
        };
    };
};
