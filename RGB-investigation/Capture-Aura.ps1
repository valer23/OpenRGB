$ErrorActionPreference = 'Stop'
$captureFile = Join-Path $PSScriptRoot ('Aura-colours-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '.pcap')
Write-Host 'ASUS AURA capture: USBPcap4, device address 4 only.'
Write-Host 'Keep OpenRGB closed. FanControl can remain running.'
Write-Host 'In Aura, select Static / Single. Set RED, GREEN, BLUE, then RED again.'
Write-Host 'Wait 5 seconds between each colour and note whether the motherboard logo matches.'
Write-Host 'When finished, return here and press Ctrl+C to stop recording.'
Write-Host ('Saving to: ' + $captureFile)
& 'C:\Program Files\USBPcap\USBPcapCMD.exe' -d '\\.\USBPcap4' --devices 4 --inject-descriptors -o $captureFile
Write-Host 'Recording ended.'
Read-Host 'Press Enter to close'
