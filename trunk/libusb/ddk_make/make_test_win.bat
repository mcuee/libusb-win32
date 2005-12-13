@echo off

set TESTS_DIR=..\tests
set SRC_DIR=..\src

copy sources_test_win sources
copy %TESTS_DIR%\testlibusb_win.c .
copy %SRC_DIR%\usb.h .
copy %SRC_DIR%\*.rc .
copy ..\testlibusb-win.exe.manifest .

build

call make_clean.bat
