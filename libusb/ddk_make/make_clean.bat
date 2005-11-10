@echo off

if exist objchk_wxp_x86\i386\*.exe (
  copy /y objchk_wxp_x86\i386\*.exe .
)

if exist objfre_wxp_x86\i386\*.exe (
  copy /y objfre_wxp_x86\i386\*.exe .
)

if exist objchk_wxp_x86\i386\*.dll (
  copy /y objchk_wxp_x86\i386\*.dll .
)

if exist objfre_wxp_x86\i386\*.dll (
  copy /y objfre_wxp_x86\i386\*.dll .
)

if exist objchk_wxp_x86\i386\*.lib (
  copy /y objchk_wxp_x86\i386\*.lib .
)

if exist objfre_wxp_x86\i386\*.lib (
  copy /y objfre_wxp_x86\i386\*.lib .
)

if exist objchk_wxp_x86\i386\*.sys (
  copy /y objchk_wxp_x86\i386\*.sys .
)

if exist objfre_wxp_x86\i386\*.sys (
  copy /y objfre_wxp_x86\i386\*.sys .
)

if exist objchk_wxp_x86 rmdir /s /q objchk_wxp_x86
if exist objfre_wxp_x86 rmdir /s /q objfre_wxp_x86
if exist sources del /q sources 
if exist *.def del *.def 
if exist *.h del *.h 
if exist *.c del *.c 
if exist *.rc del *.rc 
