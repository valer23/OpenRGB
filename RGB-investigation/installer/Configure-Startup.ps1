param([switch]$Remove)
$ErrorActionPreference = 'Stop'
$appRoot = $PSScriptRoot
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$user = $identity.Name
$shortName = $user.Split('\')[-1]
$taskName = 'OpenRGB Custom (' + $shortName + ')'
$launcher = Join-Path $appRoot 'Start-OpenRGB-Custom.ps1'
$existing = Get-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue
if($existing -and -not ($existing.Actions.Arguments -like ('*"' + $launcher + '"*'))){
    throw 'An existing startup task targets a different installation; it was preserved.'
}
if($Remove){
    if($existing){Unregister-ScheduledTask -TaskName $taskName -Confirm:$false}
    exit 0
}
$action = New-ScheduledTaskAction -Execute (Join-Path $env:WINDIR 'System32\WindowsPowerShell\v1.0\powershell.exe') -Argument ('-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "' + $launcher + '"') -WorkingDirectory $appRoot
$trigger = New-ScheduledTaskTrigger -AtLogOn -User $user
$trigger.Delay = 'PT30S'
$principal = New-ScheduledTaskPrincipal -UserId $identity.User.Value -LogonType Interactive -RunLevel Highest
$settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -ExecutionTimeLimit ([TimeSpan]::Zero) -MultipleInstances IgnoreNew
Register-ScheduledTask -TaskName $taskName -Action $action -Trigger $trigger -Principal $principal -Settings $settings -Description 'OpenRGB Custom lighting; restores ASUS LightingService on exit.' -Force | Out-Null
