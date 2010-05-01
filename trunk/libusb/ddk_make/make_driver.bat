@echo off

set SRC_DIR=..\src\driver

call make_clean.bat

copy sources_drv sources

copy %SRC_DIR%\*.c .
copy %SRC_DIR%\*.h .
copy %SRC_DIR%\*.rc .
copy %SRC_DIR%\..\*.rc .

CALL build_ddk.bat
IF %ERRORLEVEL%==0 GOTO BUILD_SUCCESS
GOTO BUILD_ERROR

:BUILD_ERROR
ECHO [make_driver] WinDDK build failed (%BUILD_ALT_DIR%)
pause
GOTO BUILD_DONE

:BUILD_SUCCESS
call make_clean.bat

:BUILD_DONE
