@echo off

set TESTS_DIR=..\tests
set SRC_DIR=..\src

call make_clean.bat

copy sources_test sources >NUL
copy %TESTS_DIR%\testlibusb.c . >NUL
copy %SRC_DIR%\lusb0_usb.h . >NUL
copy %SRC_DIR%\*.rc . >NUL

ECHO Building (%BUILD_ALT_DIR%) %0..
CALL build_ddk.bat %*
IF %BUILD_ERRORLEVEL%==0 GOTO BUILD_SUCCESS
GOTO BUILD_ERROR

:BUILD_ERROR
ECHO [%0] WinDDK build failed (%BUILD_ALT_DIR%)
EXIT /B 1

:BUILD_SUCCESS

:BUILD_DONE