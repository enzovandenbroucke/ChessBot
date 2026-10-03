param(
    [ValidateSet('Release', 'Debug')][string]$Configuration = 'Debug',
    [string]$Compiler = 'gcc',
    [switch]$Extended
)
$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'build.ps1') -Target tests -Configuration $Configuration -Compiler $Compiler
$program = Join-Path $PSScriptRoot "build/$Configuration/engine-tests.exe"
$testArgs = @()
if ($Extended) { $testArgs += '--extended' }
& $program @testArgs
if ($LASTEXITCODE -ne 0) { throw "Engine tests failed (exit $LASTEXITCODE)." }
