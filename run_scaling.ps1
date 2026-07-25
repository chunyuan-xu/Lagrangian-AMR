$env:PATH="C:\msys64\usr\bin;C:\msys64\ucrt64\bin;" + $env:PATH
$results = @()
foreach ($n in 1,2,4,8) {
    $time = (Measure-Command { & "C:\Program Files\Microsoft MPI\Bin\mpiexec.exe" -n $n ./bin/AMR_Solver.exe }).TotalSeconds
    $results += "$n,$time"
    Write-Host "Core $n : $time"
}
$results | Out-File -FilePath scaling_results.txt
