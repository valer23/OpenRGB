# One explicit owner-observed sleep experiment. Never used by the plugin.
param([switch]$WaitForAutoPowerOff,[switch]$AuthorizeOneBatteryQuery,[switch]$TemporarilyCloseAsusReaders)
$ErrorActionPreference='Stop'
if(-not $WaitForAutoPowerOff -or -not $AuthorizeOneBatteryQuery -or -not $TemporarilyCloseAsusReaders){throw 'Requires -WaitForAutoPowerOff, one-query authorization and temporary ASUS reader closure; nothing changed'}
$principal=[Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
if(-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)){throw 'Use administrator PowerShell; nothing changed'}
$frameworkPath='C:\Program Files (x86)\ASUS\ArmouryDevice\asus_framework.exe'
$socketPath='C:\Program Files (x86)\ASUS\ArmouryDevice\dll\ArmourySocketServer\ArmourySocketServer.exe'
$repoRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$resultPath=Join-Path $repoRoot 'build-battery\probe\sleep-test-restoration.json'
$controller=Get-Service -Name ArmouryCrateService
$initialServiceState=$controller.Status.ToString()
if($initialServiceState -notin @('Running','Stopped')){throw 'ArmouryCrateService is transitioning; nothing changed'}
if(@($controller.DependentServices).Count){throw 'Unexpected dependent services; nothing changed'}
$processes=@(Get-CimInstance Win32_Process -Filter "Name='asus_framework.exe' OR Name='ArmourySocketServer.exe'")
foreach($process in $processes){
    $expected=if($process.Name -eq 'ArmourySocketServer.exe'){$socketPath}else{$frameworkPath}
    if($process.ExecutablePath -ne $expected){throw 'ASUS process path is unexpected; nothing changed'}
}
$socketRoots=@($processes | Where-Object {$_.Name -eq 'ArmourySocketServer.exe'})
$frameworkRoots=@($processes | Where-Object {$_.Name -eq 'asus_framework.exe' -and $_.CommandLine -and $_.CommandLine.Trim() -eq ('"'+$frameworkPath+'"')})
foreach($socket in $socketRoots){if(-not $socket.CommandLine -or $socket.CommandLine.Trim() -ne ('"'+$socketPath+'"')){throw 'Socket-server launch arguments are unexpected; nothing changed'}}
$roots=@($socketRoots)+@($frameworkRoots)
$result=[ordered]@{serviceBefore=$initialServiceState;serviceAfter=$null;serviceStopRequested=$false;processClosureRequested=$false;closedRootPids=@();relaunchRequested=@();trialInvoked=$false;error=$null;restoreErrors=@()}
try {
    if($initialServiceState -eq 'Running'){
        $controller.Stop();$result.serviceStopRequested=$true
        $controller.WaitForStatus([System.ServiceProcess.ServiceControllerStatus]::Stopped,[TimeSpan]::FromSeconds(10))
    }
    foreach($root in $roots){
        $current=@(Get-CimInstance Win32_Process -Filter "ProcessId=$($root.ProcessId)")
        if(-not $current.Count){continue}
        if($current.Count -ne 1 -or $current[0].ExecutablePath -ne $root.ExecutablePath -or $current[0].CreationDate -ne $root.CreationDate -or $current[0].CommandLine -ne $root.CommandLine){throw 'ASUS process identity changed; no kill issued for that PID'}
        $result.processClosureRequested=$true
        & taskkill.exe /PID $root.ProcessId /T /F | Write-Host
        if($LASTEXITCODE -ne 0){throw 'ASUS process closure failed; no probe issued'}
        $result.closedRootPids+=@($root.ProcessId)
    }
    Write-Host 'ASUS readers released. The inner trial will wait for an observed natural lights-off baseline before its one query.'
    $result.trialInvoked=$true
    & (Join-Path $PSScriptRoot 'Invoke-DeltaII-LightingTrial.ps1') -TemporarilyStopLightingService -Mode SleepTest -WaitForAutoPowerOff -AuthorizeSleepTest
} catch {$result.error=$_.Exception.Message}
finally {
    if($result.serviceStopRequested){
        try {
            $controller.Refresh()
            if($controller.Status -eq [System.ServiceProcess.ServiceControllerStatus]::StopPending){$controller.WaitForStatus([System.ServiceProcess.ServiceControllerStatus]::Stopped,[TimeSpan]::FromSeconds(10));$controller.Refresh()}
            if($controller.Status -eq [System.ServiceProcess.ServiceControllerStatus]::Stopped){$controller.Start()}
            $controller.WaitForStatus([System.ServiceProcess.ServiceControllerStatus]::Running,[TimeSpan]::FromSeconds(10))
        } catch {$result.restoreErrors+=@($_.Exception.Message)}
    }
    $controller.Refresh();$result.serviceAfter=$controller.Status.ToString()
    if($result.processClosureRequested){
        # Restore the original launcher, rather than replaying transient child IPC arguments.
        $launchers=if($socketRoots.Count){@($socketPath)}elseif($frameworkRoots.Count){@($frameworkPath)}else{@()}
        foreach($launcher in $launchers){
            try {
                $running=@(Get-CimInstance Win32_Process -Filter "Name='asus_framework.exe' OR Name='ArmourySocketServer.exe'" | Where-Object {$_.ExecutablePath -eq $launcher -and $_.CommandLine -and $_.CommandLine.Trim() -eq ('"'+$launcher+'"')})
                if(-not $running.Count){Start-Process -FilePath $launcher -WindowStyle Hidden;$result.relaunchRequested+=@($launcher)}
            } catch {$result.restoreErrors+=@($_.Exception.Message)}
        }
    }
    $result | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $resultPath -Encoding utf8
}
$result | ConvertTo-Json -Depth 10 | Write-Host
if($result.restoreErrors.Count -or $result.serviceAfter -ne $result.serviceBefore){throw "Restoration needs attention; inspect $resultPath before closing this window"}
Write-Host 'Original service state restored; ASUS launcher relaunch requested where needed. Reopen Armoury to restore its device pages.'
