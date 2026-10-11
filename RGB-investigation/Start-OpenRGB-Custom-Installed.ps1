$ErrorActionPreference = 'Stop'
$appRoot = 'C:\Users\valerian\Apps\OpenRGB-Custom'
$settingsRoot = 'C:\Users\valerian\Apps\OpenRGB-Custom\Settings'
$trace = Join-Path $appRoot 'startup-trace.txt'
try {
    ('Starting at ' + (Get-Date -Format o)) | Set-Content -LiteralPath $trace
    if(Get-Process OpenRGB-Custom -ErrorAction SilentlyContinue){Add-Content -LiteralPath $trace 'Already running';exit 0}
    $process = Start-Process -FilePath (Join-Path $appRoot 'OpenRGB-Custom.exe') -WorkingDirectory $appRoot -ArgumentList ('--config "' + $settingsRoot + '" --startminimized --profile ValWhite') -RedirectStandardOutput (Join-Path $settingsRoot 'startup-stdout.txt') -RedirectStandardError (Join-Path $settingsRoot 'startup-stderr.txt') -PassThru
    Add-Content -LiteralPath $trace ('Process: ' + $process.Id)
    $process.WaitForExit()
    $process.Refresh()
    Add-Content -LiteralPath $trace ('Exit: ' + $process.ExitCode)
    exit $process.ExitCode
} catch {Add-Content -LiteralPath $trace $_.Exception.Message;exit 1}
