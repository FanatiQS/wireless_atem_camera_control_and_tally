@echo off
setlocal
cd %~dp0%
for %%i in ("%~dp0..") do set "root=%%~fi"
echo #define LIB_PATH %root% > src\lib_path.h
echo n | copy /-y "user_config.h" "waccat_config.h"
