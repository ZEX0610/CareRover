@echo off
setlocal
cd /d "%~dp0.."
if not exist ".venv\Scripts\python.exe" (
  echo First run: py -m venv .venv
  echo Then: .venv\Scripts\python.exe -m pip install -r tools\requirements.txt -r hardware\requirements.txt
  exit /b 1
)
.venv\Scripts\python.exe tools\carerover.py %*
exit /b %errorlevel%
