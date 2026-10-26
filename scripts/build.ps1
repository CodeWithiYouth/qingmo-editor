# Build QingMo with the Visual Studio generator.
# Usage:  .\scripts\build.ps1
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$cmake = $null
$generator = $null
$appName = "QingMo"

# Prefer the CMake bundled with Visual Studio so no global install is required.
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    $vs = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath
    if ($vs) {
        $vs = $vs.Trim()
        $candidate = Join-Path $vs "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
        if (Test-Path $candidate) {
            $cmake = $candidate
            $major = Split-Path (Split-Path $vs -Parent) -Leaf
            $generator = switch ($major) {
                "18" { "Visual Studio 18 2026" }
                "17" { "Visual Studio 17 2022" }
                "16" { "Visual Studio 16 2019" }
                default { $null }
            }
        }
    }
}

if (-not $cmake) {
    $cmd = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmd) { $cmake = $cmd.Source }
}
if (-not $cmake) {
    throw "CMake not found. Install 'Desktop development with C++' in Visual Studio, or install CMake 3.25+."
}

$configureArgs = @("-S", $root, "-B", "$root\build", "-DAPP_NAME=$appName")
if ($generator) {
    $configureArgs += @("-G", $generator, "-A", "x64")
}

Write-Host "Configuring with: $cmake"
& $cmake @configureArgs
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

& $cmake --build "$root\build" --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw "Build failed." }

Write-Host ""
Write-Host "Built: $root\build\bin\$appName.exe" -ForegroundColor Green
