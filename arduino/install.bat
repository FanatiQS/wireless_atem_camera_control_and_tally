@echo off
setlocal
cd %~dp0
for %%i in ("%~dp0..") do set "root=%%~fi"
echo #define LIB_PATH %root% > src\lib_path.h
if not exist "waccat_config.h" (
	copy "user_config.h" "waccat_config.h"
)
