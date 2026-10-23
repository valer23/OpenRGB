$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\..\scripts\BatteryHandleResult.ps1')
$empty=@('','Nthandle v5.0 - Handle viewer','Copyright (C) 1997-2022 Mark Russinovich','Sysinternals - www.sysinternals.com','','No matching handles found.')
$cases=@(
    @{name='empty search exit1';code=1;lines=$empty;expected='completed'},
    @{name='empty search exit0';code=0;lines=$empty;expected='completed'},
    @{name='found owner';code=0;lines=@('LightingService.exe pid: 16984  type: File  A3C: \Device\0000008b');expected='completed'},
    @{name='license refusal';code=1;lines=@('Use -accepteula to accept EULA.');expected='license-required'},
    @{name='empty output';code=0;lines=@();expected='utility-failed'},
    @{name='unexpected exit';code=2;lines=$empty;expected='utility-failed'},
    @{name='partial failed search';code=1;lines=($empty+@('Error opening process'));expected='utility-failed'},
    @{name='conflicting output';code=1;lines=($empty+@('asus_framework.exe pid: 1  type: File  A3C: \Device\0000008b'));expected='utility-failed'},
    @{name='nul encoded license';code=0;lines=@(('Use -accepteula to accept EULA.'.ToCharArray() -join [char]0));expected='license-required'}
)
foreach($case in $cases){
    $actual=Get-BatteryHandleScanStatus -ExitCode $case.code -Output $case.lines
    if($actual -ne $case.expected){throw "$($case.name): expected $($case.expected), got $actual"}
}
Write-Host "Handle result classification: $($cases.Count) cases passed."
