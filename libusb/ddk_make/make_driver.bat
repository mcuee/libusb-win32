@echo off

set SRC_DIR=..\src\driver

call make_clean.bat libusb0 %*

ECHO Building (%*) %0..
CALL build_ddk.bat libusb0 %*
IF %BUILD_ERRORLEVEL%==0 GOTO BUILD_SUCCESS
GOTO BUILD_ERROR

:BUILD_ERROR
ECHO [%0] WinDDK build failed (%*)
EXIT /B 1

:BUILD_SUCCESS

:BUILD_DONE