class cfgMods
{
    author = "Uness Team";
    timepacked = "1728014400";
};

// Нативный аудио-мост (BASS) для DayZ Expansive.
// На клиенте размещается в: Bridges\@Uness_Bridge\UE_Bridge
// Сюда же кладутся собранные UEAudioBridge.dll (x64), bass.dll (x64) и bass_aac.dll (x64).
class CfgBridges
{
    class UE_Bridge
    {
        name = "Uness Audio Bridge (BASS)";
        version = "1.0.0";
        libraryName = "UEAudioBridge.dll"; // x64
        requiredAddons[] = {"Uness_Client"};
    };
};

// ============================================================
//  Мод «унесённые» (Uness) — клиентская часть
//  Кассеты / виниловые диски / интернет-радио / музыка в машине.
//  Звук слышат все игроки, громкость затихает с расстоянием.
// ============================================================
class CfgPatches
{
    class Uness_Client
    {
        name = "Uness Client";
        author = "Uness Team";
        url = "";
        version = "1.0.0";
        requiredVersion = 0;
        requiredAddons[] = {};
        units[] = { "UE_CassettePlayer", "UE_DiskPlayer", "UE_RadioReceiver", "UE_CarRadioUnit" };
        weapons[] = { "UE_Magazine_Cassette_Rock", "UE_Magazine_Cassette_Pop", "UE_Magazine_Disk_Classic", "UE_Item_Cassette_Rock", "UE_Item_Disk_Dance" };
    };
};

// ---------- Модель проигрывателя: затухание по расстоянию ----------
// distanceExponent 2 = физически корректное затухание 1/r^2,
// дальность слышимости ~150 м от источника звука.
class CfgSounds
{
    sounds[] = {};

    class UE_Sound_Dummy
    {
        name = "UE_Sound_Dummy";
        sound[] = { "", 1, 1 };
        // параметры затухания для всех источников мода
    };
};

class AnimationStates
{
    class UESitOnChair;
};

// ---------- Профессия/аннотации ----------
class CfgNonAIVehicles
{
    class ProxyWeapon;
};

class CfgVehicleClasses
{
    class UE_AudioEquipment
    {
        displayName = "унесённые — Аудио";
    };
};

// ---------- Классы предметов и техники ----------
class CfgVehicles
{
    class Car;
    class InventoryBase;
    class ItemBase;
    class Battery;

    // ================== КАССЕТНЫЙ ПЛЕЕР ==================
    class UE_CassettePlayer_Base: InventoryBase
    {
        scope = 2;
        displayName = "Кассетный плеер";
        descriptionShort = "Портативный магнитофон. Вставьте кассету и нажмите «Воспроизвести».";
        model = "\dz\gear\electronics\notepad.p3d"; // заглушка-модель, замените на свою
        weight = 900;
        itemSize[] = {6, 4};
        allowedToAccessInProxy = 0;
        canBeTakenFromGround = 1;
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

    // ================== РАДИО ПРИЁМНИК ==================
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

    // ---------- Расходники ----------
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

    // ---------- Наследование транспорта (магнитола в машине) ----------
    class OffroadHatchback;
    class Hatchback_02;
    class Tractor;
    class Session_01;

    class UE_OffroadHatchback_Radio: OffroadHatchback
    {
        displayName = "Offroad Hatchback (с магнитолой)";
        ueCarRadio = 1;   // флаг наличия авто-радио (читается скриптом)
    };
    class UE_Hatchback_02_Radio: Hatchback_02
    {
        displayName = "Hatchback 02 (с магнитолой)";
        ueCarRadio = 1;
    };
    class UE_Tractor_Radio: Tractor
    {
        displayName = "Tractor (с магнитолой)";
        ueCarRadio = 1;
    };
    class UE_Session_01_Radio: Session_01
    {
        displayName = "Session 1 (с магнитолой)";
        ueCarRadio = 1;
    };
};

// ---------- Боеприпасы как идентификаторы плейлистов ----------
class CfgAmmo
{
    class BulletSingle;
    class UE_Track_Base: BulletSingle
    {
        model = "";
        audible = 0;
        caliber = 0;
        effectiveRange = 0;
        typicalSpeed = 0;
    };
    class UE_Track_Rock: UE_Track_Base { displayName = "Плейлист: Рок"; };
    class UE_Track_Pop: UE_Track_Base { displayName = "Плейлист: Поп"; };
    class UE_Track_Classic: UE_Track_Base { displayName = "Плейлист: Классика"; };
    class UE_Track_Dance: UE_Track_Base { displayName = "Плейлист: Танцы"; };
};

// ---------- Список доступных интернет-радиостанций ----------
// Это БЕЛЫЙ СПИСОК по умолчанию (зашит в PBO). Админ может
// добавлять станции без перупаковки через внешний файл
// <Profile>/Music/Radio.txt — см. UE_Config musicRoot ниже.
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

// ---------- Пользовательские переменные мода ----------
class UE_Config
{
    maxHearDistance = 150;      // метры — полная слышимость
    minVolumeDistance = 5;      // внутри этой дистанции — максимум громкости
    fadeCurve = 2;              // степень затухания (2 = квадратичное)
    radioBufferMs = 3000;       // буферизация радио-потока
    serverAuthEnabled = 1;      // включать серверную проверку подлинности команд
    audioBridgeEnabled = 1;     // использовать аудио-прослойку UEAudioBridge.dll (BASS):
                                // 1 = стримы и внешние треки, 0 = только штатный SoundSource
    bridgeMasterDb = -6;        // общий уровень моста, дБ
    bridgeMusicGain = 100;      // усиление канала музыки, %

    // ========== ВНЕШНЯЯ МУЗЫКАЛЬНАЯ БИБЛИОТЕКА (без PBO) ==========
    // Корень библиотеки. По умолчанию создаётся в профиле сервера:
    //   <DayZ Server>/serverDZ/profiles/<имя профиля>/Music/
    // Можно указать "~" (home клиента), абсолютный путь "/srv/music"
    // или "~@Uness/Music". Структура:
    //   Music/Type/<ПапкаПлейлиста>/track.ogg|mp3|wav   <- кассеты
    //   Music/CD/<ПапкаПлейлиста>/track.ogg|mp3|wav     <- диски
    //   Music/Radio.txt                                  <- радиостанции
    // Каждая папка-плейлист может содержать meta.txt: name=Отображаемое имя
    musicRoot = "Music";

    // HTTP-зеркало той же папки Music для клиентов (модам не нужен RPT-доступ
    // к файловой системе сервера — клиенты докачивают треки сами).
    // Пример: http://my-server.example:8080/Music/
    // Пусто = докачка выключена (играют только уже скачанные/встроенные треки).
    libraryBaseURL = "";
};
