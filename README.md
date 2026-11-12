# 📼 MusicRadio — мод музыки для DayZ Standalone

**Версия:** 1.1.0 · **Автор:** KRa Tos (Константин) · **Лицензия:** только сервер «Унесённые»

> **Моды переименованы:** клиентский мод — **@MusicRadioCore**, серверный — **@MusicRadioServer**
> (аддоны: `MusicRadioCore.pbo`, `MusicRadioScripts.pbo`, `MusicRadioServerInit.pbo`).

Атмосферный музыкальный мод: аудиокассеты, виниловые диски, интернет-радио и
магнитолы в автомобилях. Звук реалистично затухает с расстоянием — его слышат
все игроки рядом с источником.

Проект собран по официальным гайдлайнам Bohemia Interactive / DayZ wiki
(«Creating a mod», структура `@Mod/Addons/*.pbo`, `CfgPatches`, `CfgMods`,
Enforce Script модули `3_Game / 4_World / 5_Mission`).

---

## 📁 Структура репозитория

```
├── @MusicRadioCore/                  # КЛИЕНТ + СЕРВЕР (мод-пак)
│   ├── Addons/
│   │   ├── MusicRadioCore/      # config.cpp: CfgPatches, CfgMods, CfgVehicles,
│   │   │                        #   MR_RadioStations, MR_Config
│   │   └── MusicRadioScripts/   # Enforce Script:
│   │       └── scripts/
│   │           ├── 3_Game/MR_Global.c          # константы (MR_SourceType)
│   │           ├── 4_World/MR_Module*.c        # экшены предметов (world module)
│   │           └── 5_Mission/                  # миссия-модуль:
│   │               ├── MissionServer.c         # точка входа (modded MissionServer)
│   │               ├── MR_AudioManager.c       # ядро звука + затухание
│   │               ├── MR_Network.c            # RPC-слой (GetRPCManager)
│   │               ├── MR_MusicLibrary.c       # внешняя библиотека Music/
│   │               └── MR_Security.c           # серверная валидация команд
│   │       └── 5_Mission/MR_Guard.c            # защита: краш без @MusicRadioServer
│   ├── Bridges/@Uness_Bridge/              # нативный BASS-мост (опционально)
│   ├── Keys/musicradio.bikey                      # публичный ключ подписи
│   └── config.cpp                              # корневой cfgMods/CfgBridges
├── Models/                  # АССЕТЫ (исходники моделей и текстур)
│   ├── src/*.obj *.mtl      # геометрия в метрах, с именованными селекшенами
│   ├── tex_src/*.png        # _co / _nohq / _smdi + обложки альбомов
│   └── preview/             # чертежи SVG, контактный лист, рендеры
├── @MusicRadioServer/            # СЕРВЕРНАЯ часть
│   ├── Addons/MusicRadioServerInit/            # CfgRemoteExec allow-list + MR_GuardServer.c (handshake)
│   ├── Music/                                  # внешняя библиотека (без PBO!)
│   │   ├── Type/<плейлист>/track.ogg|mp3|wav   # кассеты (+ meta.txt: name=...)
│   │   ├── CD/<плейлист>/...                   # диски
│   │   └── Radio.txt                           # станции: Название = URL
│   └── tools/serve_music.py                    # HTTP-зеркало Music/ для клиентов
├── tools/pack_pbo.py            # fallback-упаковщик PBO (без DayZ Tools)
└── build.ps1                    # сборочный скрипт (Addon Builder → pack_pbo)
```

## 🎨 Ассеты носителей

Свои модели и текстуры кассеты, MiniDisc и карты памяти — в каталоге `Models/`.
Всё генерируется скриптами (чистая лицензия, воспроизводимый результат):

| Модель | Габарит | Класс предмета |
|--------|---------|----------------|
| `mr_cassette.p3d` | 100.0 × 63.8 × 12.0 мм | `MR_Item_Cassette_Rock`, `MR_Item_Cassette_Pop` |
| `mr_minidisc.p3d` | 68.0 × 72.0 × 5.0 мм | `MR_Item_MiniDisc_Rock` / `_Pop` / `_Classic` / `_Dance` |
| `mr_memorycard.p3d` | 32.0 × 24.0 × 2.1 мм | `MR_Item_MemoryCard` |

```bash
python3 tools/make_models.py      # .obj + .mtl + чертежи SVG
python3 tools/make_textures.py    # .png (_co/_nohq/_smdi) + обложки альбомов
python3 tools/make_rvmat.py       # .rvmat в @MusicRadioCore/Addons/MusicRadioCore/data/tex/
python3 tools/render_preview.py   # превью-рендер для проверки глазами
```

Два шага остаются за вами — они требуют Windows и DayZ Tools:
`.obj → .p3d` (Object Builder) и `.png → .paa` (TexView 2). Пошагово —
**`docs/17_ASSETS.md`**, габариты и селекшены — `Models/README.md`.

## 🛠 Сборка (по BI-гайду)

1. Установите **DayZ Tools** (Steam) → **Addon Builder**.
2. Sources: папка `@MusicRadioCore` (или `@MusicRadioServer`), Keys: ваш `.biprivatekey`.
3. Build → получаются `MusicRadioCore.pbo`, `MusicRadioScripts.pbo`,
   `MusicRadioServerInit.pbo` в `Addons/`.
4. Без DayZ Tools (только для тестов, PBO не подписаны):
   `powershell -File build.ps1` или `python3 tools/pack_pbo.py <staging> <out>`.

## 🚀 Установка на сервер

```cpp
// serverDZ.cfg
class Mods
{
    class MusicRadio { dir = "@MusicRadioCore";       name = "MusicRadio"; };
    class MusicRadioServer { dir = "@MusicRadioServer"; name = "MusicRadio Server"; };
};
```

* Положите собранные `.pbo` в `<@mod>/Addons/`, `.bikey` — в `keys/` сервера.
* Внешняя музыка кладется в `<profile>/Music/` — пересборка PBO **не требуется**;
  сервер сканирует папки при старте и рассылает клиентам манифест.
* Для докачки треков клиентами запустите `tools/serve_music.py` и пропишите
  `MR_Config::libraryBaseURL`.

## 🎮 Игрок

| Предмет | Действие |
|---|---|
| `MR_CassettePlayer` + `MR_Item_Cassette_*` | экшен «Включить музыку (кассета)» |
| `MR_DiskPlayer` + `MR_Item_Disk_*` | экшен «Включить музыку (диск)» |
| `MR_RadioReceiver` | «Настроить: Апекс/Европа+/Юмор FM», «Выключить радио» |
| Машина с флагом `mrCarRadio=1` | «Включить/выключить музыку в машине» |

Команды идут через RPC → **серверная валидация** (`MR_Security`: живость,
дистанция, доступ к объекту, rate-limit, белый список станций/плейлистов) →
рассылка всем игрокам в радиусе слышимости.

## 🔐 Защита

Мод не работает на сторонних серверах — двухуровневая защита:

* **Уровень 1 (config):** `MusicRadioCore` и `MusicRadioScripts` объявляют
  `requiredAddons[] = {..., "MusicRadioServerInit"}` — без @MusicRadioServer клиентский
  мод не загружается движком, миссия не стартует;
* **Уровень 2 (script handshake):** `MR_GuardServer.c` (@MusicRadioServer) при старте
  рассылает клиентам контрольную строку `UNESSED-HS-V1-KRaTos`; `MR_Guard.c`
  (@MusicRadioCore) ждёт её в течение grace-периода (`MR_Config::mrGraceSeconds`, по умолчанию 30 с).
  Если приветствия нет (серверный мод удалён/подменён) — **аварийная остановка
  сервера** (TriggerShutdown + Error в RPT);
* `CfgRemoteExec` — allow-list только наших RPC-функций (включая `F_MR_Guard_Handshake`);
* сервер проверяет существование плейлиста на диске и станции в белом списке;
* анти-спам и лимит активных источников;
* подписи PBO обязательны (`verifySignatures = 1`).

## 📄 Лицензия

© 2024–2026 KRa Tos (Константин). Использование — только на сервере «Унесённые».
Распаковка, модификация, распространение и использование на сторонних серверах запрещены.
