param(
    [ValidateSet('performance', 'strength', 'all')][string]$Mode = 'performance',
    [int[]]$Versions = @(10,11,12,13,14,15,16,17),
    [int[]]$PriorityVersions = @(),
    [int]$BudgetMs = 1000,
    [int]$Pairs = 250,
    [int]$BaseMs = 3000,
    [int]$IncrementMs = 50,
    [int]$StockfishElo = 2700,
    [string]$Stockfish = '',
    [string]$Openings = '',
    [string]$OutDir = '',
    [string]$Binaries = '',
    [string]$Book = '',
    [string]$Compiler = 'gcc',
    [string]$Python = '',
    [switch]$Adjusted,
    [switch]$StockfishOnly,
    [switch]$SkipBuild
)
$ErrorActionPreference = 'Stop'
if (-not $SkipBuild) { & (Join-Path $PSScriptRoot 'build-benchmarks.ps1') -Compiler $Compiler -OutDir $Binaries }
if (-not $Python) {
    $Python = Join-Path $PSScriptRoot 'build/metrics-venv/Scripts/python.exe'
    if (-not (Test-Path $Python)) { $Python = Join-Path $PSScriptRoot 'build/metrics-venv/bin/python.exe' }
}
if (-not (Test-Path $Python)) { throw 'Run setup-benchmarks.ps1 first, or pass -Python.' }
$arguments = @((Join-Path $PSScriptRoot 'benchmarks/run.py'), '--mode', $Mode,
               '--budget-ms', $BudgetMs, '--pairs', $Pairs, '--base-ms', $BaseMs,
               '--increment-ms', $IncrementMs, '--sf-elo', $StockfishElo)
if ($Adjusted) { $arguments += '--adjusted' }
if ($StockfishOnly) { $arguments += '--stockfish-only' }
if ($Book) { $arguments += @('--book', $Book) }
if ($Binaries) { $arguments += @('--binaries', $Binaries) }
if ($OutDir) { $arguments += @('--out', $OutDir) }
if ($Stockfish) { $arguments += @('--stockfish', $Stockfish) }
if ($Openings) { $arguments += @('--openings', $Openings) }
if ($PriorityVersions.Count) { $arguments += '--priority-versions'; $arguments += $PriorityVersions }
$arguments += '--versions'
$arguments += $Versions
& $Python @arguments
if ($LASTEXITCODE -ne 0) { throw 'Version measurements failed; inspect the saved raw records.' }
