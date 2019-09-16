@ECHO OFF

PUSHD %CD%
CD ..\ddk_make
CALL CMD /C make.cmd clean 2>NUL>NUL
POPD
