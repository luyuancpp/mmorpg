@echo off
chcp 437 >nul
setlocal

echo ==============================================
echo GRPC Build Script (parent-level launcher)
echo ==============================================

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..\..") do set "REPO_ROOT=%%~fI"
set "GRPC_DIR=%REPO_ROOT%\third_party\grpc"

:: Set path to CMake (prefer PATH; fallback to common install path)
set "CMAKE_EXE=cmake"
where %CMAKE_EXE% >nul 2>&1
if errorlevel 1 (
    set "CMAKE_EXE=C:\Program Files\CMake\bin\cmake.exe"
)

if not exist "%CMAKE_EXE%" (
    echo Error: CMake not found. Please install CMake or add it to PATH.
    pause
    exit /b 1
)

:: Check grpc source directory
if not exist "%GRPC_DIR%\CMakeLists.txt" (
    echo Error: grpc source not found at "%GRPC_DIR%"
    pause
    exit /b 1
)

:: Enter grpc root
pushd "%GRPC_DIR%"

:: Create build directory
if not exist ".build_vs2026" mkdir .build_vs2026
cd .build_vs2026

:: Generate VS2026 project files
echo [1/3] Generating VS2026 project files...
"%CMAKE_EXE%" -G "Visual Studio 18 2026" -A x64 -T v145 ^
      -DCMAKE_CXX_STANDARD=23 ^
      -DCMAKE_CXX_STANDARD_REQUIRED=ON ^
      -DCMAKE_CXX_EXTENSIONS=OFF ^
      -DGRPC_BUILD_TESTS=OFF ^
      -DGRPC_BUILD_EXAMPLES=OFF ^
      -DRE2_BUILD_TESTING=OFF ^
      -Dprotobuf_BUILD_TESTS=OFF ^
      -Dabsl_BUILD_TESTING=OFF ^
      ..

if %errorlevel% neq 0 (
    echo Error: Project generation failed!
    popd
    pause
    exit /b 1
)

:: Build GRPC
echo [2/3] Building GRPC...
"%CMAKE_EXE%" --build . --config Release -- /m /p:PlatformToolset=v145

if %errorlevel% neq 0 (
    echo Error: Build failed!
    popd
    pause
    exit /b 1
)

:: Install build artifacts
echo [3/3] Installing build artifacts...
"%CMAKE_EXE%" --install . --prefix ../install_vs2026 --config Release

if %errorlevel% neq 0 (
    echo Error: Install failed!
    popd
    pause
    exit /b 1
)

popd

echo ==============================================
echo Build completed successfully!
echo Output directory: %GRPC_DIR%\install_vs2026
echo ==============================================
pause
endlocal
