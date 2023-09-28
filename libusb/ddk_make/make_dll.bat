@echo off

set SRC_DIR=..\src

call make_clean.bat libusb-dll %*

ECHO Building (%BUILD_ALT_DIR%) %0..
CALL build_ddk.bat libusb-dll %*
IF %BUILD_ERRORLEVEL%==0 GOTO BUILD_SUCCESS
GOTO BUILD_ERROR

:BUILD_ERROR
ECHO [%0] WinDDK build failed (%BUILD_ALT_DIR%)
EXIT /B 1

:BUILD_SUCCESS
if exist libusb0.lib move /Y libusb0.lib libusb.lib >NUL
:BUILD_DONE