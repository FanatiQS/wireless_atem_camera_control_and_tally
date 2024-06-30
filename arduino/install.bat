@echo off
setlocal
cd %~dp0%
for %%i in ("%~dp0..") do set "root=%%~fi"
echo #define LIB_PATH %root% > src\lib_path.h
