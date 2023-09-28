@echo off

set SRC_DIR=..\src\driver

call make_clean.bat libusb0 %*

ECHO Building (%BUILD_ALT_DIR%) %0..
CALL build_ddk.bat libusb0 %*
IF %BUILD_ERRORLEVEL%==0 GOTO BUILD_SUCCESS
GOTO BUILD_ERROR

:BUILD_ERROR
ECHO [%0] WinDDK build failed (%BUILD_ALT_DIR%)
EXIT /B 1

:BUILD_SUCCESS

:BUILD_DONE