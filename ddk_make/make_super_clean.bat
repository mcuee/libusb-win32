@echo off

call make_clean.bat

if exist *.exe del /q *.exe
if exist *.dll del /q *.dll
if exist *.sys del /q *.sys 
if exist *.lib del /q *.lib
if exist *.log del /q *.log 
if exist *.wrn del /q *.wrn 
if exist *.err del /q *.err
if exist manifest.txt del /q manifest.txt
if exist .\x86 rmdir /s /q .\x86
if exist .\x64 rmdir /s /q .\x64
if exist .\AMD64 rmdir /s /q .\AMD64
if exist .\i64 rmdir /s /q .\i64
if exist .\w2k rmdir /s /q .\w2k
if exist libusb.lib del /q libusb.lib
if exist *.cer del /q *.cer
if exist libusb-win32.inf.in del /q libusb-win32.inf.in

IF NOT EXIST ..\projects\ GOTO DONE

PUSHD !CD!
CD ..\projects
RMDIR /S /Q .\Win32 2>NUL>NUL
RMDIR /S /Q .\x64 2>NUL>NUL

DEL /S /Q *.gitignore *.log  *.user *.ncb 2>NUL>NUL
DEL /S /Q /AH *.suo 2>NUL>NUL
DEL /S /Q .\additional\libwdi\*.exe 2>NUL>NUL
DEL /S /Q .\additional\libwdi\*.lib 2>NUL>NUL

RMDIR /S /Q .\additional\libwdi\libwdi\objfre_wxp_x86 2>NUL>NUL
RMDIR /S /Q .\additional\libwdi\libwdi\objchk_wxp_x86 2>NUL>NUL
RMDIR /S /Q .\additional\libwdi\libwdi\objfre_wxp_amd64 2>NUL>NUL
RMDIR /S /Q .\additional\libwdi\libwdi\objchk_wxp_amd64 2>NUL>NUL
RMDIR /S /Q .\additional\libwdi\Win32 2>NUL>NUL
RMDIR /S /Q .\additional\libwdi\x64 2>NUL>NUL
RMDIR /S /Q .\additional\libwdi\examples\objfre_wxp_x86 2>NUL>NUL
RMDIR /S /Q .\additional\libwdi\examples\objchk_wxp_x86 2>NUL>NUL

DEL .\additional\libwdi\libwdi\embedded.h 2>NUL>NUL

RMDIR /S /Q ".\Debug" 2>NUL>NUL
RMDIR /S /Q ".\Release" 2>NUL>NUL
POPD

:DONE


