@echo off
REM ============================================================
REM  build_msvc.bat — сборка UEAudioBridge.dll (x64) через MSVC
REM  Предварительно:
REM   1. Скачайте BASS SDK: https://www.un4seen.com/bass.html
REM      (bass.zip → распакуйте в %~dp0..\bass_sdk\)
REM   2. В папке bass_sdk\c\ должны быть: bass.h, bass.lib,
REM      dll\x64\bass.dll, плюс plugin aac: bass_aac.dll
REM   3. Запустите из "x64 Native Tools Command Prompt for VS".
REM ============================================================
setlocal
set SDK=%~dp0..
set BASS=%SDK%\bass_sdk\c
set OUT=%~dp0..\build
if not exist "%OUT%" mkdir "%OUT%"

if not exist "%BASS%\bass.h" (
    echo [ОШИБКА] Не найден BASS SDK в "%BASS%".
    echo Скачайте bass.zip с un4seen.com и распакуйте в UE_Bridge\bass_sdk\
    exit /b 1
)

cl /nologo /O2 /LD /MT /DUNICODE /D_UNICODE ^
   /I "%BASS%" /I "%SDK%\include" ^
   "%SDK%\src\ue_bridge.cpp" ^
   /Fe:"%OUT%\UEAudioBridge.dll" ^
   /link /DEF:"%SDK%\src\UEAudioBridge.def" "%BASS%\bass.lib" user32.lib ole32.lib ws2_32.lib winmm.lib

if errorlevel 1 (
    echo [ОШИБКА] Компиляция не удалась.
    exit /b 1
)

copy /Y "%SDK%\bass_sdk\dll\x64\bass.dll"      "%OUT%\" >nul
copy /Y "%SDK%\bass_sdk\addon\aac\x64\bass_aac.dll" "%OUT%\" >nul 2>nul
echo Готово: %OUT%\UEAudioBridge.dll (+ bass.dll, bass_aac.dll)
endlocal
