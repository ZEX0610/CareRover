@echo off
setlocal
cd /d "%~dp0\.."
if not exist ".venv\Scripts\python.exe" (
  echo First run setup:
  echo   py -m venv .venv
  echo   .venv\Scripts\python.exe -m pip install -r hardware\requirements.txt
  exit /b 1
)
set /p "CAREROVER_COM=Main board COM port (example COM6):"
if "%CAREROVER_COM%"=="" exit /b 2
".venv\Scripts\python.exe" hardware\serial_bridge.py --serial-port "%CAREROVER_COM%"
