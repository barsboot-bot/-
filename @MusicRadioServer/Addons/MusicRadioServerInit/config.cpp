// ============================================================
//  MusicRadioServerInit — серверный аддон мода MusicRadio
//  Обязателен: без него сервер не поднимет аудио-RPC.
//  Проверяет, что у всех подключающихся игроков установлен
//  клиентский мод (по наличию обработчиков MR_Network).
// ============================================================

class CfgPatches
{
    class MusicRadioServerInit
    {
        name = "MusicRadio Server Init";
        author = "KRa Tos (Konstantin)";
        url = "https://github.com/KRaTos/Unessed-DayZ-Mod";
        version = "1.1.0";
        requiredVersion = 0.1;
        requiredAddons[] = {"DZ_Data", "DZ_Scripts"};
        units[] = {};
        weapons[] = {};
    };
};

// ---------- Регистрация скриптового модуля серверного аддона ----------
// Скрипты MR_GuardServer.c (5_Mission) компилируются как отдельный
// missionScriptModule — по стандарту BI «Creating a mod».
class CfgMods
{
    class MusicRadioServer
    {
        id = "MusicRadioServer";
        dir = "@MusicRadioServer";
        name = "MusicRadio Server";
        picture = "";
        action = "https://github.com/KRaTos/Unessed-DayZ-Mod";
        description = "Server-side guard addon for the MusicRadio music mod";
        version = "1.1.0";
        author = "KRa Tos";
        extra = 0;
        type = "mod";
        dependencies[] = {"Mission"};

        class defs
        {
            class missionScriptModule
            {
                value = "";
                files[] = {"MusicRadioServerInit/scripts/5_Mission"};
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
        class F_MR_Network_RPC
        {
            allowed = 1;
            jip = 1;
        };
        class F_MR_Library_Manifest
        {
            allowed = 1;
            jip = 0;
        };
        // handshake защиты: сервер -> все клиенты (JIP обязателен,
        // чтобы поздние подключения тоже получили приветствие)
        class F_MR_Guard_Handshake
        {
            allowed = 1;
            jip = 1;
        };
    };
};
