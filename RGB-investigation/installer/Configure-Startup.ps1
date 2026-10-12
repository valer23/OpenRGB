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
# The application's own Startup shortcut runs without elevation and can beat
# this task to logon. Keep the task as this installation's startup entry.
$configPath = Join-Path $appRoot 'Settings\OpenRGB.json'
if(Test-Path -LiteralPath $configPath){
    $raw = [IO.File]::ReadAllText($configPath)
    $pattern = '(?s)("AutoStart"\s*:\s*\{[^}]*?"enabled"\s*:\s*)true'
    $updated = [regex]::Replace($raw, $pattern, '${1}false')
    if($updated -ne $raw){
        Copy-Item -LiteralPath $configPath -Destination ($configPath + '.before-startup-repair.bak') -Force
        [IO.File]::WriteAllText($configPath, $updated, [Text.UTF8Encoding]::new($false))
    }
}
$startup = [Environment]::GetFolderPath('Startup')
$shell = New-Object -ComObject WScript.Shell
foreach($shortcutPath in @((Join-Path $startup 'OpenRGB.lnk'),(Join-Path $startup 'OpenRGB-Custom.lnk'))){
    if(Test-Path -LiteralPath $shortcutPath){
        $shortcut = $shell.CreateShortcut($shortcutPath)
        if($shortcut.TargetPath -eq (Join-Path $appRoot 'OpenRGB-Custom.exe')){
            Move-Item -LiteralPath $shortcutPath -Destination ($shortcutPath + '.disabled') -Force
        }
    }
}
$action = New-ScheduledTaskAction -Execute (Join-Path $env:WINDIR 'System32\WindowsPowerShell\v1.0\powershell.exe') -Argument ('-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "' + $launcher + '"') -WorkingDirectory $appRoot
$trigger = New-ScheduledTaskTrigger -AtLogOn -User $user
$trigger.Delay = 'PT30S'
$principal = New-ScheduledTaskPrincipal -UserId $identity.User.Value -LogonType Interactive -RunLevel Highest
$settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -ExecutionTimeLimit ([TimeSpan]::Zero) -MultipleInstances IgnoreNew
Register-ScheduledTask -TaskName $taskName -Action $action -Trigger $trigger -Principal $principal -Settings $settings -Description 'OpenRGB Custom lighting; restores ASUS LightingService on exit.' -Force | Out-Null
