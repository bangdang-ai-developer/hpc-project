param(
    [string]$Distribution = "Ubuntu",
    [string]$LinuxUser = "root",
    [string]$Shapes = "2048x2048x2048",
    [string]$NPList = "1 2 4 8",
    [int]$Repeat = 5,
    [int]$Fresh = 1
)

$ErrorActionPreference = "Stop"

$ProjectDir = "C:\Users\bangdang\project\Master\TinhToanHieuNangCao"
$LinuxProjectDir = "/mnt/c/Users/bangdang/project/Master/TinhToanHieuNangCao"

Write-Host "Running local benchmark/report pipeline in WSL..."
Write-Host "Windows project: $ProjectDir"
Write-Host "WSL distro     : $Distribution"
Write-Host "SHAPES         : $Shapes"
Write-Host "NP_LIST        : $NPList"
Write-Host "REPEAT         : $Repeat"
Write-Host "FRESH          : $Fresh"
Write-Host ""

$cmd = "cd '$LinuxProjectDir' && SHAPES='$Shapes' NP_LIST='$NPList' REPEAT='$Repeat' FRESH='$Fresh' bash scripts/run_local_pipeline.sh"
wsl.exe -d $Distribution -u $LinuxUser -- bash -lc $cmd
