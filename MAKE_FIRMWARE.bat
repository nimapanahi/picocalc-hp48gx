@echo off
setlocal
if "%~1"=="" (
  echo Drag your gxrom-r or gxrom-r.zip file onto MAKE_FIRMWARE.bat
  pause
  exit /b 2
)
py -3 "%~dp0tools\make_uf2.py" "%~dp0hp48gx_picocalc_template.uf2" "%~1" "%~dp0hp48gx_picocalc.uf2"
if errorlevel 1 (
  echo Firmware creation failed.
  pause
  exit /b 1
)
echo.
echo Ready to flash: %~dp0hp48gx_picocalc.uf2
pause
