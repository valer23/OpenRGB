# Exercise the sleep-test wrapper without stopping real services or processes.
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.ServiceProcess
$repoRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source=Get-Content -LiteralPath (Join-Path $repoRoot 'scripts\Invoke-DeltaII-SleepTest.ps1') -Raw
$begin=$source.IndexOf('$principal=');$end=$source.IndexOf('$frameworkPath=')
if($begin -lt 0 -or $end -le $begin){throw 'Admin boundary changed'}
$source=$source.Remove($begin,$end-$begin)
$source=$source.Replace('& (Join-Path $PSScriptRoot ''Invoke-DeltaII-LightingTrial.ps1'')','Invoke-FakeSleepTrial')
$source=$source.Replace('$repoRoot=(Resolve-Path (Join-Path $PSScriptRoot ''..'')).Path','$repoRoot=$trialTestRoot')
$wrapper=[scriptblock]::Create($source)
function Get-Service {param($Name) if($Name -ne 'ArmouryCrateService'){throw 'Unexpected service'};return $script:controller}
function Get-CimInstance {
    param($ClassName,$Filter)
    if($ClassName -ne 'Win32_Process'){throw 'Unexpected metadata query'}
    $items=@($script:processes)
    if($Filter -match '^ProcessId=(\d+)$'){return @($items | Where-Object ProcessId -eq ([int]$Matches[1]))}
    return $items
}
function taskkill.exe {
    param($pidFlag,$targetPid,$treeFlag,$forceFlag)
    if($pidFlag -ne '/PID' -or $treeFlag -ne '/T' -or $forceFlag -ne '/F'){throw 'Unexpected process operation'}
    $script:kills+=@([int]$targetPid)
    $script:processes=@($script:processes | Where-Object ProcessId -ne ([int]$targetPid))
    $global:LASTEXITCODE=0
}
function Start-Process {
    param($FilePath,$WindowStyle)
    if($WindowStyle -ne 'Hidden'){throw 'Expected hidden helper launch'}
    $script:launches+=@($FilePath)
}
function Invoke-FakeSleepTrial {
    param([switch]$TemporarilyStopLightingService,$Mode,[switch]$ConfirmHeadsetAsleep,[switch]$AuthorizeSleepTest)
    if(-not $TemporarilyStopLightingService -or $Mode -ne 'SleepTest' -or -not $ConfirmHeadsetAsleep -or -not $AuthorizeSleepTest){throw 'Wrong sleep-trial arguments'}
    $script:trialCalls++
    if($script:failTrial){throw 'Injected trial failure'}
}
foreach($initial in @('Running','Stopped')){
    foreach($failure in @($false,$true)){
        $trialTestRoot=Join-Path $repoRoot ('build-battery\tests\sleep-wrapper-fixtures\'+[guid]::NewGuid().ToString())
        New-Item -ItemType Directory -Path (Join-Path $trialTestRoot 'build-battery\probe') -Force | Out-Null
        $script:controller=[pscustomobject]@{Status=[System.ServiceProcess.ServiceControllerStatus]::$initial;DependentServices=@();Stops=0;Starts=0}
        $script:controller | Add-Member ScriptMethod Stop {$this.Stops++;$this.Status=[System.ServiceProcess.ServiceControllerStatus]::Stopped}
        $script:controller | Add-Member ScriptMethod Start {$this.Starts++;$this.Status=[System.ServiceProcess.ServiceControllerStatus]::Running}
        $script:controller | Add-Member ScriptMethod Refresh {}
        $script:controller | Add-Member ScriptMethod WaitForStatus {param($expected,$timeout) if($this.Status -ne $expected){throw 'Unexpected service state'}}
        $script:processes=@([pscustomobject]@{Name='asus_framework.exe';ProcessId=123;ExecutablePath='C:\Program Files (x86)\ASUS\ArmouryDevice\asus_framework.exe';CommandLine='"C:\Program Files (x86)\ASUS\ArmouryDevice\asus_framework.exe"';CreationDate=[datetime]'2026-10-19T09:00:00'})
        $script:kills=@();$script:launches=@();$script:trialCalls=0;$script:failTrial=$failure
        & $wrapper -ConfirmHeadsetAsleep -AuthorizeOneBatteryQuery -TemporarilyCloseAsusReaders 6>$null
        if($script:controller.Status.ToString() -ne $initial){throw 'Original service state not restored'}
        $expectedChanges=if($initial -eq 'Running'){1}else{0}
        if($script:controller.Stops -ne $expectedChanges -or $script:controller.Starts -ne $expectedChanges){throw 'Unexpected service changes'}
        if($script:trialCalls -ne 1 -or $script:kills.Count -ne 1 -or $script:kills[0] -ne 123 -or $script:launches.Count -ne 1){throw 'Unexpected process/trial scope'}
    }
}
Write-Host 'Sleep-test wrapper: scoped process identity, original service state and failure restoration passed (4 cases).'
