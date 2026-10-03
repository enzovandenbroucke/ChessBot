param(
    [Parameter(Mandatory=$true)][int]$WaitProcessId,
    [Parameter(Mandatory=$true)][string]$PreviousResults,
    [Parameter(Mandatory=$true)][string]$Book,
    [Parameter(Mandatory=$true)][string]$NewResults,
    [Parameter(Mandatory=$true)][string]$Binaries,
    [Parameter(Mandatory=$true)][string]$Python,
    [Parameter(Mandatory=$true)][string]$StateFile
)
$ErrorActionPreference = 'Stop'
function Save-State([string]$Status, [string]$Detail = '') {
    @{ status = $Status; detail = $Detail; launcher_pid = $PID;
       previous_pid = $WaitProcessId; previous_results = $PreviousResults;
       results = $NewResults; updated_utc = [DateTime]::UtcNow.ToString('o') } |
        ConvertTo-Json | Set-Content -LiteralPath $StateFile -Encoding UTF8
}
try {
    if (Test-Path -LiteralPath $NewResults) { throw 'The new results directory already exists; refusing a duplicate run.' }
    Save-State 'waiting'
    $previous = Get-Process -Id $WaitProcessId -ErrorAction SilentlyContinue
    if ($previous) { $previous.WaitForExit() }
    # The original runner publishes a version only after all its comparisons finish.
    $oldMetadata = Get-Content -LiteralPath (Join-Path $PreviousResults 'metadata.json') -Raw | ConvertFrom-Json
    $oldRows = @(Get-Content -LiteralPath (Join-Path $PreviousResults 'summary.json') -Raw | ConvertFrom-Json)
    foreach ($version in $oldMetadata.parameters.versions) {
        $row = $oldRows | Where-Object { $_.version -eq $version }
        if (-not $row.fixed -or ($oldMetadata.parameters.adjusted -and -not $row.adjusted) -or
            ($version -gt 10 -and -not $oldMetadata.parameters.stockfish_only -and -not $row.previous)) {
            throw "Previous benchmark did not finish v$version; inspect it before restarting."
        }
    }
    Save-State 'running'
    & $Python (Join-Path $PSScriptRoot 'run.py') --mode strength --stockfish-only --adjusted `
        --book $Book --binaries $Binaries --out $NewResults --pairs 250 `
        --base-ms 3000 --increment-ms 50 --sf-elo 2700 --seed 20261002 `
        --priority-versions 17 16 --versions 17 16 15 14 13 12 11 10
    if ($LASTEXITCODE -ne 0) { throw "New benchmark failed (exit $LASTEXITCODE); inspect the saved logs." }
    Save-State 'completed'
} catch {
    Save-State 'failed' $_.Exception.Message
    throw
}
