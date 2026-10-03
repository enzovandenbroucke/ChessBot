param([string]$Python = 'python')
$ErrorActionPreference = 'Stop'
$sdkRoot = Join-Path $PSScriptRoot 'build/emsdk'
if (!(Test-Path -LiteralPath (Join-Path $sdkRoot 'emsdk.py'))) {
    New-Item -ItemType Directory -Force -Path (Split-Path $sdkRoot -Parent) | Out-Null
    git clone --depth 1 https://github.com/emscripten-core/emsdk.git $sdkRoot
    if ($LASTEXITCODE -ne 0) { throw 'Could not download the official Emscripten SDK.' }
}
Push-Location $sdkRoot
try {
    & $Python ./emsdk.py install 6.0.10
    if ($LASTEXITCODE -ne 0) { throw 'Emscripten SDK installation failed.' }
    & $Python ./emsdk.py activate 6.0.10
    if ($LASTEXITCODE -ne 0) { throw 'Emscripten SDK activation failed.' }
} finally { Pop-Location }
Write-Host 'Web toolchain ready. Run web.ps1 to build and preview ChessBot.'
