@echo off
rem Native MSVC environment and build. File/lock/test control lives in the sibling ps1.
setlocal
set "VSBT=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
call "%VSBT%\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 goto build_fail
set "PATH=%VSBT%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%VSBT%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;C:\Program Files\Git\cmd;%LOCALAPPDATA%\Programs\Python\Python312;%LOCALAPPDATA%\Programs\Python\Python312\Scripts;%PATH%"
set "GIT_HTTP_LOW_SPEED_LIMIT=1024"
set "GIT_HTTP_LOW_SPEED_TIME=20"
set "WS=%~1"
set "LABELS=%~2"
cd /d "%WS%"
if errorlevel 1 goto build_fail
rem CI has no pytest in its native build jobs; suppress host-dependent CUDA pytest registration.
rem Reassert the standing flags, including after a CMake option/default change.
rem IPO matches the CI build matrix, not the release packaging configuration.
cmake -S . -B build\cmake_build\static -G Ninja ^
 -DCMAKE_BUILD_TYPE=Release -DBUILD_TEST=ON -DBUILD_GUI=ON ^
 -DBUILD_BENCH=OFF -DBUILD_SHARED_LIBS=OFF -DLUMICE_CUDA_ENABLED=OFF ^
 -DLUMICE_IPO=OFF -DPython3_EXECUTABLE="%LOCALAPPDATA%\Programs\Python\Python312\python.exe" ^
 -DCMAKE_INSTALL_PREFIX="%WS%\build\cmake_install\static" ^
 -DPYTEST_EXECUTABLE:FILEPATH=OFF
if errorlevel 1 goto build_fail
cmake --build build\cmake_build\static
if errorlevel 1 goto build_fail
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0win_quick_leg_remote.ps1" -TestBuild "%WS%\build\cmake_build\static" -Labels "%LABELS%"
exit /b %ERRORLEVEL%
:build_fail
echo WIN_LEG_RESULT: FAIL stage=build reason=command-failed
exit /b 1
