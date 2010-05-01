@echo off

set TESTS_DIR=..\tests
set SRC_DIR=..\src

call make_clean.bat

copy sources_test_win sources
copy %TESTS_DIR%\testlibusb_win.c .
copy %TESTS_DIR%\testlibusb_win_rc.rc .
copy %SRC_DIR%\usb.h .
copy %SRC_DIR%\*.rc .
copy ..\manifest.txt

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