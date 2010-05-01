@echo off

set TESTS_DIR=..\tests
set SRC_DIR=..\src

call make_clean.bat

copy sources_inf_wizard sources
copy %SRC_DIR%\*.c .
copy %SRC_DIR%\*.h .
copy %SRC_DIR%\*.rc .
copy %SRC_DIR%\driver\driver_api.h .
copy ..\manifest.txt .

CALL build_ddk.bat
IF %ERRORLEVEL%==0 GOTO BUILD_SUCCESS
GOTO BUILD_ERROR

:BUILD_ERROR
ECHO [make_inf_wizard] WinDDK build failed (%BUILD_ALT_DIR%)
pause
GOTO BUILD_DONE

:BUILD_SUCCESS
call make_clean.bat

:BUILD_DONE
