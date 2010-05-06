@echo off

set SRC_DIR=..\src

call make_clean.bat

copy sources_dll sources

copy %SRC_DIR%\*.c .
copy ..\*.def .
copy %SRC_DIR%\*.h .
copy %SRC_DIR%\*.rc .
copy %SRC_DIR%\driver\driver_api.h .

CALL build_ddk.bat
IF %ERRORLEVEL%==0 GOTO BUILD_SUCCESS
GOTO BUILD_ERROR

:BUILD_ERROR
ECHO [make_dll] WinDDK build failed (%BUILD_ALT_DIR%)
pause
GOTO BUILD_DONE

:BUILD_SUCCESS
call make_clean.bat
if exist libusb.lib del /q libusb.lib
if exist libusb0.lib move libusb0.lib libusb.lib

:BUILD_DONE
