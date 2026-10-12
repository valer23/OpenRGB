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
$restoreLighting = $false
try {
    ('Starting at ' + (Get-Date -Format o)) | Set-Content -LiteralPath $trace
    if(Get-Process OpenRGB-Custom -ErrorAction SilentlyContinue){Add-Content -LiteralPath $trace 'Already running';exit 0}
    $profile = Get-Content -LiteralPath (Join-Path $settingsRoot 'profiles\ValWhite.json') -Raw | ConvertFrom-Json
    if($profile.controllers.name -contains 'ASUS ROG STRIX B850-F GAMING WIFI7 NEO (logo)') {
        $service = Get-Service LightingService -ErrorAction SilentlyContinue
        if($service -and $service.Status -eq 'Running') {
            Stop-Service LightingService -ErrorAction Stop
            $restoreLighting = $true
            Add-Content -LiteralPath $trace 'ASUS LightingService paused while OpenRGB controls the logo.'
        }
    }
    $process = Start-Process -FilePath (Join-Path $appRoot 'OpenRGB-Custom.exe') -WorkingDirectory $appRoot -ArgumentList ('--config "' + $settingsRoot + '" ' + $windowArgument + ' --profile ValWhite') -RedirectStandardOutput (Join-Path $settingsRoot 'startup-stdout.txt') -RedirectStandardError (Join-Path $settingsRoot 'startup-stderr.txt') -PassThru
    Add-Content -LiteralPath $trace ('Process: ' + $process.Id)
    $process.WaitForExit()
    $process.Refresh()
    Add-Content -LiteralPath $trace ('Exit: ' + $process.ExitCode)
    exit $process.ExitCode
} catch {Add-Content -LiteralPath $trace $_.Exception.Message;exit 1} finally {
    if($restoreLighting) {
        Start-Service LightingService
        Add-Content -LiteralPath $trace 'ASUS LightingService restored.'
    }
}
