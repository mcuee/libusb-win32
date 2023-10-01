@echo off
set TESTS_DIR=..\tests
set SRC_DIR=..\src

call make_clean.bat install-filter-win %*

ECHO Building (%*) %0..
CALL build_ddk.bat install-filter-win %*
IF %BUILD_ERRORLEVEL%==0 GOTO BUILD_SUCCESS
GOTO BUILD_ERROR

:BUILD_ERROR
ECHO [%0] WinDDK build failed (%*)
EXIT /B 1

:BUILD_SUCCESS

:BUILD_DONE
