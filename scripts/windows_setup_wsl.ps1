param(
    [string]$Distribution = "Ubuntu"
)

Write-Host "Checking WSL status..."
wsl.exe --status

$distros = (wsl.exe --list --quiet) 2>$null
if ($LASTEXITCODE -eq 0 -and $distros) {
    Write-Host "Installed WSL distributions:"
    $distros
    Write-Host ""
    Write-Host "Open Ubuntu and run:"
    Write-Host "  cd /mnt/c/Users/bangdang/project/Master/TinhToanHieuNangCao"
    Write-Host "  bash scripts/setup_ubuntu.sh"
    Write-Host "  bash scripts/check_local_env.sh"
    exit 0
}

Write-Host "No WSL Linux distribution found."
Write-Host "Run this PowerShell script as Administrator to install $Distribution:"
Write-Host "  wsl --install -d $Distribution"
Write-Host ""
Write-Host "If Windows asks for a reboot, reboot first, then open Ubuntu and run:"
Write-Host "  cd /mnt/c/Users/bangdang/project/Master/TinhToanHieuNangCao"
Write-Host "  bash scripts/setup_ubuntu.sh"
Write-Host "  bash scripts/check_local_env.sh"
