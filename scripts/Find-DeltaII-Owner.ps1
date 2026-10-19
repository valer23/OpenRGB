# Read-only diagnostic. Does not stop a service/process, close another handle,
# or open a HID data reader. Requires Microsoft's signed portable Handle utility.
$ErrorActionPreference='Stop'
$principal=[Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
if(-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)){
    Write-Host 'Run this script in your existing administrator PowerShell window. Nothing was changed.' -ForegroundColor Yellow
    exit 2
}
$repoRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$probeRoot=Join-Path $repoRoot 'build-battery\probe'
$utility=Join-Path $probeRoot 'tools\handle\handle64.exe'
if(-not(Test-Path -LiteralPath $utility)){throw 'Prepared Microsoft Handle utility is missing'}
$signature=Get-AuthenticodeSignature -LiteralPath $utility
if($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'Microsoft Corporation'){
    throw 'Microsoft signature validation failed; utility will not run'
}
$devices=@(Get-PnpDevice -PresentOnly | Where-Object InstanceId -Like 'HID\VID_0B05&PID_1AFA&MI_03&COL04\*')
if($devices.Count -ne 1){throw 'Exact Delta II collection is absent or ambiguous; no handle scan performed'}
$pdo=[string](Get-PnpDeviceProperty -InstanceId $devices[0].InstanceId -KeyName 'DEVPKEY_Device_PDOName').Data
if([string]::IsNullOrWhiteSpace($pdo) -or -not $pdo.StartsWith('\Device\')){throw 'Cached device-object name is invalid; no scan performed'}
$log=Join-Path $probeRoot 'headset-handle-owner.log'
Write-Host "Looking up handles for $pdo only. No handles will be closed."
Write-Host 'Microsoft Handle may show its license dialog on first use; review it to continue.'
$output=@(& $utility -a -g $pdo 2>&1)
$output | Out-String | Set-Content -LiteralPath $log -Encoding utf8
$output | ForEach-Object {Write-Host $_}
$owners=@()
foreach($line in $output){
    if([string]$line -match '\bpid:\s*(\d+)'){
        $ownerPid=[int]$Matches[1]
        $process=Get-Process -Id $ownerPid -ErrorAction SilentlyContinue
        $services=@(Get-CimInstance Win32_Service -Filter "ProcessId=$ownerPid" | Select-Object -ExpandProperty Name)
        $owners+=@{pid=$ownerPid;process=$process.ProcessName;services=$services}
    }
}
@{deviceInstance=$devices[0].InstanceId;deviceObject=$pdo;owners=$owners;commandsSent=0;servicesChanged=@();handlesClosed=@();log=$log} |
    ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $probeRoot 'headset-handle-owner.json') -Encoding utf8
Write-Host "Saved lookup: $log"
