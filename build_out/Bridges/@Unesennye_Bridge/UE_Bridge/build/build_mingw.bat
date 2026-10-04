@echo off
REM Сборка UEAudioBridge.dll MinGW-w64 (x64). Положите bass.dll/bass_aac.dll рядом.
setlocal
set SDK=%~dp0..
set BASS=%SDK%\bass_sdk\c
gcc -O2 -shared -std=c++14 -DUNICODE ^
  -I"%BASS%" -I"%SDK%\include" ^
  "%SDK%\src\ue_bridge.cpp" ^
  -o "%~dp0UEAudioBridge.dll" ^
  -Wl,--kill-at "%SDK%\bass_sdk\c\x64\bass.lib" user32 ole32 ws2_32 winmm -lwininet
echo Готово: %~dp0UEAudioBridge.dll ( скопируйте сюда же bass.dll, bass_aac.dll )
endlocal
