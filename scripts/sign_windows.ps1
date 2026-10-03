# Codesigns the Windows VST3 build with signtool. Requires a code-signing
# certificate (.pfx, EV or standard) and its password.
#
# Usage:
#   .\scripts\sign_windows.ps1 -PfxPath C:\certs\onyva.pfx -PfxPassword "..." [-BuildDir build] [-Config Release]

param(
    [Parameter(Mandatory=$true)][string]$PfxPath,
    [Parameter(Mandatory=$true)][string]$PfxPassword,
    [string]$BuildDir = "build",
    [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"

$vst3Path = Join-Path $BuildDir "ONYKey_artefacts\$Config\VST3\ONY Key.vst3\Contents\x86_64-win\ONY Key.vst3"

if (-not (Test-Path $vst3Path)) {
    Write-Error "VST3 binary not found at $vst3Path — build ONYKey_VST3 first."
}

signtool sign /f "$PfxPath" /p "$PfxPassword" /fd sha256 /tr http://timestamp.digicert.com /td sha256 "$vst3Path"
signtool verify /pa "$vst3Path"

Write-Host "Signed: $vst3Path"
