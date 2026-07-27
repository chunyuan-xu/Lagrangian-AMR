$ErrorActionPreference = "Stop"

Write-Host "==================================================" -ForegroundColor Cyan
Write-Host "  VALIDATION SCRIPT (CURRENT WORKING DIRECTORY)   " -ForegroundColor Cyan
Write-Host "==================================================" -ForegroundColor Cyan

# 1. Fast Parallel Compilation
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

# 2. Strict Regression Testing
Write-Host "=> Executing regression validation suite..." -ForegroundColor Yellow
python run_tests.py
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Regression validation failed! Current code breaks the baseline!" -ForegroundColor Red
    exit 1
}

Write-Host "==================================================" -ForegroundColor Cyan
Write-Host " VALIDATION PASSED: NEW CODE IS PURE AND SECURE" -ForegroundColor Green
Write-Host "==================================================" -ForegroundColor Cyan
exit 0
