// ============================================================
//  MusicRadioCore — аддон конфигураций (PBO)
//  По гайдлайнам Bohemia Interactive: классы CfgPatches,
//  CfgVehicles (предметы), CfgMods. Модульные скрипты
//  объявляются в аддоне MusicRadioScripts.
// ============================================================

class CfgPatches
{
    class MusicRadioCore
    {
        name = "MusicRadio Data";
        author = "KRa Tos (Konstantin)";
        url = "https://github.com/KRaTos/Unessed-DayZ-Mod";
        version = "1.1.0";
        requiredVersion = 0.1;
        // ЗАЩИТА МОДА: серверный аддон MusicRadioServerInit — обязательная
        // зависимость. Если @MusicRadioServer не установлен на сервере,
        // движок не сможет загрузить этот аддон -> миссия не стартует,
        // а по истечении mrGraceSeconds сервер аварийно останавливается
        // (см. scripts/5_Mission/MR_Guard.c).
        requiredAddons[] = {"DZ_Data", "DZ_Scripts", "MusicRadioServerInit"};
        units[] = {"MR_CassettePlayer", "MR_DiskPlayer", "MR_RadioReceiver", "MR_CarRadioUnit",
                   "MR_Item_Cassette_Rock", "MR_Item_Cassette_Pop",
                   "MR_Item_MiniDisc_Rock", "MR_Item_MiniDisc_Pop",
                   "MR_Item_MiniDisc_Classic", "MR_Item_MiniDisc_Dance",
                   "MR_Item_MemoryCard"};
        weapons[] = {};
    };
};

// ---------- Регистрация мода (CfgMods) ----------
class CfgMods
{
    class MusicRadio
    {
        id = "MusicRadio";
        dir = "@MusicRadioCore";
        name = "MusicRadio";
        picture = "";
        action = "https://github.com/KRaTos/Unessed-DayZ-Mod";
        description = "Music mod for the MusicRadio server";
        version = "1.1.0";
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
                files[] = {"MusicRadioScripts/scripts/3_Game"};
            };
            class worldScriptModule
            {
                value = "";
                files[] = {"MusicRadioScripts/scripts/4_World"};
            };
            class missionScriptModule
            {
                value = "";
                files[] = {"MusicRadioScripts/scripts/5_Mission"};
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
    class MR_CassettePlayer_Base: InventoryBase
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

    class MR_CassettePlayer: MR_CassettePlayer_Base
    {
        displayName = "Кассетный плеер «Квант»";
    };

    // ================== ДИСКОВЫЙ ПРОИГРЫВАТЕЛЬ ==================
    class MR_DiskPlayer: MR_CassettePlayer_Base
    {
        displayName = "Проигрыватель дисков";
        descriptionShort = "Простой CD-плеер. Поддерживает виниловые и цифровые диски.";
        weight = 1200;
        itemSize[] = {7, 5};
    };

    // ================== РАДИОПРИЁМНИК ==================
    class MR_RadioReceiver: MR_CassettePlayer_Base
    {
        displayName = "Коротковолновый приёмник";
        descriptionShort = "Ловит интернет-потоки: Апекс, Европа+, Юмор FM. Требуется питание.";
        weight = 1500;
        itemSize[] = {8, 5};
    };

    // ================== АВТОМОБИЛЬНАЯ МАГНИТОЛА ==================
    class MR_CarRadioUnit: MR_CassettePlayer_Base
    {
        displayName = "Автомобильная магнитола";
        descriptionShort = "Устанавливается в автомобиль. Играет через бортовые динамики.";
        weight = 800;
        itemSize[] = {5, 3};
    };

    // ==================================================================
    //  Носители музыки.
    //  Правило мода: один носитель = один плейлист, внутри предмета звука
    //  нет — треки лежат во внешней библиотеке Music/ (Music/Type/<плейлист>
    //  для кассет, Music/CD/<плейлист> для дисков) и подтягиваются по ключу
    //  `uePlaylist`. Поэтому новый альбом = новый класс + обложка, без
    //  пересборки аудио.
    //
    //  Модели:  Models/src/*.obj -> .p3d (Object Builder), габариты и
    //           имена селекшенов — Models/README.md, порядок — docs/17_ASSETS.md
    //  Текстуры: Models/tex_src/*.png -> .paa (TexView 2)
    //  Материалы: MusicRadioCore\data\tex\*.rvmat (назначаются в Object Builder)
    // ==================================================================
    class MR_Item_Carrier_Base: ItemBase
    {
        scope = 0;                      // базовый класс: в игре не появляется
        displayName = "Носитель";
        descriptionShort = "Носитель музыки. Вставьте в плеер.";
        weight = 40;
        itemSize[] = {2, 2};
        // `label` — имя селекшена на модели (этикетка). Материал подменяется
        // из конфига альбома: меш один и тот же, обложка — у каждого своя.
        hiddenSelections[] = {"label"};
        hiddenSelectionsMaterials[] = {"MusicRadioCore\data\tex\mr_cassette.rvmat"};
    };

    // ---------------------- аудиокассета ----------------------
    class MR_Item_Cassette_Base: MR_Item_Carrier_Base
    {
        scope = 0;
        displayName = "Аудиокассета";
        model = "MusicRadioCore\data\mr_cassette.p3d";   // 100 x 63.8 x 12 мм
        weight = 40;                    // грамм (реальная кассета ~40 г)
        itemSize[] = {2, 2};
    };

    class MR_Item_Cassette_Rock: MR_Item_Cassette_Base
    {
        scope = 2;
        displayName = "Кассета: Рок-хиты 80-х";
        descriptionShort = "Аудиокассета TYPE I, сторона A. Вставьте в кассетный плеер.";
        uePlaylist = "Rock";            // -> Music/Type/Rock на сервере
        hiddenSelectionsMaterials[] = {"MusicRadioCore\data\tex\mr_cassette_rock.rvmat"};
    };

    class MR_Item_Cassette_Pop: MR_Item_Cassette_Rock
    {
        displayName = "Кассета: Поп-музыка";
        descriptionShort = "Аудиокассета TYPE I, сторона A. Вставьте в кассетный плеер.";
        uePlaylist = "Pop";
        hiddenSelectionsMaterials[] = {"MusicRadioCore\data\tex\mr_cassette_pop.rvmat"};
    };

    // ---------------------- MiniDisc ----------------------
    class MR_Item_MiniDisc_Base: MR_Item_Carrier_Base
    {
        scope = 0;
        displayName = "MiniDisc";
        model = "MusicRadioCore\data\mr_minidisc.p3d";   // 68 x 72 x 5 мм
        weight = 30;
        itemSize[] = {1, 2};
        hiddenSelectionsMaterials[] = {"MusicRadioCore\data\tex\mr_minidisc.rvmat"};
    };

    class MR_Item_MiniDisc_Rock: MR_Item_MiniDisc_Base
    {
        scope = 2;
        displayName = "MD: Рок-хиты 80-х";
        descriptionShort = "Перезаписываемый MiniDisc на 74 минуты.";
        uePlaylist = "Rock";
        hiddenSelectionsMaterials[] = {"MusicRadioCore\data\tex\mr_minidisc_rock.rvmat"};
    };

    class MR_Item_MiniDisc_Pop: MR_Item_MiniDisc_Base
    {
        scope = 2;
        displayName = "MD: Поп-музыка";
        descriptionShort = "Перезаписываемый MiniDisc на 74 минуты.";
        uePlaylist = "Pop";
        hiddenSelectionsMaterials[] = {"MusicRadioCore\data\tex\mr_minidisc_pop.rvmat"};
    };

    class MR_Item_MiniDisc_Classic: MR_Item_MiniDisc_Base
    {
        scope = 2;
        displayName = "MD: Классика рока";
        descriptionShort = "Перезаписываемый MiniDisc на 74 минуты.";
        uePlaylist = "Classic";         // -> Music/CD/Classic
        hiddenSelectionsMaterials[] = {"MusicRadioCore\data\tex\mr_minidisc_classic.rvmat"};
    };

    class MR_Item_MiniDisc_Dance: MR_Item_MiniDisc_Base
    {
        scope = 2;
        displayName = "MD: Танцевальный микс";
        descriptionShort = "Перезаписываемый MiniDisc на 74 минуты.";
        uePlaylist = "Dance";
        hiddenSelectionsMaterials[] = {"MusicRadioCore\data\tex\mr_minidisc_dance.rvmat"};
    };

    // ---------------------- компакт-диск ----------------------
    // Модель диска — отдельная задача: пока используется ванильная заглушка,
    // геометрия и текстуры делаются ровно так же, как для кассеты и MD.
    class MR_Item_Disk_Base: MR_Item_Carrier_Base
    {
        scope = 0;
        displayName = "Компакт-диск";
        model = "\dz\items\magazine_rifle_556.p3d";  // ЗАГЛУШКА до своей модели
        weight = 15;
        itemSize[] = {2, 2};
    };

    class MR_Item_Disk_Classic: MR_Item_Disk_Base
    {
        scope = 2;
        displayName = "Диск: Классика рока";
        descriptionShort = "Компакт-диск с коллекцией классических треков.";
        uePlaylist = "Classic";
    };

    class MR_Item_Disk_Dance: MR_Item_Disk_Classic
    {
        displayName = "Диск: Танцевальный микс";
        uePlaylist = "Dance";
    };

    // ---------------------- карта памяти ----------------------
    class MR_Item_MemoryCard_Base: MR_Item_Carrier_Base
    {
        scope = 0;
        displayName = "Карта памяти";
        model = "MusicRadioCore\data\mr_memorycard.p3d"; // 32 x 24 x 2.1 мм
        weight = 10;
        itemSize[] = {1, 1};
        hiddenSelectionsMaterials[] = {"MusicRadioCore\data\tex\mr_memorycard.rvmat"};
    };

    class MR_Item_MemoryCard: MR_Item_MemoryCard_Base
    {
        scope = 2;
        displayName = "Карта памяти 8 GB";
        descriptionShort = "SD-карта: ключ плеера. Музыка на ней не хранится, плейлист задаёт вставленный носитель.";
    };

    // ---------- Магазин-носитель для плеера ----------
    class MR_Magazine_Cassette_Rock: Battery
    {
        scope = 2;
        displayName = "Магазин: Кассета (Рок)";
        descriptionShort = "Кассета, вставленная в плеер.";
        model = "MusicRadioCore\data\mr_cassette.p3d";
        ammo = "MR_Track_Rock";
        count = 1;
        weight = 100;
    };

    class MR_Magazine_Cassette_Pop: MR_Magazine_Cassette_Rock
    {
        displayName = "Магазин: Кассета (Поп)";
        ammo = "MR_Track_Pop";
    };

    class MR_Magazine_Disk_Classic: MR_Magazine_Cassette_Rock
    {
        displayName = "Магазин: Диск (Классика)";
        ammo = "MR_Track_Classic";
    };
};

// ---------- Плейлисты как идентификаторы ----------
class CfgAmmo
{
    class BulletSingle;
    class MR_Track_Base: BulletSingle
    {
        model = "";
        audible = 0;
        caliber = 0.1;
        effectiveRange = 0;
        typicalSpeed = 0;
    };
    class MR_Track_Rock: MR_Track_Base { displayName = "Плейлист: Рок"; };
    class MR_Track_Pop: MR_Track_Base { displayName = "Плейлист: Поп"; };
    class MR_Track_Classic: MR_Track_Base { displayName = "Плейлист: Классика"; };
    class MR_Track_Dance: MR_Track_Base { displayName = "Плейлист: Танцы"; };
};

// ---------- Белый список радиостанций (по умолчанию в PBO) ----------
class MR_RadioStations
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
class MR_Config
{
    maxHearDistance = 150;      // метры — полная слышимость
    minVolumeDistance = 5;      // внутри этой дистанции — максимум громкости
    fadeCurve = 2;              // степень затухания (2 = квадратичное)
    radioBufferMs = 3000;       // буферизация радио-потока
    serverAuthEnabled = 1;      // серверная проверка подлинности команд
    audioBridgeEnabled = 1;     // использовать UEAudioBridge.dll (BASS)
    bridgeMasterDb = -6;        // общий уровень моста, дБ
    bridgeMusicGain = 100;      // усиление канала музыки, %

    // ========== ЗАЩИТА ОТ ЗАПУСКА БЕЗ @MusicRadioServer ==========
    // grace-период (секунды), в течение которого клиентская часть мода
    // ждёт "приветствие" (handshake) от серверного аддона
    // MusicRadioServerInit. Если handshake не получен — серверный мод
    // отсутствует / извлечён из pak / переупакован — и сервер аварийно
    // останавливается (см. scripts/5_Mission/MR_Guard.c).
    mrGraceSeconds = 30;        // 0 = graceful shutdown без принудительного краша

    // Внешняя музыкальная библиотека (без пересборки PBO):
    //   Music/Type/<Плейлист>/track.ogg|mp3|wav   <- кассеты
    //   Music/CD/<Плейлист>/track.ogg|mp3|wav     <- диски
    //   Music/Radio.txt                            <- радиостанции
    musicRoot = "Music";
    libraryBaseURL = "";        // HTTP-зеркало Music/ для клиентов
};
