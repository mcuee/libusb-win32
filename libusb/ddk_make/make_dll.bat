@echo off

set SRC_DIR=..\src

call make_clean.bat

copy sources_dll sources >NUL

copy %SRC_DIR%\*.c . >NUL
copy ..\*.def . >NUL
copy %SRC_DIR%\*.h . >NUL
copy %SRC_DIR%\*.rc . >NUL
copy %SRC_DIR%\driver\driver_api.h . >NUL

ECHO Building (%BUILD_ALT_DIR%) dll..
CALL build_ddk.bat
IF %ERRORLEVEL%==0 GOTO BUILD_SUCCESS
GOTO BUILD_ERROR

:BUILD_ERROR
ECHO [make_dll] WinDDK build failed (%BUILD_ALT_DIR%)
pause
GOTO BUILD_DONE

:BUILD_SUCCESS
call make_clean.bat
if exist libusb.lib del /q libusb.lib >NUL
if exist libusb0.lib move libusb0.lib libusb.lib >NUL

:BUILD_DONE
