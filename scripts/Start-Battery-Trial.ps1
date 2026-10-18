param([switch]$Hardware)
$ErrorActionPreference='Stop'
$repoRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$trialRoot=Join-Path $repoRoot 'build-battery\trial'
$exe=Join-Path $trialRoot 'OpenRGB-Battery-Trial.exe'
if(-not(Test-Path -LiteralPath $exe)){throw 'Prepare the isolated trial first'}
if($Hardware){
    $other=Get-Process -Name '*OpenRGB*' -ErrorAction SilentlyContinue
    if($other){throw 'Exit other OpenRGB instances from their tray menus before starting the hardware trial. Fan Control should stay running.'}
}
$arguments=@('--gui','--noautoconnect','--config',('"'+(Join-Path $trialRoot 'Settings')+'"'),'--loglevel','6')
if(-not $Hardware){$arguments+='--nodetect'}
$process=Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory $trialRoot -WindowStyle Hidden -PassThru
Write-Output "Started isolated trial PID $($process.Id). Hardware detection: $Hardware. Exit this trial from its tray when finished; no installed files are replaced."
