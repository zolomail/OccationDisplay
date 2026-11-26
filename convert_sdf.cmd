@echo off
setlocal
set "SCRIPT_DIR=%~dp0\convert-png\"
python "%SCRIPT_DIR%sdf_converter.py" "%SCRIPT_DIR%sdf_samples\*.png" -o "%SCRIPT_DIR%..\src\bitmaps\SdfAtlases.h" --size 32
