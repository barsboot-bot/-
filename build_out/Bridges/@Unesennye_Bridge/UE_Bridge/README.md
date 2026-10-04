# Аудио-прослойка «унесённые» (UEAudioBridge)

Мост между DayZ-скриптом мода и библиотекой **BASS** (un4seen.com).
Позволяет то, чего движок DayZ из коробки не умеет:

| Возможность | Реализация |
|---|---|
| Радио-потоки HTTP ICY/Shoutcast (Апекс, Европа+, Юмор FM) | `BASS_StreamCreateURL` (MP3 через ядро BASS, AAC — плагин `bass_aac.dll`) |
| Треки кассет/дисков (.ogg/.mp3/.wav вне PBO) | `BASS_StreamCreateFile` |
| Затухание по расстоянию для всех источников | скрипт шлёт позицию слушателя → мост пересчитывает громкость `(d0/d)^k`, d0=5м, предел 150м |
| Синхронный трек у всех игроков | позиция источника и время старта идут через RPC сервера; `UE_GetState` отдаёт позицию трека |

## Состав
```
include/ue_bridge.h      — публичный C-API DLL
src/ue_bridge.cpp        — реализация на BASS (собирается в UEAudioBridge.dll)
src/UEAudioBridge.def    — список экспортов
build/build_msvc.bat     — сборка MSVC x64 (или build_mingw.bat)
overlay/UEOverlay.cs     — компаньон клиента (читает очередь команд, вызывает DLL)
```

## Как это подключено к моду
Чистый EnforceScript **не умеет** LoadLibrary/GetProcAddress. Поэтому цепочка:

```
UE_AudioManager (скрипт)
   └─ UE_ISoundBackend (Scripts/Core/UE_SoundBackend.c)
        ├─ UE_NativeSoundBackend  → пишет подписанные команды в
        │    <DayZ>/unesennye_bridge/commands.q
        │         └─ UEOverlay.exe читает очередь → UEAudioBridge.dll (BASS)
        │              и отвечает статусом в bridge_status.txt
        └─ UE_FallbackSoundBackend→ штатный SoundSource движка (только .ogg, без стримов)
```

Команды подписаны FNV-1a + общим ключом (`UE-BRIDGE-KEY-2026` в
`UE_SoundBackend.c` и `UEOverlay.cs`) — строки, подброшенные в файл очереди
любой сторонней программой, оверлеем отбрасываются. URL дополнительно
фильтруется белым списком станций дважды: на сервере (UE_Security) и локально
в бэкенде.

## Сборка
1. Скачайте BASS SDK: https://www.un4seen.com/download.php?bass24
   Распакуйте так, чтобы `bass_sdk/c/bass.h`, `bass_sdk/c/bass.lib`,
   `bass_sdk/c/dll/x64/bass.dll` существовали. Для AAC-стрима Европы+
   возьмите также `bass_aac` add-on.
2. `build/build_msvc.bat` (из x64 Native Tools Prompt) или `build/build_mingw.bat`.
3. Оверлей: `csc /platform:x64 /out:UEOverlay.exe overlay/UEOverlay.cs`
   (требуется .NET Framework 4.8; UEAudioBridge.dll+bass.dll кладутся рядом с exe).
4. Дистрибутив игроку: папка `Mods/@Unesennye/Bin/` = UEAudioBridge.dll,
   bass.dll, bass_aac.dll, UEOverlay.exe.

## Проверка без DayZ
`UE_SelfTest("http://62.152.59.3:8000/nkz", 30)` проигрывает стрим 30 секунд,
имитируя удаление слушателя на 10 м/с — видно, как громкость уходит по кривой.

## Оговорки
* licenzия BASS бесплатна для некоммерческого использования; для коммерческих
  серверов — купите лицензию un4seen.
* Путь к папке клиента задан в `UE_BridgePaths` — поправьте под свою установку
  (или замените на автоопределение через GetFileNameFromPathEx, если ваша
  версия скрипта это разрешает).
* Если у вашего серверного лодера есть нативный экспорт вызова DLL из скрипта,
  файловую очередь можно выбросить и звать UEAudioBridge.dll напрямую —
  интерфейс UE_ISoundBackend позволяет сделать это одной новой реализацией.
