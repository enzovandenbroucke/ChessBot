param(
    [string]$Compiler,
    [string]$Python,
    [ValidateRange(1024, 65535)][int]$Port = 8080,
    [switch]$SkipBuild
)
$ErrorActionPreference = 'Stop'
if (!$SkipBuild) {
    $options = @{}
    if ($Compiler) { $options.Compiler = $Compiler }
    & (Join-Path $PSScriptRoot 'build-web.ps1') @options
}
if (!$Python) {
    $sdkPythonRoot = Join-Path $PSScriptRoot 'build/emsdk/python'
    if (Test-Path -LiteralPath $sdkPythonRoot) {
        $Python = Get-ChildItem -LiteralPath $sdkPythonRoot -Directory |
            ForEach-Object { Join-Path $_.FullName 'python.exe' } |
            Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    }
    if (!$Python) { $Python = 'python' }
}
& $Python (Join-Path $PSScriptRoot 'helpers/serve_web.py') --port $Port
if ($LASTEXITCODE -ne 0) { throw "Local preview stopped with exit $LASTEXITCODE." }
