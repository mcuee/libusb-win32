@echo off
call make_clean.bat
SET ERRORLEVEL=0

call make_dll.bat %*
IF NOT %ERRORLEVEL%==0 GOTO BUILD_ERROR
call make_driver.bat %*
IF NOT %ERRORLEVEL%==0 GOTO BUILD_ERROR
call make_test.bat %*
IF NOT %ERRORLEVEL%==0 GOTO BUILD_ERROR
call make_test_win.bat %*
IF NOT %ERRORLEVEL%==0 GOTO BUILD_ERROR
call make_install_filter.bat %*
IF NOT %ERRORLEVEL%==0 GOTO BUILD_ERROR
call make_install_filter_win.bat %*
IF NOT %ERRORLEVEL%==0 GOTO BUILD_ERROR

GOTO DONE

:BUILD_ERROR
GOTO DONE

:DONE
