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
│   ├── Keys/KRaTos.bikey                      # публичный ключ подписи
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
2. Sources: папка `@Uness` (или `@KRa_TosServer`).
3. **Подпись — вашим ключом `KRaTos.biprivatekey`:**
   Addon Builder → кнопка/вкладка **Signature** → в поле *Private key*
   выберите **`KRaTos`** (список подхватывает все `.biprivatekey` из папки
   `...\DayZTools\addons\Keys\`; если ключ с паролем — введите его).
   ⚠️ Имя ключа в GUI = имя файла без расширения: файл `KRaTos.biprivatekey`
   отображается как **KRaTos** (не «uness» и не «KRa_TosServerInit»).
4. Build with signature → получаются подписанные `Uness_Data.pbo`,
   `Uness_Scripts.pbo`, `KRa_TosServerInit.pbo` в `Addons/`.
5. Без DayZ Tools (только для тестов, PBO не подписаны):
   `powershell -File build.ps1` или `python3 tools/pack_pbo.py <staging> <out>`.
6. Автоматическая подпись из скрипта (когда DayZ Tools установлен):
   ```powershell
   powershell -ExecutionPolicy Bypass -File build.ps1 -Sign `
     -KeyName "KRaTos" -KeyDir "C:\...\DayZTools\addons\Keys" -KeyPass "пароль"
   ```
   (положите `KRaTos.biprivatekey` в `@Uness\Keys\` — тогда `-KeyDir` не нужен;
   приватный ключ в git не попадает: `*.biprivatekey` в .gitignore)

### ❌ Частые причины «Failed to sign»
| Причина | Решение |
|---|---|
| В Addon Builder выбран несуществующий ключ (`uness`, `KRa_TosServerInit`) | выбрать **KRaTos** (= ваш `KRaTos.biprivatekey`) |
| `.biprivatekey` лежит не в папке Keys, указанной в настройках DayZ Tools | Addon Builder → Options → путь к папке ключей; либо переложить `KRaTos.biprivatekey` туда |
| Ключ защищён паролем, но пароль не введён | ввести пароль в диалоге Signature / `-KeyPass` |
| PBO собраны python-паковщиком (тестовые) | пересобрать через сам Addon Builder (Build), затем Sign |

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
