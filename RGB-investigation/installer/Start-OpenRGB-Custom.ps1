param([switch]$ShowWindow)
$principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if(-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    $arguments = '-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "' + $PSCommandPath + '"'
    if($ShowWindow){$arguments += ' -ShowWindow'}
    Start-Process -FilePath (Join-Path $env:WINDIR 'System32\WindowsPowerShell\v1.0\powershell.exe') -ArgumentList $arguments -Verb RunAs -WindowStyle Hidden
    exit
}
$windowArgument = if($ShowWindow){'--gui'}else{'--startminimized'}
$ErrorActionPreference = 'Stop'
$appRoot = $PSScriptRoot
$settingsRoot = Join-Path $appRoot 'Settings'
$trace = Join-Path $appRoot 'startup-trace.txt'
try {
    ('Starting at ' + (Get-Date -Format o)) | Set-Content -LiteralPath $trace
    if(Get-Process OpenRGB-Custom -ErrorAction SilentlyContinue){Add-Content -LiteralPath $trace 'Already running';exit 0}
    $process = Start-Process -FilePath (Join-Path $appRoot 'OpenRGB-Custom.exe') -WorkingDirectory $appRoot -ArgumentList ('--config "' + $settingsRoot + '" ' + $windowArgument + ' --noautoconnect') -RedirectStandardOutput (Join-Path $settingsRoot 'startup-stdout.txt') -RedirectStandardError (Join-Path $settingsRoot 'startup-stderr.txt') -PassThru
    Add-Content -LiteralPath $trace ('Process: ' + $process.Id)
    $process.WaitForExit()
    $process.Refresh()
    Add-Content -LiteralPath $trace ('Exit: ' + $process.ExitCode)
    exit $process.ExitCode
} catch {Add-Content -LiteralPath $trace $_.Exception.Message;exit 1}
