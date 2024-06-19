@echo off
setlocal
cd %~dp0%
for %%i in ("%~dp0..") do set "root=%%~fi"
echo #define LIB_PATH %root% > lib_path.h
