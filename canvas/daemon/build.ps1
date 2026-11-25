# Compila ojbsd per arm64-v8a con l'NDK.
#
#   .\build.ps1 -Ndk "C:\Users\Utente\AppData\Local\Android\Sdk\ndk\27.0.12077973"
#
# L'NDK porta il suo clang e il suo CMake toolchain: non serve altro compilatore
# sulla macchina. Ninja arriva con l'SDK in cmake\<versione>\bin.

param(
    [Parameter(Mandatory = $true)]
    [string]$Ndk,

    [string]$Abi = "arm64-v8a",

    [string]$Platform = "android-26",

    [string]$BuildDir = "build"
)

$ErrorActionPreference = "Stop"

$toolchain = Join-Path $Ndk "build\cmake\android.toolchain.cmake"
if (-not (Test-Path $toolchain)) {
    throw "toolchain non trovato: $toolchain"
}

$sourceDir = $PSScriptRoot

cmake -S $sourceDir -B (Join-Path $sourceDir $BuildDir) `
    -DCMAKE_TOOLCHAIN_FILE="$toolchain" `
    -DANDROID_ABI=$Abi `
    -DANDROID_PLATFORM=$Platform `
    -DCMAKE_BUILD_TYPE=Release

cmake --build (Join-Path $sourceDir $BuildDir) --config Release

$binary = Join-Path $sourceDir "$BuildDir\ojbsd"
if (Test-Path $binary) {
    Write-Output "binario pronto: $binary"
} else {
    throw "build finita ma ojbsd non trovato in $BuildDir"
}
