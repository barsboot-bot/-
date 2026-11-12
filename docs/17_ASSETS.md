# Ассеты носителей: кассета, MD-диск, карта памяти

Документ про то, как довести уже готовые исходники (`Models/`) до состояния
«видно в игре». Исходники сделаны скриптами, но два шага всё равно требуют
Windows и DayZ Tools — они расписаны по кнопкам.

---

## 0. Что уже готово

| Что | Где | Статус |
|-----|-----|--------|
| Геометрия в метрах, с именованными селекшенами | `Models/src/*.obj` + `*.mtl` | готово |
| Текстуры: цвет / нормали / блики | `Models/tex_src/*.png` (45 шт., 15 материалов) | готово |
| Обложки альбомов `rock / pop / classic / dance` | `Models/tex_src/ue_cassette_<album>_*`, `ue_minidisc_<album>_*` | готово |
| Материалы `.rvmat` | `@Uness/Addons/Uness_Data/data/tex/` (15 шт.) | готово |
| Классы предметов, пути к моделям, подмена материала | `@Uness/Addons/Uness_Data/config.cpp` | готово |
| Чертёж с размерами | `Models/preview/*_draft.svg` | готово |
| Контрольный рендер (проверить глазами до сборки) | `Models/preview/render_*.png`, `textures_sheet.png` | готово |
| **`.p3d`** (модели) | `@Uness/Addons/Uness_Data/data/` | **только вы, Object Builder** |
| **`.paa`** (текстуры) | `@Uness/Addons/Uness_Data/data/tex/` | **только вы, TexView 2** |

Габариты, селекшены и материалы — `Models/README.md`.

> Почему `.p3d` и `.paa` не делает скрипт: это бинарные форматы Bohemia
> (ODOL и PaC). Их пишут только родные инструменты, текстом такой файл не
> собрать. Всё, что можно подготовить заранее, — подготовлено.

---

## 1. Что нужно на компьютере

1. **DayZ Tools** (Steam → Библиотека → Инструменты) — понадобятся
   *Object Builder* и *TexView 2*.
2. **Python 3** + `pip install pillow numpy` — только если будете пересобирать
   исходники (`tools/make_*.py`).

---

## 2. Шаг 1. Текстуры: PNG → `.paa` (TexView 2)

Делается первым: без `.paa` модели будут белыми, и проверять геометрию будет
неудобно.

1. Откройте **TexView 2**, `File → Open…`, выберите
   `Models\tex_src\ue_cassette_co.png`. Картинка должна открыться.
   * Если «Error loading file» — проверьте, что файл не палитровый:
     `python -c "from PIL import Image; print(Image.open(r'Models\tex_src\ue_cassette_co.png').mode)"`
     должно быть `RGB`.
2. `File → Save As…`.
   * Тип файла: **PaC (DirectX)** — это и есть `.paa`;
   * Имя: **сохраняем суффикс**: `ue_cassette_co.paa`;
   * Папка: `@Uness\Addons\Uness_Data\data\tex\`.
3. В диалоге сжатия:

   | Карта | Формат | Почему |
   |-------|--------|--------|
   | `_co` (цвет) | **DXT1** | прозрачности нет |
   | `_nohq` (нормали) | **DXT5** | DXT1 даёт ступеньки по зелёному каналу |
   | `_smdi` (блики) | **DXT1** | серая карта, цвет не нужен |

   **Generate mipmaps — включено** (иначе текстура «звенит» на расстоянии).
4. Повторите для всех 45 PNG. Быстрее всего — три прохода пакетного
   режима (`File → Batch Convert…`): сначала все `*_co`, потом `*_nohq`,
   потом `*_smdi`.

**Проверка.** В `@Uness\Addons\Uness_Data\data\tex\` должно быть
45 файлов `.paa` + 15 файлов `.rvmat` и **ни одного `.png`**. PNG в пак не
кладём: движок их не читает, а вес растёт.

---

## 3. Шаг 2. Модели: `.obj` → `.p3d` (Object Builder)

Для каждого из трёх файлов (`ue_cassette`, `ue_minidisc`, `ue_memorycard`):

1. `File → Import → Wavefront (.obj)` → выбрать `Models\src\ue_cassette.obj`.
2. **Масштаб 1.0.** В файлах уже метры, Object Builder тоже работает в метрах.
   Масштаб 0.001 = предмет размером с пылинку (самая частая ошибка).
3. Проверьте габарит: `Structure → Bounding Center` / размеры должны совпасть
   с `Models\preview\ue_cassette_draft.svg` (100.0 × 63.8 × 12.0 мм).
4. Назначьте материалы. Имена материалов уже приехали из `.mtl`
   (`ue_cassette`, `ue_plastic`, …). В свойствах текстуры укажите
   **`*.rvmat`**, а не `.paa` — тогда нормали и блики подхватятся сами:

   | Имя материала | Файл |
   |---------------|------|
   | `ue_cassette` | `Uness_Data\data\tex\ue_cassette.rvmat` |
   | `ue_plastic` | `Uness_Data\data\tex\ue_plastic.rvmat` |
   | `ue_minidisc` | `Uness_Data\data\tex\ue_minidisc.rvmat` |
   | `ue_metal` | `Uness_Data\data\tex\ue_metal.rvmat` |
   | `ue_disc` | `Uness_Data\data\tex\ue_disc.rvmat` |
   | `ue_memorycard` | `Uness_Data\data\tex\ue_memorycard.rvmat` |

5. Соберите LOD-схему (минимум, чтобы предмет жил в игре):

   | LOD | Что внутри |
   |-----|------------|
   | `Resolution 0.000` | вся геометрия с материалами (приехала из `.obj`) |
   | `View Geometry` | копия корпуса (`shell`) — попадания/прицеливание |
   | `Geometry` | упрощённая коробка по габариту — физика |
   | `Memory` | точка `ce_center` в центре предмета |
   | `Shadow Volume 0.000` | копия `Resolution` (или `shadow = 0` в конфиге) |

   Для носителей анимация не нужна, поэтому Memory-LOD достаточно из одной
   точки `ce_center`.
6. `File → Save As` → `@Uness\Addons\Uness_Data\data\ue_cassette.p3d`
   (имя — ровно как в `config.cpp`).

**Иконка инвентаря** — здесь же: `Export → Icon`, когда модель сохранена.
Потом путь к иконке можно прописать классу в `inventoryIcon`/`iconTexture`.

---

## 4. Шаг 3. Сборка и проверка

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1
```

`build.ps1` сам копирует `@Uness\Addons\*` в staging и собирает PBO
(Addon Builder, либо `tools/pack_pbo.py`, если Tools не установлены).
Папка `data\` уже внутри аддона — отдельно её нигде подключать не нужно.

В игре (админ-спавн):

```
#spawnitem UE_Item_Cassette_Rock
#spawnitem UE_Item_MiniDisc_Classic
#spawnitem UE_Item_MemoryCard
```

Что должно быть:

| Предмет | Ожидание |
|---------|----------|
| кассета | модель кассеты, на этикетке «РОК-ХИТЫ 80-Х» |
| `UE_Item_MiniDisc_*` | картридж MD, в окошке диск, сверху металлическая шторка |
| карта памяти | SD-карта с наклейкой «8 GB», на обороте золотые контакты |

---

## 5. Как добавить обложку нового альбома

Плейлист = папка в `Music/Type/<плейлист>` (кассеты) или `Music/CD/<плейлист>`
(диски). Дальше:

1. Добавьте альбом в список `ALBUMS` в `tools/make_textures.py`
   (`id`, `title`, `sub`, `color`, `minutes`) и в `tools/make_rvmat.py`.
2. Перегенерируйте:

   ```bash
   python3 tools/make_textures.py --albums
   python3 tools/make_rvmat.py
   ```

3. Сконвертируйте новые PNG в `.paa` (шаг 2).
4. Добавьте класс в `config.cpp` — по образцу `UE_Item_MiniDisc_Rock`:
   `uePlaylist` = имя папки в `Music/`, `hiddenSelectionsMaterials[]` =
   свой `.rvmat`.

Модель при этом **не меняется**: меш один, обложка подменяется материалом
через селекшен `label`.

---

## 6. Чек-лист

- [ ] PNG сконвертированы в `.paa` (45 шт.) и лежат в `Uness_Data\data\tex\`
- [ ] в `data\tex\` нет ни одного `.png`
- [ ] три `.p3d` сохранены в `Uness_Data\data\` с именами из `config.cpp`
- [ ] у каждой модели есть LOD 0.000, Geometry, View Geometry, Memory
- [ ] материалы в `.p3d` — ссылки на `.rvmat`, а не на `.paa`
- [ ] импорт был с масштабом 1.0, габарит совпал с чертежом
- [ ] PBO собран, `.bikey` подписан (Addon Builder)
- [ ] три предмета заспавнены и выглядят как кассета / MD / SD-карта

## 7. Грабли

| Симптом | Причина |
|---------|---------|
| предмет размером с точку | импорт `.obj` с масштабом 0.001 вместо 1.0 |
| модель белая или розовая | не назначен `.rvmat` или нет `.paa` рядом |
| текстура «не та» (например, торцы на лице) | перепутаны стадии в `.rvmat`: поменяйте местами `Stage0` и `Stage1` |
| предмет не появляется в инвентаре | нет LOD 0.000 или `scope = 0` у класса |
| модель видна, но этикетка без названия | классу не прописан `hiddenSelectionsMaterials[]` с обложкой альбома |
| кассета выглядит «глухой», катушек не видно | окно перекрыто сплошной плашкой: селекшен `window` — рамка, середина открыта |
| PNG не открывается в TexView | файл палитровый (`P`) — пересоберите текстуры скриптом |
