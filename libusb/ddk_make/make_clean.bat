@echo on

if "%1"=="all" (
if exist .\output\%2 rmdir /s /q .\output\%2
)

echo off

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
