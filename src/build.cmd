@echo off
REM Build the native NYoesyx interpreter (nesxi) with MSVC.
REM Self-contained: sets up the VS 2022 x64 dev env, then compiles the single TU.
setlocal
set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (
  echo [build] vcvars64.bat not found - install VS 2022 C++ tools.
  exit /b 1
)
call "%VCVARS%" >nul
rc /nologo "%~dp0nesxi.rc"
cl /nologo /EHsc /std:c++17 /O2 "%~dp0nesxi.cpp" "%~dp0nesxi.res" /Fe:"%~dp0nesxi.exe" /Fo:"%~dp0nesxi.obj"
set RC=%ERRORLEVEL%
del "%~dp0nesxi.obj" "%~dp0nesxi.res" >nul 2>&1
exit /b %RC%
