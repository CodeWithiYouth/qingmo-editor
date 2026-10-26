# Run the already-built GllEdit.
# Usage:  .\scripts\run.ps1
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root "build\bin\GllEdit.exe"

if (-not (Test-Path $exe)) {
    throw "GllEdit.exe not found. Build it first: .\scripts\build.ps1"
}

Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe)
