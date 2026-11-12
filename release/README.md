# 📼 Унесённые | MusicRadio — готовый билд для загрузки

**Версия:** 1.1.0 · **Автор:** KRa Tos (Константин) · **Сборка:** test build (unsigned, python-pack)

## Состав релиза

```
release/
├── @MusicRadioCore/                       ← КЛИЕНТНЫЙ мод (игроки)
│   └── Addons/
│       ├── MusicRadioCore.pbo         (3.3 KB — предметы, конфиги, звуки)
│       └── MusicRadioScripts.pbo      (30 KB  — скрипты Enforce + MR_Guard)
├── @MusicRadioServer/               ← СЕРВЕРНЫЙ мод (только DayZServer)
│   └── Addons/
│       └── MusicRadioServerInit.pbo  (2.2 KB — handshake-защита)
└── Keys/
    └── musicradio.bikey                ← публичный ключ (в keys/ сервера)
```

## Установка клиента
1. Скопировать папку `@MusicRadioCore` в `DayZ\Mods\`.
2. В лаунчере добавить MOD: путь `...\DayZ\Mods\@MusicRadioCore`.

## Установка сервера
1. Папку `@MusicRadioServer` положить рядом с `DayZServer.exe`.
2. `musicradio.bikey` → в папку `DayZServer\keys\`.
3. `serverDZ.cfg`:
   ```
   modDir = "@MusicRadioCore;@MusicRadioServer";
   ```
4. Перезапуск сервера. В `serverDZ.cfg` также:
   ```cpp
   class Mods {
       class MusicRadio        { dir = "@MusicRadioCore";        name = "MusicRadio"; };
       class MusicRadioServer{ dir = "@MusicRadioServer"; name = "KRa Tos Server"; };
   };
   ```

## Проверка установки
- RPT сервера: `[MusicRadioServer] Guard handshake sent`
- Без `MusicRadioServerInit.pbo` миссия не стартует (requiredAddons), а guard через ~30 с останавливает сервер — защита активна.

## ⚠️ Важно
PBO упакованы тестовым паковщиком и **не подписаны**. Для продакшена пересоберите
и подпишите официальным **Addon Builder (DayZ Tools)** тем же ключом, что `musicradio.bikey`,
либо запустите на сервере параметр `-skipAssestSignatureCheck` только для локального теста.

---

## 🎨 Архив ассетов

`MusicRadio_assets_v1.1.0.zip` — исходники моделей и текстур носителей
(кассета, MiniDisc, карта памяти) для работы на машине с DayZ Tools:

```
1_MODELS/      .obj + .mtl  (→ Object Builder, масштаб 1.0)
2_TEXTURES/    45 PNG: _co / _nohq / _smdi (→ TexView 2)
3_MATERIALS/   15 .rvmat (→ @MusicRadioCore\Addons\MusicRadioCore\data\tex\)
4_PREVIEW/     чертежи SVG, контрольные рендеры, контактный лист
5_IN_GAME/     config.cpp (образец классов) + лут types
tools/         генераторы исходников + REBUILD_ASSETS.bat
```

Что **нельзя** сделать скриптом: `.p3d` и `.paa` — бинарные форматы Bohemia,
их пишут только Object Builder и TexView 2. Порядок — `docs/17_ASSETS.md`
в репозитории (и `docs/17_ASSETS.md` внутри архива).
