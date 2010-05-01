@echo off
set TESTS_DIR=..\tests
set SRC_DIR=..\src

call make_clean.bat

copy sources_install_filter sources %~1
copy %SRC_DIR%\*.c . %~1
copy %SRC_DIR%\*.h . %~1
copy %SRC_DIR%\*.rc . %~1
copy %SRC_DIR%\driver\driver_api.h . %~1
copy ..\manifest.txt . %~1

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
