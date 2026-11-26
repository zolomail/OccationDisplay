@echo off
setlocal
set "SCRIPT_DIR=%~dp0\convert-png\"
python "%SCRIPT_DIR%font_converter.py" "%SCRIPT_DIR%fonts\*.png" --out-dir "%SCRIPT_DIR%..\src\fonts"
