@echo on

set MAKEFLAGS=
path %PATH%;%1%\bin
set current_dir=%cd%

call setenv.bat %1 chk

cd %current_dir%
cd src\driver

@echo on
build -c -g -w
@echo on
cd %current_dir%
copy src\driver\i386\libusb0.sys libusb0.sys
cd %current_dir%
