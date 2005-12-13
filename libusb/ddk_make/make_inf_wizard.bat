@echo off

set TESTS_DIR=..\tests
set SRC_DIR=..\src

copy sources_inf_wizard sources
copy %SRC_DIR%\*.c .
copy %SRC_DIR%\*.h .
copy %SRC_DIR%\*.rc .
copy %SRC_DIR%\driver\driver_api.h .
copy ..\inf-wizard.exe.manifest .
build

call make_clean.bat
