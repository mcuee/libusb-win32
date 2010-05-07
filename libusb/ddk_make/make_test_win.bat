@echo off

set TESTS_DIR=..\tests
set SRC_DIR=..\src

call make_clean.bat

copy sources_test_win sources >NUL
copy %TESTS_DIR%\testlibusb_win.c . >NUL
copy %TESTS_DIR%\testlibusb_win_rc.rc . >NUL
copy %SRC_DIR%\usb.h . >NUL
copy %SRC_DIR%\*.rc . >NUL
copy ..\manifest_%_BUILDARCH%.xml . >NUL

ECHO Building (%BUILD_ALT_DIR%) testlibusb-win app..
CALL build_ddk.bat
IF %ERRORLEVEL%==0 GOTO BUILD_SUCCESS
GOTO BUILD_ERROR

:BUILD_ERROR
ECHO [make_test_win] WinDDK build failed (%BUILD_ALT_DIR%)
pause
GOTO BUILD_DONE

:BUILD_SUCCESS
call make_clean.bat

:BUILD_DONE