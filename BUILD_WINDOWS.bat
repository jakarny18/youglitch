@echo off
setlocal
cd /d "%~dp0"
echo ==========================================
echo YouGlitch SMP 1024 - Windows x64 build
echo ==========================================
where cmake >nul 2>nul
if errorlevel 1 (
  echo ERROR: CMake is not installed or not in PATH.
  goto :error
)
cmake -S . -B build -A x64 -DCMAKE_BUILD_TYPE=Release -DJUCE_BUILD_EXAMPLES=OFF -DJUCE_BUILD_EXTRAS=OFF
if errorlevel 1 goto :error
cmake --build build --config Release --parallel 2
if errorlevel 1 goto :error
echo.
echo Build completed. Check build\YouGlitchSMP1024_artefacts\Release\
pause
exit /b 0
:error
echo.
echo BUILD FAILED. Copy the FIRST CMake/compiler error shown above.
pause
exit /b 1
