# Exercise the real transaction with OS/device boundaries replaced in memory.
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.ServiceProcess
$repoRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source=Get-Content -LiteralPath (Join-Path $repoRoot 'scripts\Invoke-DeltaII-LightingTrial.ps1') -Raw
$begin=$source.IndexOf('$principal=')
$end=$source.IndexOf('$repoRoot=')
if($begin -lt 0 -or $end -le $begin){throw 'Admin boundary changed'}
$source=$source.Remove($begin,$end-$begin)
$source=$source.Replace('$repoRoot=(Resolve-Path (Join-Path $PSScriptRoot ''..'')).Path','$repoRoot=$trialTestRoot')
$source=$source.Replace('& (Join-Path $PSScriptRoot ''Find-DeltaII-Owner.ps1'')','Invoke-FakeOwnerScan')
if($source -match 'WindowsPrincipal|Find-DeltaII-Owner.ps1|Resolve-Path'){throw 'An unmocked OS boundary remains'}
$transaction=[scriptblock]::Create($source)
function Get-Service {param($Name) if($Name -ne 'LightingService'){throw 'Unexpected service'};return $script:controller}
function Get-CimInstance {param($ClassName,$Filter) return [pscustomobject]@{ProcessId=0}}
function Invoke-FakeOwnerScan {
    if($script:controller.Status.ToString() -notin @('Running','Stopped')){throw 'Owner scan must follow service stabilization'}
    @{status='completed';owners=@()} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $trialTestRoot 'build-battery\probe\headset-handle-owner.json')
}
function Read-Host {
    param($Prompt)
    if($script:controller.Status -ne [System.ServiceProcess.ServiceControllerStatus]::Stopped){throw 'Observation must precede service restoration'}
    if($script:answers.Count -eq 0){throw 'Unexpected observation prompt'}
    return $script:answers.Dequeue()
}
function Start-Process {
    param($FilePath,$ArgumentList,$WindowStyle,[switch]$PassThru,$RedirectStandardOutput,$RedirectStandardError)
    $script:probeCalls++
    if(($ArgumentList -join " ") -ne $script:expectedArguments){throw "Unexpected probe arguments"}
    if($script:failProbe){throw 'Injected probe launch failure'}
    '{"percentage":42,"charging":false,"level_query_attempted":true}' | Set-Content -LiteralPath $RedirectStandardOutput
    $child=[pscustomobject]@{ExitCode=0}
    $child | Add-Member ScriptMethod WaitForExit {param($timeout) if($timeout -ne 6000){throw 'Unexpected deadline'};return $true}
    return $child
}
foreach($initial in @('Running','Stopped','StartPending','StopPending')){
    $stable=if($initial -eq 'StartPending'){'Running'}elseif($initial -eq 'StopPending'){'Stopped'}else{$initial}
    foreach($failure in @($false,$true)){
    foreach($mode in @('Awake','SleepTest')){
        $trialTestRoot=Join-Path $repoRoot ('build-battery\tests\service-trial-fixtures\'+[guid]::NewGuid().ToString())
        $probeDir=Join-Path $trialTestRoot 'build-battery\probe'
        New-Item -ItemType Directory -Path $probeDir -Force | Out-Null
        New-Item -ItemType File -Path (Join-Path $probeDir 'BatteryStatusProbe.exe') | Out-Null
        $script:probeCalls=0;$script:failProbe=$failure
        $script:answers=[System.Collections.Queue]::new();$script:answers.Enqueue('no')
        $script:expectedArguments=if($mode -eq "SleepTest"){"--sleep-test --authorize-one-battery-query --confirm-headset-asleep"}else{"--awake-read --confirm-headset-on"}
        $script:controller=[pscustomobject]@{Status=[System.ServiceProcess.ServiceControllerStatus]::$initial;DependentServices=@();Stops=0;Starts=0}
        $script:controller | Add-Member ScriptMethod Stop {$this.Stops++;$this.Status=[System.ServiceProcess.ServiceControllerStatus]::Stopped}
        $script:controller | Add-Member ScriptMethod Start {$this.Starts++;$this.Status=[System.ServiceProcess.ServiceControllerStatus]::Running}
        $script:controller | Add-Member ScriptMethod Refresh {}
        $script:controller | Add-Member ScriptMethod WaitForStatus {param($expected,$timeout) if($this.Status.ToString() -in @('StartPending','StopPending')){$this.Status=$expected}; if($this.Status -ne $expected){throw 'Unexpected service state'}}
        & $transaction -ConfirmHeadsetOn -TemporarilyStopLightingService -Mode $mode -ConfirmHeadsetAsleep -AuthorizeSleepTest 6>$null
        $result=Get-Content -LiteralPath (Join-Path $probeDir 'lighting-trial-result.json') -Raw | ConvertFrom-Json
        $expectedChanges=if($stable -eq 'Running'){1}else{0}
        if($script:controller.Stops -ne $expectedChanges -or $script:controller.Starts -ne $expectedChanges){throw "Incorrect service changes for $initial"}
        if($result.before -ne $stable -or $result.after -ne $stable -or $result.restoreError){throw "Initial state not preserved for $initial"}
        if($script:probeCalls -ne 1){throw 'Probe must have exactly one launch attempt'}
        if([bool]$result.error -ne $failure){throw 'Probe failure reporting mismatch'}
    }
    }
}
foreach($initial in @('Running','Stopped','StartPending','StopPending')){
    $stable=if($initial -eq 'StartPending'){'Running'}elseif($initial -eq 'StopPending'){'Stopped'}else{$initial}
    foreach($outcome in @('success','failure','cancel')){
        $trialTestRoot=Join-Path $repoRoot ('build-battery\tests\service-trial-fixtures\'+[guid]::NewGuid().ToString())
        $probeDir=Join-Path $trialTestRoot 'build-battery\probe'
        New-Item -ItemType Directory -Path $probeDir -Force | Out-Null
        New-Item -ItemType File -Path (Join-Path $probeDir 'BatteryStatusProbe.exe') | Out-Null
        $script:probeCalls=0;$script:failProbe=($outcome -eq 'failure')
        $script:answers=[System.Collections.Queue]::new()
        $script:answers.Enqueue($(if($outcome -eq 'cancel'){'cancel'}else{'off'}));$script:answers.Enqueue('no')
        $script:expectedArguments='--sleep-test --authorize-one-battery-query --confirm-headset-asleep'
        $script:controller=[pscustomobject]@{Status=[System.ServiceProcess.ServiceControllerStatus]::$initial;DependentServices=@();Stops=0;Starts=0}
        $script:controller | Add-Member ScriptMethod Stop {$this.Stops++;$this.Status=[System.ServiceProcess.ServiceControllerStatus]::Stopped}
        $script:controller | Add-Member ScriptMethod Start {$this.Starts++;$this.Status=[System.ServiceProcess.ServiceControllerStatus]::Running}
        $script:controller | Add-Member ScriptMethod Refresh {}
        $script:controller | Add-Member ScriptMethod WaitForStatus {param($expected,$timeout) if($this.Status.ToString() -in @('StartPending','StopPending')){$this.Status=$expected}; if($this.Status -ne $expected){throw 'Unexpected service state'}}
        & $transaction -TemporarilyStopLightingService -Mode SleepTest -WaitForAutoPowerOff -AuthorizeSleepTest 6>$null
        $result=Get-Content -LiteralPath (Join-Path $probeDir 'lighting-trial-result.json') -Raw | ConvertFrom-Json
        $expectedChanges=if($stable -eq 'Running'){1}else{0}
        if($script:controller.Stops -ne $expectedChanges -or $script:controller.Starts -ne $expectedChanges -or $result.after -ne $stable -or $result.restoreError){throw 'Observed baseline must preserve service state'}
        $expectedCalls=if($outcome -eq 'cancel'){0}else{1}
        if($script:probeCalls -ne $expectedCalls){throw 'No query allowed without observed natural power-off; otherwise exactly one'}
        if([bool]$result.error -ne ($outcome -ne 'success')){throw 'Observed-baseline outcome mismatch'}
        if([bool]$result.baselineLightsOff -ne ($outcome -ne 'cancel')){throw 'Baseline must record the actual owner confirmation'}
    }
}
$script:controller.Status=[System.ServiceProcess.ServiceControllerStatus]::StartPending
$script:controller.Stops=0;$script:controller.Starts=0;$script:probeCalls=0;$script:waitCalls=0
$script:controller | Add-Member -Force ScriptMethod WaitForStatus {
    param($expected,$timeout)
    if($timeout.TotalSeconds -ne 10){throw 'Transition wait must be bounded to ten seconds'}
    $script:waitCalls++;throw 'Injected transition timeout'
}
$timedOut=$false
try { & $transaction -TemporarilyStopLightingService -Mode SleepTest -WaitForAutoPowerOff -AuthorizeSleepTest 6>$null }
catch { $timedOut=$_.Exception.Message -like '*Injected transition timeout*' }
if(-not $timedOut -or $script:waitCalls -ne 1 -or $script:probeCalls -ne 0 -or $script:controller.Stops -ne 0 -or $script:controller.Starts -ne 0){throw 'Unsettled transition must abort without a service command or query'}
Write-Host 'Service trial: state preservation, observed-light baseline, cancellation and bounded transition waits passed (29 cases).'