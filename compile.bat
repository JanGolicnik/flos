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

copy /Y "res" %build_dir%\

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
