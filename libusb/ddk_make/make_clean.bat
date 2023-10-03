@echo on

if "%1"=="all" (
if exist .\build\%2 rmdir /s /q .\build\%2
)

echo off
