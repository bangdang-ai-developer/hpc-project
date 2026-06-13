$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $Root

$Source = "config/group_info.example.txt"
$Target = "config/group_info.txt"

if (!(Test-Path $Source)) {
    throw "Missing template: $Source"
}

if (Test-Path $Target) {
    Write-Host "$Target already exists. Opening it for editing..."
} else {
    Copy-Item $Source $Target
    Write-Host "Created $Target from $Source"
}

notepad $Target
