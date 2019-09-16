@echo off

set OUTDIR=

if exist .\output\i386 set OUTDIR=.\output\i386
if exist .\output\amd64 set OUTDIR=.\output\amd64
if exist .\output\ia64  set OUTDIR=.\output\ia64

if "%OUTDIR%"=="" GOTO NO_OUTDIR
  if exist %OUTDIR%\*.exe copy /y %OUTDIR%\*.exe . >NUL
  if exist %OUTDIR%\*.dll copy /y %OUTDIR%\*.dll . >NUL
  if exist %OUTDIR%\*.lib copy /y %OUTDIR%\*.lib . >NUL
  if exist %OUTDIR%\*.sys copy /y %OUTDIR%\*.sys . >NUL
:NO_OUTDIR

if exist .\output rmdir /s /q .\output

if exist .\objchk_wxp_x86 rmdir /s /q .\objchk_wxp_x86
if exist .\objchk_wnet_AMD64 rmdir /s /q .\objchk_wnet_AMD64
if exist .\objchk_wnet_IA64 rmdir /s /q .\objchk_wnet_IA64
if exist .\objchk_wxp_ia64 rmdir /s /q .\objchk_wxp_ia64
if exist .\objchk_w2k_x86 rmdir /s /q .\objchk_w2k_x86

if exist .\objfre_wxp_x86 rmdir /s /q .\objfre_wxp_x86
if exist .\objfre_wnet_AMD64 rmdir /s /q .\objfre_wnet_AMD64
if exist .\objfre_wnet_IA64 rmdir /s /q .\objfre_wnet_IA64
if exist .\objfre_wxp_ia64 rmdir /s /q .\objfre_wxp_ia64
if exist .\objfre_w2k_x86 rmdir /s /q .\objfre_w2k_x86

if exist sources del /q sources 
if exist *.def del *.def 
if exist *.h del *.h 
if exist *.c del *.c 
if exist *.rc del *.rc 
if exist manifest_*.xml del /q manifest_*.xml
if exist install-filter*.txt del /q install-filter*.txt
DEL /Q "..\*.o" "..\*.dll" "..\*.a" "..\*.exp" "..\*.lib" "..\*.exe" 2>NUL>NUL
DEL /Q "..\*.tar.gz" "..\*.iss" "..\*.rc" "..\*.h" "..\*.sys" "..\*.log" 2>NUL>NUL
DEL /Q /S "..\*~" 2>NUL>NUL
DEL /Q "..\README.txt" 2>NUL>NUL
