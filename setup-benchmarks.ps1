param([string]$Python = 'python')
$ErrorActionPreference = 'Stop'
$pythonPath = (Get-Command $Python -ErrorAction Stop).Source
$environment = Join-Path $PSScriptRoot 'build/metrics-venv'
& $pythonPath -m venv $environment
if ($LASTEXITCODE -ne 0) { throw 'Could not create the benchmark Python environment.' }
$venvPython = Join-Path $environment 'Scripts/python.exe'
if (-not (Test-Path $venvPython)) { $venvPython = Join-Path $environment 'bin/python.exe' }
& $venvPython -m pip install -r (Join-Path $PSScriptRoot 'benchmarks/requirements.txt')
if ($LASTEXITCODE -ne 0) { throw 'Benchmark dependency installation failed.' }
Write-Host "Benchmark Python: $venvPython"
