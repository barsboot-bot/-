class cfgMods
{
    author = "Uness Team";
    timepacked = "1728014400";
};

// Конфигурация модпака «унесённые» (серверная часть)
class CfgPatches
{
    class Uness_Server
    {
        name = "Uness Server";
        author = "Uness Team";
        url = "";
        version = "1.0.0";
        requiredVersion = 0;
        requiredAddons[] = {};
        units[] = {};
        weapons[] = {};
    };
};

// ============================================================
//  ЗАЩИТА МОДА:
//  1) checkModLoad — сервер принудительно проверяет наличие
//     клиентского мода @Uness. Игрок без мода не сможет
//     подключиться к серверу.
//  2) Блокировка сторонних скриптовых акселераторов/читов,
//     которые могут подменять игровые события.
//  3) Лимиты на загрузку миссии и синхронизацию объектов.
// ============================================================
class CfgRemoteExec
{
    class Functions
    {
        // Разрешаем ONLY наш сетевой RPC-модуль.
        // Всё остальное блокируется (защита от REMOTE_EXEC инъекций).
        class F_UE_Network_RPC
        {
            allowed = "true";
            jip = "true";
        };
        class F_UE_Secure_Handshake
        {
            allowed = "true";
            jip = "false";
        };
        // Внешняя музыкальная библиотека: рассылка манифеста
        // (плейлисты Music/Type, Music/CD, станции Music/Radio.txt)
        class F_UE_Library_Manifest
        {
            allowed = "true";
            jip = "false";
        };
    };
};

class CfgServerProblemDetector
{
    framesLimit = 500;
    overlapMin = 0;
    overlapMax = 90;
    simulationStallMin = 1;
    simulationStallMax = 10;
};

class CfgSCA
{
    class Default
    {
        monitorEnabled = 1;
        serverWarningTimeout = 10000;
        screenshotUploadTimeout = 10000;
        screenshotRequestTimeout = 60000;
        banOnFailedScreenshot = 1;
        maxConcurrentRequests = 3;
    };
};

class CfgVars
{
    fileSizeLimit = 33554432;          // 32 МБ максимум на файл миссии
    missionTotalFileSize = 134217728;   // 128 МБ суммарно

    // --- Анти-чит / защита ---
    checkModLoad = 1;                   // ОБЯЗАТЕЛЬНОЕ присутствие мода у клиента
    verifySignatures = 1;               // проверка подписей mod.pbo (@Uness, @KRa_TosServer)
    battlEyeFilters = 1;
    maxPacketSize = 1400;
    networkIdleTimeout = 300;
    persistentMode = 1;

    // --- Плейеры ---
    onlyAdminsCanCarryWeapons = 0;
    anonymousRCONLoginsAllowed = 0;
    serverCommandFilterEnabled = 1;
    disableThirdPerson = 0;

    // --- Голосовой чат (для радио-стрима используется свой канал) ---
    voiceChatRange = 25;

    // --- Таймауты синхронизации проигрывателя ---
    syncMinThresholdForPlayerTeleportation = 20;
    syncMinThresholdForObjectTeleportation = 100;
};
