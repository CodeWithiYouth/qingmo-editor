# SPDX-License-Identifier: MIT
# Run the already-built QingMo.
# Usage:  .\scripts\run.ps1
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root "build\bin\QingMo.exe"

if (-not (Test-Path $exe)) {
    throw "QingMo.exe not found. Build it first: .\scripts\build.ps1"
}

Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe)
