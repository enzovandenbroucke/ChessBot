param([string]$Compiler = 'gcc', [string]$OutDir = '', [int[]]$Versions = @(10,11,12,13,14,15,16,17))
$ErrorActionPreference = 'Stop'
$compilerPath = (Get-Command $Compiler -ErrorAction Stop).Source
if (-not $OutDir) { $OutDir = Join-Path $PSScriptRoot 'build/benchmarks' }
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$flags = @('-std=gnu11', '-O3', '-Wall', '-Wextra', '-I', $PSScriptRoot)
$compilerVersion = & $compilerPath --version
@{ compiler = $compilerPath; version = $compilerVersion[0]; flags = $flags } |
    ConvertTo-Json | Set-Content -Encoding UTF8 (Join-Path $outDir 'build-info.json')
foreach ($version in $Versions) {
    if ($version -lt 10 -or $version -gt 17) { throw "Unsupported benchmark version: $version" }
    Write-Host "Building benchmark adapter v$version"
    $sources = @('benchmarks/adapter.c', 'Board.c', 'Position.c', 'Eval.c', 'Data.c', 'Zobrist.c') |
        ForEach-Object { Join-Path $PSScriptRoot $_ }
    & $compilerPath @flags "-DBENCH_VERSION=$version" @sources -o (Join-Path $outDir "v$version.exe") -lm
    if ($LASTEXITCODE -ne 0) { throw "Benchmark build failed for v$version." }
}
