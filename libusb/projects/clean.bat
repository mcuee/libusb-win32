@ECHO OFF
DEL /Q *.user *.ncb 2>NUL>NUL
DEL /Q /AH *.suo 2>NUL>NUL
IF EXIST ".\Debug" RMDIR /S /Q ".\Debug"
IF EXIST ".\Release" RMDIR /S /Q ".\Release"
PUSHD %CD%
CD ..\ddk_make
CALL CMD /C make.cmd clean 2>NUL>NUL
POPD
