@echo off

set TESTS_DIR=..\tests
set SRC_DIR=..\src

call make_clean.bat

copy sources_test sources >NUL
copy %TESTS_DIR%\testlibusb.c . >NUL
copy %SRC_DIR%\usb.h . >NUL
copy %SRC_DIR%\*.rc . >NUL

ECHO Building (%BUILD_ALT_DIR%) testlibusb app..
CALL build_ddk.bat
IF %ERRORLEVEL%==0 GOTO BUILD_SUCCESS
GOTO BUILD_ERROR

:BUILD_ERROR
ECHO [make_test] WinDDK build failed (%BUILD_ALT_DIR%)
pause
GOTO BUILD_DONE

:BUILD_SUCCESS
call make_clean.bat

:BUILD_DONE