# ============================================================
#  «унесённые» (Uness) — сборочный скрипт по гайдлайнам BI
#  https://dayz.dev/gallery/guides/creating-mod/
#
#  Что делает:
#    1. Копирует исходники аддонов в staging-директорию;
#    2. Упаковывает каждый аддон в .pbo (BI формат:
#       заголовок "VBP\0" + таблица файлов + zlib-данные);
#    3. Формирует структуру @Uness / @KRa_TosServer;
#    4. (опционально) вызывает Addon Builder из DayZ Tools,
#       если он установлен — иначе использует встроенный
#       python-упаковщик (совместимый с vanilla-PBO без бинарей).
#
#  Запуск:  powershell -ExecutionPolicy Bypass -File build.ps1
#           или: dayztools\bin\addonbuilder.exe <workspace> <outdir>
# ============================================================

$ErrorActionPreference = "Stop"
$Root     = Split-Path -Parent $MyInvocation.MyCommand.Path
$Staging  = Join-Path $Root "build\staging"
$OutDir   = Join-Path $Root "build\out\@Uness\Addons"
$OutSrv   = Join-Path $Root "build\out\@KRa_TosServer\Addons"

Write-Host "== Uness build ==" -ForegroundColor Cyan

# --- 1. Staging: чистые папки аддонов (без .cpp-мусора билда) ---
if (Test-Path $Staging) { Remove-Item $Staging -Recurse -Force }
New-Item -ItemType Directory -Force -Path $Staging | Out-Null

$addons = @(
    @{ src = "@Uness\Addons\Uness_Data";     out = $OutDir },
    @{ src = "@Uness\Addons\Uness_Scripts";  out = $OutDir },
    @{ src = "@KRa_TosServer\Addons\KRa_TosServerInit"; out = $OutSrv }
)

foreach ($a in $addons) {
    $s = Join-Path $Root $a.src
    if (-not (Test-Path $s)) { Write-Warning "нет аддона: $s"; continue }
    Copy-Item $s -Destination $Staging -Recurse
}

# --- 2. Сборка PBO ---
# Предпочитаем официальный Addon Builder (DayZ Tools), т.к. только он
# делает подписываемые PBO. Если инструментов нет — fallback на python.
$AddonBuilder = $null
$steam = "${env:ProgramFiles(x86)}\Steam\steamapps\common\DayZTools"
$candidates = @(
    (Join-Path $steam "-addons\AddonBuilder\DayZAddonBuilder.exe"),
    (Join-Path $steam "RTM\private\AddonBuilder\DayZAddonBuilder.exe")
)
foreach ($c in $candidates) { if (Test-Path $c) { $AddonBuilder = $c; break } }

if ($AddonBuilder) {
    Write-Host "используем Addon Builder: $AddonBuilder" -ForegroundColor Green
    New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
    New-Item -ItemType Directory -Force -Path $OutSrv | Out-Null
    foreach ($dir in Get-ChildItem $Staging -Directory) {
        # выбор выходной папки по имени аддона
        $dest = if ($dir.Name -like "*Server*") { $OutSrv } else { $OutDir }
        & $AddonBuilder $dir.FullName $dest "-r:=$($Root)\@Uness;prefix;$($Root)\@KRa_TosServer" -wipe -silent -log
    }
} else {
    Write-Host "Addon Builder не найден — используем python-упаковщик" -ForegroundColor Yellow
    python3 "$Root\tools\pack_pbo.py" $Staging $OutDir $OutSrv
}

# --- 3. Ключи и README для релизной папки ---
New-Item -ItemType Directory -Force -Path "$(Split-Path -Parent $OutDir)\..\Keys" | Out-Null
Copy-Item (Join-Path $Root "@Uness\Keys\*") "$(Split-Path -Parent $OutDir)\..\Keys\" -Force -ErrorAction SilentlyContinue

Write-Host "готово: $Root\build\out" -ForegroundColor Green
