@echo off
rem Replicability script (Windows): reproduces the frames of the teaser figure (Fig. 1) of the paper
rem   Dynamic Skinning: Kinematics-Driven Cartoon Effects for Articulated Characters (IEEE TVCG 2026)
rem
rem Usage (no parameter, from any directory):  replicability\teaser.bat
rem
rem Same as teaser.sh: builds the complete application with CMake and Visual Studio (code\complete.exe),
rem then runs it in capture mode on the three examples of the teaser, and saves the frames as PNG images in
rem replicability\output\teaser\ (giraffe\, clown\, train\). A window opens during each capture, and closes
rem by itself after the last frame. See REPLICABILITY.txt for the description of the frames.

setlocal
pushd "%~dp0.."
set "ROOT=%CD%"
popd
set "OUTPUT=%ROOT%\replicability\output\teaser"

rem ---- Build ----
cd /d "%ROOT%\code"
cmake -S . -B build || exit /b 1
cmake --build build --config Release --target complete || exit /b 1

rem ---- Captures ----
call :capture giraffe Giraffe "3.0,3.1,3.2,3.3,3.4,3.5,3.6,3.75,3.9,4.1,4.3,4.6,5.0"
call :capture clown Clown "3.0,3.1,3.2,3.3,3.4,3.5,3.6,3.75,3.9,4.1,4.3,4.6,5.0"
call :capture train Train "0.5,0.6,0.7,0.8,0.9,1.0,1.1,1.2,1.3,1.5,3.6,3.8,4.0,4.2,4.4"

echo Done: frames of the teaser in %OUTPUT%
exit /b 0

rem capture <directory> <model> <animation times>
:capture
if not exist "%OUTPUT%\%1" mkdir "%OUTPUT%\%1"
del /q "%OUTPUT%\%1\*.png" 2>nul
echo Capturing %2 ...
set "DS_CAPTURE_DIR=%OUTPUT%\%1\"
set "DS_CAPTURE_MODEL=%2"
set "DS_CAPTURE_TIMES=%~3"
set "DS_CAPTURE_SKELETON=0"
complete.exe > "%OUTPUT%\%1\log.txt" 2>&1
for /f %%n in ('dir /b "%OUTPUT%\%1\*.png" ^| find /c /v ""') do echo   %%n frames in %OUTPUT%\%1
exit /b 0
