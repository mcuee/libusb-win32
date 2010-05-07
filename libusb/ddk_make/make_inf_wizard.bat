@echo off

set TESTS_DIR=..\tests
set SRC_DIR=..\src

call make_clean.bat

copy sources_inf_wizard sources >NUL
copy %SRC_DIR%\*.c . >NUL
copy %SRC_DIR%\*.h . >NUL
copy %SRC_DIR%\*.rc . >NUL
copy %SRC_DIR%\driver\driver_api.h . >NUL
copy ..\manifest_%_BUILDARCH%.xml . >NUL

ECHO Building (%BUILD_ALT_DIR%) inf_wizard app..
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
