@echo off

call make_clean.bat all

DEL /Q *.exe *.dll *.sys *.lib *.log *.wrn *.err *.cer *.manifest *.ico ..\*.inf.in 2>NUL>NUL

if exist .\x86 rmdir /s /q .\x86
if exist .\x64 rmdir /s /q .\x64
if exist .\AMD rmdir /s /q .\AMD
if exist .\AMD64 rmdir /s /q .\AMD64

IF NOT EXIST ..\projects\ GOTO DONE

PUSHD !CD!
CD ..\projects\vs2019
RMDIR /S /Q .\Debug 2>NUL>NUL
RMDIR /S /Q .\Release 2>NUL>NUL
RMDIR /S /Q .\x64 2>NUL>NUL
RMDIR /S /Q .\ARM 2>NUL>NUL
RMDIR /S /Q .\ARM64 2>NUL>NUL

RMDIR /S /Q .\_ReSharper.libusb-win32 2>NUL>NUL
RMDIR /S /Q .\.vs

DEL /S /Q *.gitignore *.log  *.user *.ncb *.resharper 2>NUL>NUL
DEL /S /Q /AH *.suo 2>NUL>NUL
DEL /S /Q .\additional\libwdi\*.exe 2>NUL>NUL
DEL /S /Q .\additional\libwdi\*.lib 2>NUL>NUL

RMDIR /S /Q .\additional\libwdi\libwdi\objfre_wxp_x86 2>NUL>NUL
RMDIR /S /Q .\additional\libwdi\libwdi\objchk_wxp_x86 2>NUL>NUL
RMDIR /S /Q .\additional\libwdi\libwdi\objfre_wxp_amd64 2>NUL>NUL
RMDIR /S /Q .\additional\libwdi\libwdi\objchk_wxp_amd64 2>NUL>NUL
RMDIR /S /Q .\additional\libwdi\libwdi\objfre_w2k_x86 2>NUL>NUL
RMDIR /S /Q .\additional\libwdi\libwdi\objchk_w2k_x86 2>NUL>NUL
RMDIR /S /Q .\additional\libwdi\libwdi\objfre_w2k_amd64 2>NUL>NUL
RMDIR /S /Q .\additional\libwdi\libwdi\objchk_w2k_amd64 2>NUL>NUL
RMDIR /S /Q .\additional\libwdi\Win32 2>NUL>NUL
RMDIR /S /Q .\additional\libwdi\x64 2>NUL>NUL
RMDIR /S /Q .\additional\libwdi\examples\objfre_wxp_x86 2>NUL>NUL
RMDIR /S /Q .\additional\libwdi\examples\objchk_wxp_x86 2>NUL>NUL
RMDIR /S /Q .\additional\libwdi\examples\objfre_wxp_w2k 2>NUL>NUL
RMDIR /S /Q .\additional\libwdi\examples\objchk_wxp_w2k 2>NUL>NUL

DEL .\additional\libwdi\libwdi\embedded.h 2>NUL>NUL
DEL .\additional\libwdi\libwdi\config.h 2>NUL>NUL
DEL /S /Q .\additional\libwdi\*.o 2>NUL>NUL
RMDIR /S /Q ".\Win32" 2>NUL>NUL
RMDIR /S /Q ".\x64" 2>NUL>NUL
POPD

:DONE


