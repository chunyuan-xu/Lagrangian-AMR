$ErrorActionPreference = "Stop"

Write-Host "==================================================" -ForegroundColor Cyan
Write-Host " ULTRA-FAST ROLLBACK & VALIDATION SCRIPT " -ForegroundColor Cyan
Write-Host "==================================================" -ForegroundColor Cyan

# 1. Find the latest [GOLDEN-PASS] commit
Write-Host "=> Locating latest [GOLDEN-PASS] baseline..." -ForegroundColor Yellow
$hash = git log --grep="\[GOLDEN-PASS\]" -n 1 --format="%H"

if ([string]::IsNullOrWhiteSpace($hash)) {
    Write-Host "ERROR: No commit with [GOLDEN-PASS] tag found in Git history." -ForegroundColor Red
    exit 1
}

Write-Host "=> Found golden rollback point: $hash" -ForegroundColor Green

# 2. Hard Reset and Clean
Write-Host "=> Restoring source code and reference data..." -ForegroundColor Yellow
git reset --hard $hash
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host "=> Wiping all untracked garbage files..." -ForegroundColor Yellow
git clean -fd
if ($LASTEXITCODE -ne 0) { exit 1 }

# 3. Fast Parallel Compilation
Write-Host "=> Cleaning and compiling in MSYS2 environment..." -ForegroundColor Yellow
$OldPath = $env:PATH
$env:PATH = "C:\msys64\usr\bin;C:\msys64\ucrt64\bin;" + $env:PATH
make clean
make -j8
$makeExit = $LASTEXITCODE
$env:PATH = $OldPath

if ($makeExit -ne 0) {
    Write-Host "ERROR: Compilation failed!" -ForegroundColor Red
    exit 1
}

# 4. Strict Regression Testing
Write-Host "=> Executing regression validation suite..." -ForegroundColor Yellow
python python/run_tests.py
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Regression validation failed! Baseline is broken!" -ForegroundColor Red
    exit 1
}

Write-Host "==================================================" -ForegroundColor Cyan
Write-Host " VALIDATION PASSED: SYSTEM IS 100% PURE AND SECURE" -ForegroundColor Green
Write-Host "==================================================" -ForegroundColor Cyan
exit 0
