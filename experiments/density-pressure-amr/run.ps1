param(
    [Parameter(Mandatory=$true)][ValidateSet('noh','sedov')][string]$Case,
    [string]$RunName='',
    [string]$MpiExec='C:/Program Files/Microsoft MPI/Bin/mpiexec.exe'
)
$ErrorActionPreference='Stop'
if(-not $RunName){$RunName="$Case-$(Get-Date -Format yyyyMMdd-HHmmss)"}
if($RunName -notmatch '^[A-Za-z0-9][A-Za-z0-9_-]*$'){throw 'RunName must be a plain directory name'}
$exe=Join-Path $PSScriptRoot 'bin/AMR_Solver.exe'
if(-not (Test-Path -LiteralPath $exe)){throw 'Build this package first with build.ps1'}
if(-not (Test-Path -LiteralPath $MpiExec)){throw "Missing MPI launcher: $MpiExec"}
$out=Join-Path $PSScriptRoot "runs/$RunName"
if(Test-Path -LiteralPath $out){throw "Refuse to overwrite existing result: $out"}
$presets=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'configs/presets.json') -Raw | ConvertFrom-Json
$preset=$presets.cases.$Case
$expected=(Get-Content -LiteralPath (Join-Path $PSScriptRoot 'expected-results.json') -Raw | ConvertFrom-Json).cases.$Case
$setting=@{}
foreach($entry in $presets.common_environment.PSObject.Properties){$setting[$entry.Name]=[string]$entry.Value}
foreach($entry in $preset.environment.PSObject.Properties){$setting[$entry.Name]=[string]$entry.Value}
# Presence-controlled switches must be absent, not merely set to zero.
$saved=@{}
Get-ChildItem Env: | Where-Object {$_.Name -like 'AMR_*' -or $_.Name -eq 'LAGRANGIAN_CHECK_STATE_INVARIANTS'} | ForEach-Object {$saved[$_.Name]=$_.Value}
$oldPath=$env:PATH
New-Item -ItemType Directory -Path (Join-Path $out 'output') | Out-Null
Copy-Item -LiteralPath (Join-Path $PSScriptRoot $preset.config) -Destination (Join-Path $out 'param.ini')
$setting | ConvertTo-Json | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $out 'environment.json')
$exeHash=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant()
try {
    Get-ChildItem Env:AMR_* | ForEach-Object {Remove-Item -LiteralPath ('Env:'+$_.Name)}
    foreach($key in $setting.Keys){[Environment]::SetEnvironmentVariable($key,$setting[$key],'Process')}
    $env:PATH='C:/msys64/ucrt64/bin;C:/Program Files/Microsoft MPI/Bin;'+$oldPath
    Push-Location $out
    try {
        & $MpiExec -n 1 $exe 1> solver.log 2> solver.err
        $solverExit=$LASTEXITCODE
    } finally {Pop-Location}
} finally {
    Get-ChildItem Env: | Where-Object {$_.Name -like 'AMR_*' -or $_.Name -eq 'LAGRANGIAN_CHECK_STATE_INVARIANTS'} | ForEach-Object {Remove-Item -LiteralPath ('Env:'+$_.Name)}
    foreach($key in $saved.Keys){[Environment]::SetEnvironmentVariable($key,$saved[$key],'Process')}
    $env:PATH=$oldPath
}
$result=[ordered]@{case=$Case;exit=$solverExit;executable_sha256=$exeHash;baseline=$preset.baseline;verified=$false}
$result | ConvertTo-Json | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $out 'verification.json')
if($solverExit -ne 0){throw "Solver failed: $solverExit; preserved logs in $out"}
$log=[IO.File]::ReadAllText((Join-Path $out 'solver.log'))
if([IO.File]::ReadAllText((Join-Path $out 'solver.err')).Trim()){throw "Nonempty stderr: $out"}
if(-not $log.Contains('[invariant-checker] enabled')){throw 'State invariant checker was not enabled'}
if($log -match '\bnan\b|STATE INVARIANT|job aborted|invalid least-squares'){throw 'Invalid-state marker in solver log'}
$steps=[regex]::Matches($log,'simulation_step=\s*(\d+), delta_time = [^,]+, simulation_time = ([^\s]+)')
if($steps.Count -ne [int]$expected.steps){throw 'Unexpected accepted-step count'}
$last=$steps[$steps.Count-1]
if([int]$last.Groups[1].Value -ne [int]$expected.steps){throw 'Unexpected final step'}
$culture=[Globalization.CultureInfo]::InvariantCulture
$time=[double]::Parse($last.Groups[2].Value,$culture)
if([Math]::Abs($time-[double]$expected.last_logged_time) -gt 1e-6){throw 'Unexpected final time'}
$maxEnergy=0.0
$energies=[regex]::Matches($log,'the total energy error is ([^\s]+)')
if(-not $energies.Count){throw 'Missing energy checks'}
foreach($m in $energies){$e=[double]::Parse($m.Groups[1].Value,$culture);if([double]::IsNaN($e) -or [double]::IsInfinity($e)){throw 'Invalid energy error'};$maxEnergy=[Math]::Max($maxEnergy,[Math]::Abs($e))}
if($maxEnergy -gt 1e-10){throw 'Energy check failed'}
$profile=Join-Path $out 'DistanceProfiles.plt'
$profileHash=(Get-FileHash -LiteralPath $profile -Algorithm SHA256).Hash.ToLowerInvariant()
if($profileHash -ne $expected.profile_sha256){throw 'Final profile differs from the historical reference; preserve and investigate, do not update reference automatically'}
$result.verified=$true
$result['steps']=[int]$expected.steps
$result['last_logged_time']=$time
$result['profile_sha256']=$profileHash
$result['final_cells']=[int]$expected.final_cells
$result['raw_density_max']=[double]$expected.raw_density_max
$result['max_logged_energy_error']=$maxEnergy
$result['note']='Full-run exit, state/energy logs and exact historical final profile verified. Not an independent all-frame accuracy or geometry audit.'
$result | ConvertTo-Json | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $out 'verification.json')
Write-Output "Verified $Case; result: $out"
