@echo off

set SRC_DIR=..\src\driver

call make_clean.bat

copy sources_drv sources >NUL

copy %SRC_DIR%\*.c . >NUL
copy %SRC_DIR%\*.h . >NUL
copy %SRC_DIR%\*.rc . >NUL
copy %SRC_DIR%\..\*.rc . >NUL
copy %SRC_DIR%\..\libusb_version.h . >NUL
copy %SRC_DIR%\..\error.? . >NUL

ECHO Building (%BUILD_ALT_DIR%) %0..
CALL build_ddk.bat %1 %2 %3 %4 %5 %6 %7 %8 %9
IF %BUILD_ERRORLEVEL%==0 GOTO BUILD_SUCCESS
GOTO BUILD_ERROR

:BUILD_ERROR
ECHO [%0] WinDDK build failed (%BUILD_ALT_DIR%)
EXIT /B 1

:BUILD_SUCCESS

:BUILD_DONE