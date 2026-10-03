param(
    [ValidateSet('all', 'compare', 'gen-header', 'tests')][string]$Target = 'all',
    [ValidateSet('Release', 'Debug')][string]$Configuration = 'Release',
    [string]$Compiler = 'gcc'
)
$ErrorActionPreference = 'Stop'
$compilerCommand = Get-Command $Compiler -ErrorAction Stop
$compilerPath = $compilerCommand.Source
$outDir = Join-Path $PSScriptRoot "build/$Configuration"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$flags = @('-std=gnu11', '-Wall', '-Wextra', '-I', $PSScriptRoot)
if ($Configuration -eq 'Debug') { $flags += @('-O0', '-g') }
else { $flags += '-O3' }

function Build-Program([string]$Name, [string[]]$Sources, [string[]]$Libraries) {
    $sourcePaths = @($Sources | ForEach-Object { Join-Path $PSScriptRoot $_ })
    $outputPath = Join-Path $outDir "$Name.exe"
    Write-Host "Building $Name ($Configuration)"
    & $compilerPath @flags @sourcePaths -o $outputPath @Libraries
    if ($LASTEXITCODE -ne 0) { throw "Compilation failed for $Name (exit $LASTEXITCODE)." }
    Write-Host "Created $outputPath"
}
if ($Target -in @('all', 'tests')) {
    Build-Program 'engine-tests' @('tests/engine_tests.c', 'Board.c', 'Position.c', 'Eval.c', 'Data.c', 'Zobrist.c') @('-lm')
}

if ($Target -in @('all', 'compare')) {
    Build-Program 'compare' @('CompareThread.c', 'Board.c', 'Position.c', 'Eval.c', 'Data.c', 'Zobrist.c') @('-pthread', '-lm')
}
if ($Target -in @('all', 'gen-header')) {
    Build-Program 'gen-header' @('helpers/gen_header.c') @()
}
