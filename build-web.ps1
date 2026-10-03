param(
    [string]$Compiler,
    [ValidateSet('Release', 'Debug')][string]$Configuration = 'Release'
)
$ErrorActionPreference = 'Stop'
if (!$Compiler) {
    $localCompiler = Join-Path $PSScriptRoot 'build/emsdk/upstream/emscripten/emcc.exe'
    if (Test-Path -LiteralPath $localCompiler) { $Compiler = $localCompiler }
    else { $Compiler = 'emcc' }
}
$compilerPath = (Get-Command $Compiler -ErrorAction Stop).Source
$outDir = Join-Path $PSScriptRoot 'build/web'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$flags = @('-std=gnu11', '-Wall', '-Wextra', '-DCHESSBOT_WEB', '-I', $PSScriptRoot)
if ($Configuration -eq 'Release') { $flags += '-O3' }
else { $flags += @('-O0', '-g', '-sASSERTIONS=2') }
$sources = @('web/bridge.c', 'ui/Game.c', 'Board.c', 'Position.c', 'Eval.c', 'Data.c', 'Zobrist.c') |
    ForEach-Object { Join-Path $PSScriptRoot $_ }
$flags += @('--no-entry', '-sMODULARIZE=1', '-sEXPORT_NAME=createChessBot',
    '-sENVIRONMENT=web,worker,node', '-sALLOW_MEMORY_GROWTH=1', '-sINITIAL_MEMORY=67108864',
    '-sMAXIMUM_MEMORY=268435456', '-sSTACK_SIZE=1048576', '-sFILESYSTEM=0',
    '-sEXPORTED_RUNTIME_METHODS=ccall,UTF8ToString')
Write-Host "Building ChessBot WebAssembly ($Configuration)"
$previousConfig = $env:EM_CONFIG
$previousPython = $env:EMSDK_PYTHON
$sdkConfig = Join-Path (Split-Path (Split-Path (Split-Path $compilerPath -Parent) -Parent) -Parent) '.emscripten'
try {
    if (Test-Path -LiteralPath $sdkConfig) {
        $env:EM_CONFIG = $sdkConfig
        $sdkPythonRoot = Join-Path (Split-Path $sdkConfig -Parent) 'python'
        if (Test-Path -LiteralPath $sdkPythonRoot) {
            $sdkPython = Get-ChildItem -LiteralPath $sdkPythonRoot -Directory |
                ForEach-Object { Join-Path $_.FullName 'python.exe' } |
                Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
            if ($sdkPython) { $env:EMSDK_PYTHON = $sdkPython }
        }
    }
    & $compilerPath @flags @sources -o (Join-Path $outDir 'engine.js') -lm
} finally { $env:EM_CONFIG = $previousConfig; $env:EMSDK_PYTHON = $previousPython }
if ($LASTEXITCODE -ne 0) { throw "WebAssembly compilation failed (exit $LASTEXITCODE)." }
Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'web') -File |
    Where-Object { $_.Extension -in @('.html', '.css', '.js') } |
    ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $outDir -Force }
$assets = Join-Path $outDir 'pieces'
New-Item -ItemType Directory -Force -Path $assets | Out-Null
Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'web/pieces') -File |
    ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $assets -Force }
Write-Host "Static website created in $outDir"
