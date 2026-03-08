param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "Profile", "Retail")]
    [string]$Config = "Debug",
    [string]$BuildDir = "",
    [string]$PackageDir = "",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$sourceDir = Join-Path $root "tools\diligent"
$defaultBuildDir = Join-Path $root ("out\build\diligent-" + $Config.ToLowerInvariant())

if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = $defaultBuildDir
}

if ([string]::IsNullOrWhiteSpace($PackageDir)) {
    $PackageDir = Join-Path $BuildDir "package"
}

if ($Clean -and (Test-Path $BuildDir)) {
    Remove-Item -Recurse -Force $BuildDir
}

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
New-Item -ItemType Directory -Force -Path $PackageDir | Out-Null

$toolchainFile = Join-Path $root "cmake\toolchains\llvm-windows.cmake"
$cmakeArgs = @(
    "-S", $sourceDir,
    "-B", $BuildDir,
    "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=$Config",
    "-DHIVE_ROOT_DIR=$root",
    "-DHIVE_DILIGENT_EXPORT_DIR=$PackageDir"
)

if (Test-Path $toolchainFile) {
    $cmakeArgs += "-DCMAKE_TOOLCHAIN_FILE=$toolchainFile"
}

$rcCompiler = Get-ChildItem `
    -Path 'C:\Program Files (x86)\Windows Kits\10\bin' `
    -Filter rc.exe `
    -Recurse `
    -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -like '*\\x64\\rc.exe' } |
    Sort-Object FullName -Descending |
    Select-Object -First 1 -ExpandProperty FullName

if ($rcCompiler) {
    $cmakeArgs += "-DCMAKE_RC_COMPILER=$rcCompiler"
}

$compilerLauncher = $null
if (Get-Command sccache -ErrorAction SilentlyContinue) {
    $compilerLauncher = (Get-Command sccache).Source
} elseif (Get-Command ccache -ErrorAction SilentlyContinue) {
    $compilerLauncher = (Get-Command ccache).Source
}

if ($compilerLauncher) {
    $cmakeArgs += "-DCMAKE_C_COMPILER_LAUNCHER=$compilerLauncher"
    $cmakeArgs += "-DCMAKE_CXX_COMPILER_LAUNCHER=$compilerLauncher"
}

& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& cmake --build $BuildDir --config $Config --target HiveDiligentPackage
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host "HiveDiligent package ready:"
Write-Host "  Config:  $Config"
Write-Host "  Build:   $BuildDir"
Write-Host "  Package: $PackageDir"
Write-Host "Use -DHIVE_DILIGENT_PROVIDER=PREBUILT to require this package, or leave AUTO for fallback."
