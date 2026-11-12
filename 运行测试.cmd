@echo off
rem AI-generated test launcher. Assigned owner: Song Kunyuan (2411241).
setlocal
pushd "%~dp0"
python -X utf8 build_windows.py test
set "parser_status=%errorlevel%"
popd
echo.
echo Test exit code: %parser_status%
pause
exit /b %parser_status%
