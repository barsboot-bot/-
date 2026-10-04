// ============================================================
//  Uness_Data — аддон конфигураций (PBO)
//  По гайдлайнам Bohemia Interactive: классы CfgPatches,
//  CfgVehicles (предметы), CfgMods. Модульные скрипты
//  объявляются в аддоне Uness_Scripts.
// ============================================================

class CfgPatches
{
    class Uness_Data
    {
        name = "Uness Data";
        author = "KRa Tos (Konstantin)";
        url = "https://github.com/KRaTos/Unessed-DayZ-Mod";
        version = "1.0.0";
        requiredVersion = 0.1;
        // ЗАЩИТА МОДА: серверный аддон KRa_TosServerInit — обязательная
        // зависимость. Если @KRa_TosServer не установлен на сервере,
        // движок не сможет загрузить этот аддон -> миссия не стартует,
        // а по истечении ueGraceSeconds сервер аварийно останавливается
        // (см. scripts/5_Mission/UE_Guard.c).
        requiredAddons[] = {"DZ_Data", "DZ_Scripts", "KRa_TosServerInit"};
        units[] = {"UE_CassettePlayer", "UE_DiskPlayer", "UE_RadioReceiver", "UE_CarRadioUnit"};
        weapons[] = {};
    };
};

// ---------- Регистрация мода (CfgMods) ----------
class CfgMods
{
    class Uness
    {
        id = "Uness";
        dir = "@Uness";
        name = "Uness";
        picture = "";
        action = "https://github.com/KRaTos/Unessed-DayZ-Mod";
        description = "Music mod for the Uness server";
        version = "1.0.0";
        author = "KRa Tos";
        authorID = "-1";
        extra = 0;
        type = "mod";
        dependencies[] = {"Game", "World", "Mission"};

        class defs
        {
            class gameScriptModule
            {
                value = "";
                files[] = {"Uness_Scripts/scripts/3_Game"};
            };
            class worldScriptModule
            {
                value = "";
                files[] = {"Uness_Scripts/scripts/4_World"};
            };
            class missionScriptModule
            {
                value = "";
                files[] = {"Uness_Scripts/scripts/5_Mission"};
            };
        };
    };
};

// ---------- Предметы ----------
class CfgVehicles
{
    class InventoryBase;
    class ItemBase;
    class Battery;

    // ================== КАССЕТНЫЙ ПЛЕЕР ==================
    class UE_CassettePlayer_Base: InventoryBase
    {
        scope = 2;
        displayName = "Кассетный плеер";
        descriptionShort = "Портативный магнитофон. Вставьте кассету и нажмите «Воспроизвести».";
        model = "\dz\gear\electronics\notepad.p3d"; // заглушка — замените на свою модель
        weight = 900;
        itemSize[] = {6, 4};
        isMeleeWeapon = 0;
        class AnimationSources
        {
            class lid
            {
                source = "user";
                animPeriod = 1;
                initPhase = 0;
            };
        };
    };

    class UE_CassettePlayer: UE_CassettePlayer_Base
    {
        displayName = "Кассетный плеер «Квант»";
    };

    // ================== ДИСКОВЫЙ ПРОИГРЫВАТЕЛЬ ==================
    class UE_DiskPlayer: UE_CassettePlayer_Base
    {
        displayName = "Проигрыватель дисков";
        descriptionShort = "Простой CD-плеер. Поддерживает виниловые и цифровые диски.";
        weight = 1200;
        itemSize[] = {7, 5};
    };

    // ================== РАДИОПРИЁМНИК ==================
    class UE_RadioReceiver: UE_CassettePlayer_Base
    {
        displayName = "Коротковолновый приёмник";
        descriptionShort = "Ловит интернет-потоки: Апекс, Европа+, Юмор FM. Требуется питание.";
        weight = 1500;
        itemSize[] = {8, 5};
    };

    // ================== АВТОМОБИЛЬНАЯ МАГНИТОЛА ==================
    class UE_CarRadioUnit: UE_CassettePlayer_Base
    {
        displayName = "Автомобильная магнитола";
        descriptionShort = "Устанавливается в автомобиль. Играет через бортовые динамики.";
        weight = 800;
        itemSize[] = {5, 3};
    };

    // ---------- Носители (расходники) ----------
    class UE_Item_Cassette_Rock: ItemBase
    {
        scope = 2;
        displayName = "Кассета: Рок-хиты 80-х";
        descriptionShort = "Аудиокассета с записью рок-композиций.";
        model = "\dz\items\magazine_rifle_556.p3d";
        weight = 100;
        itemSize[] = {4, 2};
    };

    class UE_Item_Cassette_Pop: UE_Item_Cassette_Rock
    {
        displayName = "Кассета: Поп-музыка";
    };

    class UE_Item_Disk_Classic: UE_Item_Cassette_Rock
    {
        displayName = "Диск: Классика рока";
        descriptionShort = "Компакт-диск с коллекцией классических треков.";
    };

    class UE_Item_Disk_Dance: UE_Item_Disk_Classic
    {
        displayName = "Диск: Танцевальный микс";
    };

    // ---------- Магазин-носитель для плеера ----------
    class UE_Magazine_Cassette_Rock: Battery
    {
        scope = 2;
        displayName = "Магазин: Кассета (Рок)";
        descriptionShort = "Кассета, вставленная в плеер.";
        model = "\dz\items\magazine_rifle_556.p3d";
        ammo = "UE_Track_Rock";
        count = 1;
        weight = 100;
    };

    class UE_Magazine_Cassette_Pop: UE_Magazine_Cassette_Rock
    {
        displayName = "Магазин: Кассета (Поп)";
        ammo = "UE_Track_Pop";
    };

    class UE_Magazine_Disk_Classic: UE_Magazine_Cassette_Rock
    {
        displayName = "Магазин: Диск (Классика)";
        ammo = "UE_Track_Classic";
    };
};

// ---------- Плейлисты как идентификаторы ----------
class CfgAmmo
{
    class BulletSingle;
    class UE_Track_Base: BulletSingle
    {
        model = "";
        audible = 0;
        caliber = 0.1;
        effectiveRange = 0;
        typicalSpeed = 0;
    };
    class UE_Track_Rock: UE_Track_Base { displayName = "Плейлист: Рок"; };
    class UE_Track_Pop: UE_Track_Base { displayName = "Плейлист: Поп"; };
    class UE_Track_Classic: UE_Track_Base { displayName = "Плейлист: Классика"; };
    class UE_Track_Dance: UE_Track_Base { displayName = "Плейлист: Танцы"; };
};

// ---------- Белый список радиостанций (по умолчанию в PBO) ----------
class UE_RadioStations
{
    class Apex
    {
        displayName = "Апекс ФМ";
        streamURL = "http://62.152.59.3:8000/nkz";
        genre = "Pop/Rock";
        bitrate = 64;
    };
    class EuropaPlus
    {
        displayName = "Европа Плюс";
        streamURL = "http://online-2.gkvr.ru:8000/europa_nkz_64.aac";
        genre = "Hot AC";
        bitrate = 64;
    };
    class HumorFM
    {
        displayName = "Юмор FM";
        streamURL = "http://62.231.184.253:8000/humor";
        genre = "Humor/Talk";
        bitrate = 64;
    };
};

// ---------- Параметры мода ----------
class UE_Config
{
    maxHearDistance = 150;      // метры — полная слышимость
    minVolumeDistance = 5;      // внутри этой дистанции — максимум громкости
    fadeCurve = 2;              // степень затухания (2 = квадратичное)
    radioBufferMs = 3000;       // буферизация радио-потока
    serverAuthEnabled = 1;      // серверная проверка подлинности команд
    audioBridgeEnabled = 1;     // использовать UEAudioBridge.dll (BASS)
    bridgeMasterDb = -6;        // общий уровень моста, дБ
    bridgeMusicGain = 100;      // усиление канала музыки, %

    // ========== ЗАЩИТА ОТ ЗАПУСКА БЕЗ @KRa_TosServer ==========
    // grace-период (секунды), в течение которого клиентская часть мода
    // ждёт "приветствие" (handshake) от серверного аддона
    // KRa_TosServerInit. Если handshake не получен — серверный мод
    // отсутствует / извлечён из pak / переупакован — и сервер аварийно
    // останавливается (см. scripts/5_Mission/UE_Guard.c).
    ueGraceSeconds = 30;        // 0 = graceful shutdown без принудительного краша

    // Внешняя музыкальная библиотека (без пересборки PBO):
    //   Music/Type/<Плейлист>/track.ogg|mp3|wav   <- кассеты
    //   Music/CD/<Плейлист>/track.ogg|mp3|wav     <- диски
    //   Music/Radio.txt                            <- радиостанции
    musicRoot = "Music";
    libraryBaseURL = "";        // HTTP-зеркало Music/ для клиентов
};
