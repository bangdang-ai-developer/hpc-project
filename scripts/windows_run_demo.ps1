param(
    [string]$Distribution = "Ubuntu",
    [string]$LinuxUser = "root"
)

$ErrorActionPreference = "Stop"

$ProjectDir = "C:\Users\bangdang\project\Master\TinhToanHieuNangCao"
$LinuxProjectDir = "/mnt/c/Users/bangdang/project/Master/TinhToanHieuNangCao"

Write-Host "Running MPI interactive demo in WSL..."
Write-Host "Windows project: $ProjectDir"
Write-Host "WSL distro     : $Distribution"
Write-Host ""

wsl.exe -d $Distribution -u $LinuxUser -- bash -lc "cd '$LinuxProjectDir' && bash scripts/demo.sh"
