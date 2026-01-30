@echo off
setlocal

set configuration=debug
if "%1"=="release" (
    set configuration=release
)

set build_dir=build\%configuration%

if not exist %build_dir% (
   build.bat %configuration%
)

xcopy res %build_dir%\res /e /y /I /H

cd %build_dir%

if exist flos.exe (
   del flos.exe
)

make

if "%2"=="" (

if exist flos.exe (
   gdb -batch -ex "set logging on" -ex run -ex "bt full" -ex quit --args flos
)
)

cd ../..

endlocal & exit /b %BUILD_RESULT%
