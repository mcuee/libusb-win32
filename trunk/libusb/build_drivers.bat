@echo on

path %1%\bin
set current_dir=%cd%

set DDKBUILDENV=
call setenv.bat %1 %2
cd %current_dir%
cd src\drivers
build -c
@echo on
cd %current_dir%
copy src\drivers\i386\libusbfl.sys libusbfl.sys
copy src\drivers\i386\libusbst.sys libusbst.sys
cd %current_dir%
