param([switch]$Debug)

$gxx = (Get-Command g++ -ErrorAction SilentlyContinue)
if (-not $gxx) {
    $candidates = Get-ChildItem "$env:LOCALAPPDATA\Microsoft\WinGet\Packages" -Recurse -Filter "g++.exe" -ErrorAction SilentlyContinue
    if ($candidates) { $gxx = $candidates[0].FullName } else { Write-Error "g++ not found"; exit 1 }
} else { $gxx = $gxx.Source }

$flags = @("-std=c++17", "-O3", "-march=native", "-flto", "-static", "-DNDEBUG")
if ($Debug) { $flags = @("-std=c++17", "-O1", "-g", "-fsanitize=undefined") }

Write-Host "Using $gxx"
& $gxx @flags src/main.cpp -o singularity.exe
if ($LASTEXITCODE -eq 0) { Write-Host "Build OK -> singularity.exe" } else { exit 1 }
