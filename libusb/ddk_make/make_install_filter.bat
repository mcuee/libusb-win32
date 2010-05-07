@echo off
set TESTS_DIR=..\tests
set SRC_DIR=..\src

call make_clean.bat

copy sources_install_filter sources >NUL
copy %SRC_DIR%\*.c . >NUL
copy %SRC_DIR%\*.h . >NUL
copy %SRC_DIR%\*.rc . >NUL
copy %SRC_DIR%\driver\driver_api.h . >NUL
copy ..\manifest.txt . >NUL

ECHO Building (%BUILD_ALT_DIR%) install-filter app..
CALL build_ddk.bat
IF %ERRORLEVEL%==0 GOTO BUILD_SUCCESS
GOTO BUILD_ERROR

:BUILD_ERROR
ECHO [install_filter] WinDDK build failed (%BUILD_ALT_DIR%)
pause
GOTO BUILD_DONE

:BUILD_SUCCESS
call make_clean.bat

:BUILD_DONE
