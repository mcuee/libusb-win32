@echo off
set TESTS_DIR=..\tests
set SRC_DIR=..\src

call make_clean.bat

copy sources_install_filter_win sources >NUL
copy %SRC_DIR%\*.c . >NUL
copy %SRC_DIR%\*.h . >NUL
copy %SRC_DIR%\*.rc . >NUL
copy %SRC_DIR%\driver\driver_api.h . >NUL
copy %SRC_DIR%\install_filter_win.* . >NUL
copy %SRC_DIR%\common_controls_admin.manifest . >NUL

ECHO Building (%BUILD_ALT_DIR%) %0..
CALL build_ddk.bat %*
IF %BUILD_ERRORLEVEL%==0 GOTO BUILD_SUCCESS
GOTO BUILD_ERROR

:BUILD_ERROR
ECHO [%0] WinDDK build failed (%BUILD_ALT_DIR%)
EXIT /B 1

:BUILD_SUCCESS

:BUILD_DONE
