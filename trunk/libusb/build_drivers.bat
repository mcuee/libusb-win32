@echo on

path %PATH%;%1%\bin
set current_dir=%cd%

call setenv.bat %1 checked
cd %current_dir%
cd src\drivers
@echo on
build -c -g -w
@echo on
cd %current_dir%
copy src\drivers\i386\libusbfl.sys libusbfl.sys
copy src\drivers\i386\libusbst.sys libusbst.sys
cd %current_dir%
