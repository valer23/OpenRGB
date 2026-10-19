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
    @{status='completed';owners=@()} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $trialTestRoot 'build-battery\probe\headset-handle-owner.json')
}
function Read-Host {param($Prompt) return 'no'}
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
foreach($initial in @('Running','Stopped')){
    foreach($failure in @($false,$true)){
    foreach($mode in @('Awake','SleepTest')){
        $trialTestRoot=Join-Path $repoRoot ('build-battery\tests\service-trial-fixtures\'+[guid]::NewGuid().ToString())
        $probeDir=Join-Path $trialTestRoot 'build-battery\probe'
        New-Item -ItemType Directory -Path $probeDir -Force | Out-Null
        New-Item -ItemType File -Path (Join-Path $probeDir 'BatteryStatusProbe.exe') | Out-Null
        $script:probeCalls=0;$script:failProbe=$failure
        $script:expectedArguments=if($mode -eq "SleepTest"){"--sleep-test --authorize-one-battery-query --confirm-headset-asleep"}else{"--awake-read --confirm-headset-on"}
        $script:controller=[pscustomobject]@{Status=[System.ServiceProcess.ServiceControllerStatus]::$initial;DependentServices=@();Stops=0;Starts=0}
        $script:controller | Add-Member ScriptMethod Stop {$this.Stops++;$this.Status=[System.ServiceProcess.ServiceControllerStatus]::Stopped}
        $script:controller | Add-Member ScriptMethod Start {$this.Starts++;$this.Status=[System.ServiceProcess.ServiceControllerStatus]::Running}
        $script:controller | Add-Member ScriptMethod Refresh {}
        $script:controller | Add-Member ScriptMethod WaitForStatus {param($expected,$timeout) if($this.Status -ne $expected){throw 'Unexpected service state'}}
        & $transaction -ConfirmHeadsetOn -TemporarilyStopLightingService -Mode $mode -ConfirmHeadsetAsleep -AuthorizeSleepTest 6>$null
        $result=Get-Content -LiteralPath (Join-Path $probeDir 'lighting-trial-result.json') -Raw | ConvertFrom-Json
        $expectedChanges=if($initial -eq 'Running'){1}else{0}
        if($script:controller.Stops -ne $expectedChanges -or $script:controller.Starts -ne $expectedChanges){throw "Incorrect service changes for $initial"}
        if($result.before -ne $initial -or $result.after -ne $initial -or $result.restoreError){throw "Initial state not preserved for $initial"}
        if($script:probeCalls -ne 1){throw 'Probe must have exactly one launch attempt'}
        if([bool]$result.error -ne $failure){throw 'Probe failure reporting mismatch'}
    }
    }
}
Write-Host 'Service trial: Running/Stopped initial states preserved on success and probe failure (8 cases, including single-request sleep-test routing).'
