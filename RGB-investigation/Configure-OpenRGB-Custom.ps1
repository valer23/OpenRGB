$ErrorActionPreference = 'Stop'
$appRoot = 'C:\Users\valerian\Apps\OpenRGB-Custom'
$settingsRoot = 'C:\Users\valerian\Apps\OpenRGB-Custom\Settings'
$exe = Join-Path $appRoot 'OpenRGB-Custom.exe'
$resultFile = Join-Path $settingsRoot 'setup-result.json'
try {
    if(-not(Test-Path -LiteralPath $exe)){throw 'Full application is not installed.'}
    if(Get-Process OpenRGB,OpenRGB-Custom,OpenRGB-KFA2-test -ErrorAction SilentlyContinue){throw 'Close other OpenRGB instances before the device check.'}
    if(-not(Get-Process FanControl -ErrorAction SilentlyContinue)){throw 'Keep Fan Control running.'}
    $arguments = '--config "' + $settingsRoot + '" --list-devices'
    $scan = Start-Process -FilePath $exe -WorkingDirectory $appRoot -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
    if($scan.ExitCode -ne 0){throw ('Full device discovery exited with code ' + $scan.ExitCode)}
    $latestLog = Get-ChildItem -LiteralPath (Join-Path $settingsRoot 'logs') -Filter '*.log' | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if(-not $latestLog){throw 'Device discovery produced no log.'}
    $devices = @(Get-Content -LiteralPath $latestLog.FullName | Where-Object {$_ -match 'Registering RGB controller'} | ForEach-Object {($_ -split 'Registering RGB controller ',2)[1]})
    if($devices.Count -eq 0){throw 'No RGB controllers were detected.'}
    $profile = Join-Path $settingsRoot 'profiles\ValPurple.json'
    if(-not(Test-Path -LiteralPath $profile)){throw 'Saved purple profile is missing.'}
    $userId = [Security.Principal.WindowsIdentity]::GetCurrent().Name
    $action = New-ScheduledTaskAction -Execute "C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe" -WorkingDirectory $appRoot -Argument ('-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "' + (Join-Path $appRoot 'Start-OpenRGB-Custom.ps1') + '"')
    $trigger = New-ScheduledTaskTrigger -AtLogOn -User $userId
    $trigger.Delay = 'PT30S'
    $principal = New-ScheduledTaskPrincipal -UserId $userId -LogonType Interactive -RunLevel Highest
    $taskSettings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -ExecutionTimeLimit ([TimeSpan]::Zero) -MultipleInstances IgnoreNew
    Register-ScheduledTask -TaskName 'OpenRGB Custom (valerian)' -Action $action -Trigger $trigger -Principal $principal -Settings $taskSettings -Description 'Load the saved OpenRGB lighting profile after sign-in.' -Force | Out-Null
    $task = Get-ScheduledTask -TaskName 'OpenRGB Custom (valerian)'
    [PSCustomObject]@{success=$true;devices=$devices;log=$latestLog.FullName;task=$task.TaskName;state=[string]$task.State} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $resultFile -Encoding utf8
} catch {
    [PSCustomObject]@{success=$false;error=$_.Exception.Message} | ConvertTo-Json | Set-Content -LiteralPath $resultFile -Encoding utf8
    exit 1
}
