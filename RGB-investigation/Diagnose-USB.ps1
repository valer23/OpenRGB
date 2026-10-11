$ErrorActionPreference = 'Continue'
$diagnosticLog = Join-Path $PSScriptRoot 'USB-capture-diagnostic.txt'
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
('Administrator token: ' + $isAdmin) | Out-File -LiteralPath $diagnosticLog
& 'C:\Program Files\Wireshark\tshark.exe' -D 2>&1 | Out-File -LiteralPath $diagnosticLog -Append
foreach ($usbCaptureIndex in 1..4) {
    $usbCaptureInterface = '\\.\USBPcap' + $usbCaptureIndex
    $usbCaptureInterface | Out-File -LiteralPath $diagnosticLog -Append
    & 'C:\Program Files\Wireshark\extcap\USBPcapCMD.exe' --extcap-config --extcap-interface $usbCaptureInterface 2>&1 | Out-File -LiteralPath $diagnosticLog -Append
}
Get-CimInstance Win32_Process -Filter "Name='FanControl.exe'" | Select-Object ProcessId,ExecutablePath | Out-File -LiteralPath $diagnosticLog -Append
Write-Host 'USB device check saved. No settings changed.' -ForegroundColor Green
