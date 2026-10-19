# Classify Handle output conservatively. Exit 1 also means a successful empty search.
function Get-BatteryHandleScanStatus {
    param([int]$ExitCode,[string[]]$Output)
    $lines=@($Output | ForEach-Object { ([string]$_).Replace([string][char]0,'').Trim() })
    if(($lines -join "`n") -match 'must accept EULA|Use -accepteula'){return 'license-required'}
    $hasOwners=@($lines | Where-Object {$_ -match '\bpid:\s*\d+\s+type:\s+File\b'}).Count -gt 0
    if($ExitCode -eq 0 -and $hasOwners){return 'completed'}
    $noMatches=@($lines | Where-Object {$_ -eq 'No matching handles found.'}).Count -eq 1
    $unexpected=@($lines | Where-Object {
        $_ -ne '' -and $_ -ne 'No matching handles found.' -and
        $_ -notmatch '^(?:Nthandle|Handle) v[0-9.]+ - Handle viewer$' -and
        $_ -notmatch '^Copyright \(C\) [0-9-]+ Mark Russinovich$' -and
        $_ -ne 'Sysinternals - www.sysinternals.com'
    })
    if($ExitCode -in @(0,1) -and $noMatches -and -not $hasOwners -and $unexpected.Count -eq 0){return 'completed'}
    return 'utility-failed'
}
