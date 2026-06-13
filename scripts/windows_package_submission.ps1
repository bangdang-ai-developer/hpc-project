param(
    [string]$Distribution = "Ubuntu",
    [string]$LinuxUser = "root",
    [int]$SkipPreflight = 0
)

$ErrorActionPreference = "Stop"

$ProjectDir = "C:\Users\bangdang\project\Master\TinhToanHieuNangCao"
$LinuxProjectDir = "/mnt/c/Users/bangdang/project/Master/TinhToanHieuNangCao"

Write-Host "Packaging submission in WSL..."
Write-Host "Windows project: $ProjectDir"
Write-Host "WSL distro     : $Distribution"
Write-Host "SKIP_PREFLIGHT : $SkipPreflight"
Write-Host ""

$cmd = "cd '$LinuxProjectDir' && SKIP_PREFLIGHT='$SkipPreflight' bash scripts/package_submission.sh"
wsl.exe -d $Distribution -u $LinuxUser -- bash -lc $cmd
