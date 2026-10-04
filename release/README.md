# 📼 Унесённые | Uness — готовый билд для загрузки

**Версия:** 1.0.0 · **Автор:** KRa Tos (Константин) · **Сборка:** test build (unsigned, python-pack)

## Состав релиза

```
release/
├── @Uness/                       ← КЛИЕНТНЫЙ мод (игроки)
│   └── Addons/
│       ├── Uness_Data.pbo         (3.3 KB — предметы, конфиги, звуки)
│       └── Uness_Scripts.pbo      (30 KB  — скрипты Enforce + UE_Guard)
├── @KRa_TosServer/               ← СЕРВЕРНЫЙ мод (только DayZServer)
│   └── Addons/
│       └── KRa_TosServerInit.pbo  (2.2 KB — handshake-защита)
└── Keys/
    └── uness.bikey                ← публичный ключ (в keys/ сервера)
```

## Установка клиента
1. Скопировать папку `@Uness` в `DayZ\Mods\`.
2. В лаунчере добавить MOD: путь `...\DayZ\Mods\@Uness`.

## Установка сервера
1. Папку `@KRa_TosServer` положить рядом с `DayZServer.exe`.
2. `uness.bikey` → в папку `DayZServer\keys\`.
3. `serverDZ.cfg`:
   ```
   modDir = "@Uness;@KRa_TosServer";
   ```
4. Перезапуск сервера. В `serverDZ.cfg` также:
   ```cpp
   class Mods {
       class Uness        { dir = "@Uness";        name = "Uness"; };
       class KRa_TosServer{ dir = "@KRa_TosServer"; name = "KRa Tos Server"; };
   };
   ```

## Проверка установки
- RPT сервера: `[KRa_TosServer] Guard handshake sent`
- Без `KRa_TosServerInit.pbo` миссия не стартует (requiredAddons), а guard через ~30 с останавливает сервер — защита активна.

## ⚠️ Важно
PBO упакованы тестовым паковщиком и **не подписаны**. Для продакшена пересоберите
и подпишите официальным **Addon Builder (DayZ Tools)** тем же ключом, что `uness.bikey`,
либо запустите на сервере параметр `-skipAssestSignatureCheck` только для локального теста.
