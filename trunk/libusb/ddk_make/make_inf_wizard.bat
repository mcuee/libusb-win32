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

ECHO Building (%BUILD_ALT_DIR%) %0..
CALL build_ddk.bat %1 %2 %3 %4 %5 %6 %7 %8 %9
IF %BUILD_ERRORLEVEL%==0 GOTO BUILD_SUCCESS
GOTO BUILD_ERROR

:BUILD_ERROR
ECHO [%0] WinDDK build failed (%BUILD_ALT_DIR%)
EXIT /B 1

:BUILD_SUCCESS

:BUILD_DONE
