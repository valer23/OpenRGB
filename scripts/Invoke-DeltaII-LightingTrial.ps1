# Explicit, one-shot diagnostic; never changes ASUS framework or OpenRGB processes.
param([switch]$ConfirmHeadsetOn,[switch]$TemporarilyStopLightingService,[switch]$DiagnoseOnly)
$ErrorActionPreference='Stop'
if((-not $ConfirmHeadsetOn -and -not $DiagnoseOnly) -or -not $TemporarilyStopLightingService){throw 'Requires -TemporarilyStopLightingService plus either -ConfirmHeadsetOn or -DiagnoseOnly. No changes made.'}
$principal=[Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
if(-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)){throw 'Use administrator PowerShell. No changes made.'}
$repoRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$probeRoot=Join-Path $repoRoot 'build-battery\probe'
$probe=Join-Path $probeRoot 'BatteryStatusProbe.exe'
if(-not(Test-Path -LiteralPath $probe)){throw 'Prepared probe is missing'}
# Refresh exact-PDO ownership; do not stop anything if another reader remains.
& (Join-Path $PSScriptRoot 'Find-DeltaII-Owner.ps1')
$ownership=Get-Content -LiteralPath (Join-Path $probeRoot 'headset-handle-owner.json') -Raw | ConvertFrom-Json
if($ownership.status -ne 'completed'){throw 'Owner scan did not complete; nothing stopped'}
$controller=Get-Service -Name LightingService
$serviceInfo=Get-CimInstance Win32_Service -Filter "Name='LightingService'"
$otherOwners=@($ownership.owners | Where-Object { $_.pid -ne $serviceInfo.ProcessId -or $_.process -ne 'LightingService' })
if($otherOwners.Count -and -not $DiagnoseOnly){throw 'Another reader still holds the headset. Close the Armoury device page/app normally, then retry this script. No service or device changes made.'}
if($controller.Status -ne [System.ServiceProcess.ServiceControllerStatus]::Running){throw 'LightingService must initially be Running; no changes made'}
if(@($controller.DependentServices).Count){throw 'Unexpected dependent services; no changes made'}
$runId='lighting-trial-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff')
$stdout=Join-Path $probeRoot ($runId+'.json')
$stderr=Join-Path $probeRoot ($runId+'.stderr.log')
$resultPath=Join-Path $probeRoot 'lighting-trial-result.json'
$result=[ordered]@{service='LightingService';before='Running';diagnoseOnly=[bool]$DiagnoseOnly;ownersWhileStopped=$null;stopRequested=$false;probeAttempted=$false;probeExitCode=$null;probeResult=$null;error=$null;restoreError=$null;after=$null;stdout=$stdout;stderr=$stderr}
$previousPath=$env:PATH
try {
    $env:PATH='C:\Users\valerian\git-repos\OpenRGB-KFA2\RGB-investigation\toolchain\Tools\mingw1310_64\bin;'+$env:PATH
    Write-Host 'Temporarily stopping LightingService. Leave this window open until restoration finishes.'
    $controller.Stop();$result.stopRequested=$true
    $controller.WaitForStatus([System.ServiceProcess.ServiceControllerStatus]::Stopped,[TimeSpan]::FromSeconds(10))
    & (Join-Path $PSScriptRoot 'Find-DeltaII-Owner.ps1')
    $during=Get-Content -LiteralPath (Join-Path $probeRoot 'headset-handle-owner.json') -Raw | ConvertFrom-Json
    if($during.status -ne 'completed'){throw 'During-stop ownership scan failed; no battery query issued'}
    $result.ownersWhileStopped=@($during.owners)
    $during | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath (Join-Path $probeRoot ($runId+'-owners.json')) -Encoding utf8
    if(-not $DiagnoseOnly){
        if(@($during.owners).Count){throw 'A reader holds the headset while LightingService is stopped; no battery query issued'}
    $child=Start-Process -FilePath $probe -ArgumentList @('--awake-read','--confirm-headset-on') -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    $result.probeAttempted=$true
    if(-not $child.WaitForExit(6000)){$child.Kill();$child.WaitForExit();throw 'Six-second deadline exceeded; no retry'}
    $result.probeExitCode=$child.ExitCode
    $result.probeResult=Get-Content -LiteralPath $stdout -Raw | ConvertFrom-Json
    }
} catch {$result.error=$_.Exception.Message}
finally {
    $env:PATH=$previousPath
    if($result.stopRequested){
        Write-Host 'Restoring LightingService...'
        try {
            $controller.Refresh()
            if($controller.Status -eq [System.ServiceProcess.ServiceControllerStatus]::StopPending){$controller.WaitForStatus([System.ServiceProcess.ServiceControllerStatus]::Stopped,[TimeSpan]::FromSeconds(10));$controller.Refresh()}
            if($controller.Status -eq [System.ServiceProcess.ServiceControllerStatus]::Stopped){$controller.Start()}
            $controller.WaitForStatus([System.ServiceProcess.ServiceControllerStatus]::Running,[TimeSpan]::FromSeconds(10))
        } catch {$result.restoreError=$_.Exception.Message}
    }
    $controller.Refresh();$result.after=$controller.Status.ToString()
    $result | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $resultPath -Encoding utf8
}
$result | ConvertTo-Json -Depth 20 | Write-Host
if($result.restoreError -or $result.after -ne 'Running'){throw 'RESTORATION FAILED: start LightingService in Windows Services before closing this window'}
Write-Host "LightingService restored. Result saved to $resultPath"
