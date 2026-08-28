@echo off
setlocal

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Release"

cmake -G "Visual Studio 18 2026" -A x64 --preset %CONFIG% -B ./build || exit /b 1
cmake --build ./build --config %CONFIG% --target INSTALL || exit /b 1
