param(
    [string]$QtDir = "E:/Software/qt/qt/6.9.1/mingw_64",
    [string]$CompilerDir = "E:/Software/qt/qt/Tools/mingw1310_64/bin",
    [string]$BuildDir = "build"
)

$ErrorActionPreference = "Stop"

cmake -S $PSScriptRoot -B (Join-Path $PSScriptRoot $BuildDir) -G Ninja `
    -DCMAKE_PREFIX_PATH="$QtDir" `
    -DCMAKE_CXX_COMPILER="$CompilerDir/g++.exe" `
    -DCMAKE_BUILD_TYPE=Release

if ($LASTEXITCODE -ne 0) {
    throw "CMake configuration failed."
}

cmake --build (Join-Path $PSScriptRoot $BuildDir) --parallel
if ($LASTEXITCODE -ne 0) {
    throw "Build failed."
}

$exe = Join-Path $PSScriptRoot "$BuildDir/WeStudio.exe"
& "$QtDir/bin/windeployqt.exe" --release --no-translations `
    --no-opengl-sw --compiler-runtime $exe

if ($LASTEXITCODE -ne 0) {
    throw "Qt deployment failed."
}

Write-Host ""
Write-Host "Built: $exe"

