# 🎵 Мод «унесённые» (Unesennye) для DayZ

Мод добавляет **музыку**: кассеты, диски, интернет-радио и автомобильную магнитолу. Звук слышат **все игроки рядом**, громкость **затухает с расстоянием**. Есть **серверная защита** от злоупотреблений.

## Возможности
| Функция | Предметы | Действие |
|---|---|---|
| 📼 Кассеты | `UE_CassettePlayer` + `UE_Item_Cassette_Rock/Pop` | «Включить музыку (кассета)» |
| 💿 Диски | `UE_DiskPlayer` + `UE_Item_Disk_Classic/Dance` | «Включить музыку (диск)» |
| 📻 Радио-стримы | `UE_RadioReceiver` | «Настроить: Апекс / Европа Плюс / Юмор FM» |
| 🚗 Музыка в машине | авто с флагом `ueCarRadio=1` | «Включить музыку в машине» |

### Радиостанции (белый список сервера)
- Апекс: http://62.152.59.3:8000/nkz
- Европа+: http://online-2.gkvr.ru:8000/europa_nkz_64.aac
- Юмор FM: http://62.231.184.253:8000/humor

### Затухание звука
Клиент пересчитывает громкость каждые 0.5 с по квадратичной модели  
`V = base · (d₀/d)^k`, d₀ = 5 м (максимум), граница слышимости 150 м (параметры в `UE_Config`). Для машин позиция источника обновляется — звук следует за автомобилем.

## Структура
```
@Unesennye/                     # клиентский модпак (обязателен у игроков)
├── config.cpp                  # предметы, станции, параметры затухания
└── mpmissions/unesennye.Mission.Enoch/   # миссия со скриптами
    ├── init.c mission.c description.cpp mission.xml
    └── Scripts/
        ├── Core/UE_AudioManager.c   # звуковое ядро + attenuation
        ├── Core/UE_Security.c       # СЕРВЕРНАЯ ЗАЩИТА мода
        ├── Network/UE_Network.c     # RPC клиент↔сервер↔все
        └── Modules/UE_Module{Cassette,Disk,Radio,CarRadio}.c
@UnesennyeServer/               # серверный защитный модпак
└── config.cpp                  # checkModLoad, CfgRemoteExec whitelist, лимиты
```

## Серверная защита (@UnesennyeServer + UE_Security)
1. `checkModLoad = 1` — игрок без `@Unesennye` не подключится.
2. `verifySignatures = 1` — подписи PBO (`.bikey` в `keys/`).
3. `CfgRemoteExec` — разрешены только RPC мода, чужие блокируются.
4. Runtime-валидация каждой команды на сервере: объект мода, живость игрока, дистанция ≤ 300 м, доступ к объекту, анти-флуд (≥1.5 с, ≤20 команд/мин), лимит 64 источников, HMAC-подпись пакета, белый список станций, лог нарушений.
5. Нельзя включить произвольный URL — только станции из конфига.

## Установка
1. Скопировать `@Unesennye` и `@UnesennyeServer` на сервер.
2. `serverDZ.cfg`: `modDir=@Unesennye;@UnesennyeServer`, `mission=unesennye.Mission.Enoch`.
3. Подписать PBO своим ключом, `.bikey` положить в `keys/`.
4. Игроки скачивают `@Unesennye`.

## ⚠️ Важно для продакшена
- **DayZ Engine не воспроизводит HTTP-стримы штатными средствами.** Слой `UE_LocalSound.Play(url,…)` требует аудио-прослойки (FMOD/BASS-мод или голосовой канал); без неё радио — заглушка. Треки кассет/дисков играют через штатный звук движка: положите `.ogg` в `dzue/sounds/cassettes|disks/…` внутри PBO.
- Скрипты — рабочий каркас на EnforceScript: при сборке сверьте сигнатуры API вашей версии DayZ (1.2x).
- Замените модели-заглушки (`notepad.p3d`, `magazine_rifle_556.p3d`) на свои `.p3d`.

© Unesennye Team, 2026

## Внешняя музыка без PBO (Music/)
Музыка кассет/дисков НЕ пакуется в PBO. Сервер читает папку `<Profile>/Music`:
```
Music/Type/<плейлист>/track.ogg|mp3|wav   — кассеты
Music/CD/<плейлист>/track.ogg|mp3|wav    — диски
Music/Radio.txt                            — станции ("Название = URL")
Music/<...>/meta.txt                       — name = Отображаемое имя
```
Клиенты получают манифест по RPC и докачивают треки с HTTP-зеркала
(`UE_Config::libraryBaseURL`), кэш — `<Profile>/Music_cache/`.
Образец структуры: `@UnesennyeServer/Music/`, зеркало: `@UnesennyeServer/tools/serve_music.py`.

## Сборка в PBO

```
python build_pbo.py          # → build_out/ (native unsigned PBO, без внешних тулзов)
```

# Готовые артефакты — что скачано и что докачать

В `build_out/` после сборки лежат готовые к раздаче файлы:

| Файл | Куда ставить | Статус |
|---|---|---|
| `@Unesennye/Unesennye.pbo` | `mods/@Unesennye/` (сервер + клиенты) | ✅ собран (unsigned PBO корректного BI-формата) |
| `@Unesennye/mpmissions/unesennye.Mission.Enoch.pbo` | `mpmissions/` сервера | ✅ собран |
| `@UnesennyeServer/UnesennyeServer.pbo` | только на сервер | ✅ собран |
| `Music/…`, `tools/serve_music.py` | профиль сервера / HTTP-зеркало | ✅ шаблоны готовы |
| `UE_Bridge/bass_sdk/` (bass.h, bass.lib x86/x64, bass.dll x64) | уже внутри Unesennye.pbo | ✅ скачано с un4seen.com |

### Нужно докачать/собрать вручную
1. **bass_aac.dll (x64)** — плагин AAC для «Европа+» (.aac-стрим). un4seen.com → BASS add-ons → «AAC», положить в `@Unesennye/Bridges/@Unesennye_Bridge/UE_Bridge/bass_sdk/addon/aac/x64/`. Без него Апекс и ЮморFM (MP3) играют, Европа+ — нет.
2. **UEAudioBridge.dll** — готовой сборки не существует; собрать из исходников на Windows:
   - MSVC: открыть «x64 Native Tools Command Prompt for VS» → `@Unesennye\UE_Bridge\build\build_msvc.bat` (BASS SDK уже лежит в `bass_sdk\`);
   - MinGW: `build_mingw.bat`;
   - результат (`UEAudioBridge.dll` + скопированный рядом `bass.dll`) раздаётся клиентам через лаунчер/архив мода.
3. **Подпись PBO ключом** (для `verifySignatures=1`): DayZ Tools (Steam) или `dayzplus_bekeypair` (GitHub Kelson/dayztools):
   `BeKeyPair.exe create @Unesennye 128` → подписать каждый .pbo → `.bikey` положить в `keys/` сервера и клиентов. Пока подпись не сделана — ставьте `verifySignatures=0` в server.cfg.
4. **Треки**: `.ogg/.mp3` в `Music/Type/<плейлист>/` и `Music/CD/<плейлист>/` (вне PBO).

> Примечание: релизы armake2/Karel-a на GitHub отдают 404, поэтому в репозитории есть собственный упаковщик native-PBO в `build_pbo.py` — он формирует валидные неподписанные .pbo без внешних утилит.

Результат в `build_out/`: `@Unesennye/Unesennye.pbo`,
`@Unesennye/Missions|mpmissions/unesennye.Mission.Enoch.pbo`,
`@UnesennyeServer/UnesennyeServer.pbo`, внешние `Music/` и `tools/` — не в PBO.
Для релиза подпишите PBO (`verifySignatures=1`), `.bikey` → `keys/`.
