@echo off
rem AI-generated demo launcher. Assigned owner: Song Kunyuan (2411241).
setlocal
pushd "%~dp0"
python -X utf8 build_windows.py demo
set "parser_status=%errorlevel%"
popd
echo.
echo Demo exit code: %parser_status%
pause
exit /b %parser_status%
