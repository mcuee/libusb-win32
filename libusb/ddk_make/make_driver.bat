@echo off

set SRC_DIR=..\src\driver

copy sources_drv sources
copy %SRC_DIR%\*.c .
copy %SRC_DIR%\*.h .
copy %SRC_DIR%\*.rc .

build

call make_clean.bat
