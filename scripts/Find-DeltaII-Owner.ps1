# Read-only diagnostic. Does not stop a service/process, close another handle,
# or open a HID data reader. Requires Microsoft's signed portable Handle utility.
# Specify this switch only after reviewing and accepting Microsoft's Handle license.
param([switch]$AcceptHandleLicense)
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
$handleArguments=@('-a','-g',$pdo)
if($AcceptHandleLicense){$handleArguments=@('-accepteula')+$handleArguments}
$output=@(& $utility @handleArguments 2>&1)
$handleExitCode=$LASTEXITCODE
# Handle's redirected first-run output can contain embedded UTF-16 NULs.
$output=@($output | ForEach-Object {([string]$_).Replace([string][char]0,'')})
$output | Out-String | Set-Content -LiteralPath $log -Encoding utf8
$output | ForEach-Object {Write-Host $_}
. (Join-Path $PSScriptRoot 'BatteryHandleResult.ps1')
$status=Get-BatteryHandleScanStatus -ExitCode $handleExitCode -Output $output
$licenseRequired=$status -eq 'license-required'
if($status -ne 'completed'){
    @{status=$status;exitCode=$handleExitCode;owners=$null;commandsSent=0;servicesChanged=@();handlesClosed=@();log=$log} |
        ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $probeRoot 'headset-handle-owner.json') -Encoding utf8
    if($licenseRequired){throw 'Owner scan did not run. Review the displayed Microsoft license; if you accept it, rerun with -AcceptHandleLicense.'}
    throw "Owner scan failed (exit $handleExitCode); see $log"
}
$owners=@()
foreach($line in $output){
    if([string]$line -match '\bpid:\s*(\d+)'){
        $ownerPid=[int]$Matches[1]
        $process=Get-Process -Id $ownerPid -ErrorAction SilentlyContinue
        $services=@(Get-CimInstance Win32_Service -Filter "ProcessId=$ownerPid" | Select-Object -ExpandProperty Name)
        $launch=Get-CimInstance Win32_Process -Filter "ProcessId=$ownerPid"
        $ancestry=@()
        $parentPid=[int]$launch.ParentProcessId
        $seen=@($ownerPid)
        for($depth=0;$depth -lt 5 -and $parentPid -gt 0 -and $parentPid -notin $seen;$depth++){
            $parent=Get-CimInstance Win32_Process -Filter "ProcessId=$parentPid"
            if(-not $parent){break}
            $parentServices=@(Get-CimInstance Win32_Service -Filter "ProcessId=$parentPid" | Select-Object -ExpandProperty Name)
            $ancestry+=@{pid=$parentPid;process=$parent.Name;executable=$parent.ExecutablePath;commandLine=$parent.CommandLine;services=$parentServices}
            $seen+=@($parentPid)
            $parentPid=[int]$parent.ParentProcessId
        }
        $owners+=@{pid=$ownerPid;process=$process.ProcessName;services=$services;executable=$launch.ExecutablePath;commandLine=$launch.CommandLine;ancestors=$ancestry}
    }
}
@{status='completed';deviceInstance=$devices[0].InstanceId;deviceObject=$pdo;owners=$owners;commandsSent=0;servicesChanged=@();handlesClosed=@();log=$log} |
    ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $probeRoot 'headset-handle-owner.json') -Encoding utf8
Write-Host "Saved lookup: $log"
