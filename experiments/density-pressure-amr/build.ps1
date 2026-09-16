param([int]$Jobs=3)
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$make='C:/msys64/usr/bin/make.exe'
if(-not (Test-Path -LiteralPath $make)){throw "Missing existing MSYS2 make: $make"}
$prefix='experiments/density-pressure-amr'
$oldPath=$env:PATH
Push-Location $repo
try {
    $env:PATH='C:/msys64/usr/bin;C:/msys64/ucrt64/bin;C:/Program Files/Microsoft MPI/Bin;'+$oldPath
    & $make -j $Jobs "$prefix/bin/AMR_Solver.exe" "SRCDIR=$prefix/src" "BINDIR=$prefix/bin" "OBJDIR=$prefix/build"
    if($LASTEXITCODE -ne 0){throw "Build failed: $LASTEXITCODE"}
} finally {Pop-Location; $env:PATH=$oldPath}
