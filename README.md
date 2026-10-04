# 📼 унесённые | Uness — мод музыки для DayZ Standalone

**Версия:** 1.0.0 · **Автор:** KRa Tos (Константин) · **Лицензия:** только сервер «Унесённые»

> **Моды переименованы:** клиентский мод — **@Uness**, серверный — **@KRa_TosServer**
> (аддоны: `Uness_Data.pbo`, `Uness_Scripts.pbo`, `KRa_TosServerInit.pbo`).

Атмосферный музыкальный мод: аудиокассеты, виниловые диски, интернет-радио и
магнитолы в автомобилях. Звук реалистично затухает с расстоянием — его слышат
все игроки рядом с источником.

Проект собран по официальным гайдлайнам Bohemia Interactive / DayZ wiki
(«Creating a mod», структура `@Mod/Addons/*.pbo`, `CfgPatches`, `CfgMods`,
Enforce Script модули `3_Game / 4_World / 5_Mission`).

---

## 📁 Структура репозитория

```
├── @Uness/                  # КЛИЕНТ + СЕРВЕР (мод-пак)
│   ├── Addons/
│   │   ├── Uness_Data/      # config.cpp: CfgPatches, CfgMods, CfgVehicles,
│   │   │                        #   UE_RadioStations, UE_Config
│   │   └── Uness_Scripts/   # Enforce Script:
│   │       └── scripts/
│   │           ├── 3_Game/UE_Global.c          # константы (UE_SourceType)
│   │           ├── 4_World/UE_Module*.c        # экшены предметов (world module)
│   │           └── 5_Mission/                  # миссия-модуль:
│   │               ├── MissionServer.c         # точка входа (modded MissionServer)
│   │               ├── UE_AudioManager.c       # ядро звука + затухание
│   │               ├── UE_Network.c            # RPC-слой (GetRPCManager)
│   │               ├── UE_MusicLibrary.c       # внешняя библиотека Music/
│   │               └── UE_Security.c           # серверная валидация команд
│   │       └── 5_Mission/UE_Guard.c            # защита: краш без @KRa_TosServer
│   ├── Bridges/@Uness_Bridge/              # нативный BASS-мост (опционально)
│   ├── Keys/uness.bikey                      # публичный ключ подписи
│   └── config.cpp                              # корневой cfgMods/CfgBridges
├── @KRa_TosServer/            # СЕРВЕРНАЯ часть
│   ├── Addons/KRa_TosServerInit/            # CfgRemoteExec allow-list + UE_GuardServer.c (handshake)
│   ├── Music/                                  # внешняя библиотека (без PBO!)
│   │   ├── Type/<плейлист>/track.ogg|mp3|wav   # кассеты (+ meta.txt: name=...)
│   │   ├── CD/<плейлист>/...                   # диски
│   │   └── Radio.txt                           # станции: Название = URL
│   └── tools/serve_music.py                    # HTTP-зеркало Music/ для клиентов
├── tools/pack_pbo.py            # fallback-упаковщик PBO (без DayZ Tools)
└── build.ps1                    # сборочный скрипт (Addon Builder → pack_pbo)
```

## 🛠 Сборка (по BI-гайду)

1. Установите **DayZ Tools** (Steam) → **Addon Builder**.
2. Sources: папка `@Uness` (или `@KRa_TosServer`), Keys: ваш `.biprivatekey`.
3. Build → получаются `Uness_Data.pbo`, `Uness_Scripts.pbo`,
   `KRa_TosServerInit.pbo` в `Addons/`.
4. Без DayZ Tools (только для тестов, PBO не подписаны):
   `powershell -File build.ps1` или `python3 tools/pack_pbo.py <staging> <out>`.

## 🚀 Установка на сервер

```cpp
// serverDZ.cfg
class Mods
{
    class Uness { dir = "@Uness";       name = "Uness"; };
    class KRa_TosServer { dir = "@KRa_TosServer"; name = "Uness Server"; };
};
```

* Положите собранные `.pbo` в `<@mod>/Addons/`, `.bikey` — в `keys/` сервера.
* Внешняя музыка кладется в `<profile>/Music/` — пересборка PBO **не требуется**;
  сервер сканирует папки при старте и рассылает клиентам манифест.
* Для докачки треков клиентами запустите `tools/serve_music.py` и пропишите
  `UE_Config::libraryBaseURL`.

## 🎮 Игрок

| Предмет | Действие |
|---|---|
| `UE_CassettePlayer` + `UE_Item_Cassette_*` | экшен «Включить музыку (кассета)» |
| `UE_DiskPlayer` + `UE_Item_Disk_*` | экшен «Включить музыку (диск)» |
| `UE_RadioReceiver` | «Настроить: Апекс/Европа+/Юмор FM», «Выключить радио» |
| Машина с флагом `ueCarRadio=1` | «Включить/выключить музыку в машине» |

Команды идут через RPC → **серверная валидация** (`UE_Security`: живость,
дистанция, доступ к объекту, rate-limit, белый список станций/плейлистов) →
рассылка всем игрокам в радиусе слышимости.

## 🔐 Защита

Мод не работает на сторонних серверах — двухуровневая защита:

* **Уровень 1 (config):** `Uness_Data` и `Uness_Scripts` объявляют
  `requiredAddons[] = {..., "KRa_TosServerInit"}` — без @KRa_TosServer клиентский
  мод не загружается движком, миссия не стартует;
* **Уровень 2 (script handshake):** `UE_GuardServer.c` (@KRa_TosServer) при старте
  рассылает клиентам контрольную строку `UNESSED-HS-V1-KRaTos`; `UE_Guard.c`
  (@Uness) ждёт её в течение grace-периода (`UE_Config::ueGraceSeconds`, по умолчанию 30 с).
  Если приветствия нет (серверный мод удалён/подменён) — **аварийная остановка
  сервера** (TriggerShutdown + Error в RPT);
* `CfgRemoteExec` — allow-list только наших RPC-функций (включая `F_UE_Guard_Handshake`);
* сервер проверяет существование плейлиста на диске и станции в белом списке;
* анти-спам и лимит активных источников;
* подписи PBO обязательны (`verifySignatures = 1`).

## 📄 Лицензия

© 2024–2026 KRa Tos (Константин). Использование — только на сервере «Унесённые».
Распаковка, модификация, распространение и использование на сторонних серверах запрещены.
