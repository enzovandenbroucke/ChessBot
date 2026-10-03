# Version benchmarks

I compare v10–v17a using paired games and fixed-budget searches. All versions
share the current board and evaluation modules, so these measurements compare
the included implementations rather than reconstructing their original ratings.
v10 has no measured predecessor here because v9 isn't included.

## Run the benchmarks

With Python and MinGW-w64 GCC installed, run from the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\setup-benchmarks.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\metrics.ps1 -Mode performance
```

Setup installs `chess==1.11.2` in `build/metrics-venv/` and only needs to run once.
The launcher builds one native executable per version with `-O3`, without
`-march=native`. Pass `-Compiler 'C:\path\to\gcc.exe'` if needed, and keep the
compiler's runtime DLLs on PATH.

For matches, put `stockfish.exe` in the repository root or pass `-Stockfish`:

```powershell
# Performance and 500 games per comparison, with adjusted Stockfish as well.
powershell -NoProfile -ExecutionPolicy Bypass -File .\metrics.ps1 -Mode all -Pairs 250 -Adjusted

# A smaller experiment for v17 only, using existing compiled adapters.
powershell -NoProfile -ExecutionPolicy Bypass -File .\metrics.ps1 -Mode strength -Versions 17 -Pairs 50 -Adjusted -SkipBuild

# Only Stockfish matches, with my external opening book and newest versions first.
powershell -NoProfile -ExecutionPolicy Bypass -Command "& .\metrics.ps1 -Mode strength -StockfishOnly -Adjusted -Book 'C:\path\to\komodo.bin' -PriorityVersions 17,16 -Versions 17,16,15,14,13,12,11,10"
```

`-SkipBuild` reuses the binaries, so use it only when they match the sources you
want to measure. Full multi-version runs take a long time. The settings are
configurable through `metrics.ps1` and `benchmarks/run.py --help`.

## Indicators

| Metric | Definition |
| --- | --- |
| W/D/L and score | Wins/draws/losses; score = `(wins + draws / 2) / games` |
| ΔElo | `400 log10(score / (1 − score))` against the opponent |
| Stockfish fixed | Same configured Stockfish Elo for every version |
| Stockfish adjusted | Tune its Elo on pilot games, freeze it, then measure fresh games |
| Calibrated Elo | Configured Stockfish Elo + measured ΔElo |
| Nodes/s | Total counted nodes divided by total measured search time |
| Mean/median depth | Last fully completed iteration at a fixed time budget |
| Depth/s | Mean of completed depth divided by actual search time |

I interpret depth at a fixed budget alongside throughput: depth grows nonlinearly
and depends on pruning, while a higher nodes/s count doesn't necessarily mean
stronger play. The Stockfish-based Elo is specific to the calibration protocol,
not a tournament rating. At 0% or 100%, no finite ΔElo can be estimated.

## Protocol

Performance uses eight mixed positions, three repetitions and **1000 ms per
search**. Warm-up searches are excluded, and TT, pawn cache, PV and killers are
cleared between measurements. Nodes include quiescence and interrupted-iteration
work counted by the existing engine counter. The timer measures search duration
at high resolution. v17's soft time-management stop is disabled only for these
fixed-budget measurements.

For matches I use **3 seconds + 50 ms per move**, serial games,
and Stockfish at **2700 Elo with one thread and 32 MiB hash**. ChessBot's search
TT is also 32 MiB; its shared allocator additionally reserves an unused 128 MiB
global TT. Openings come from `test_pos_dataset_opening.txt`, shuffled with a
recorded seed. Each position is played with both color assignments.

I enable my external Polyglot book with `-Book`; the book is not distributed in
this repository. Without this option, matches use normal search throughout.
Performance searches always exclude the book. A requested book must load
successfully before play starts. Results record its hash and the number of
actual book moves. Each color pair uses the same recorded book-selection seed;
the engine stops consulting the book after its first miss, until the next game.

`-Mode strength -StockfishOnly` skips performance measurements and matches
against previous versions. Version order follows `-Versions`. With
`-PriorityVersions 17,16`, both fixed-Stockfish results are saved first, then
their adjusted-Stockfish results, before moving to the remaining versions.

v17 uses its dynamic time management. For older versions, the adapter allocates
`remaining / 25 + increment / 2`, capped at 75% of remaining time. ChessBot clocks
charge native search duration; Stockfish clocks include the UCI round trip.
This difference matters near a time forfeit. The opening seed doesn't control
Stockfish's internal randomness.

I use python-chess as an independent referee, automatically claiming available
threefold/fifty-move draws. It also checks legal moves, mate, stalemate and
insufficient material. Games reaching the 512-ply cap remain unfinished (`*`);
their whole color pair is excluded from the rating summary. Time losses, illegal
moves and search-process failures are recorded separately in the results.

The approximate 95% interval resamples complete color pairs with a percentile
bootstrap. When all pair scores coincide, a conservative Hoeffding interval
avoids reporting zero uncertainty. Sample size and opening diversity still matter.

Adjusted Stockfish uses three pilot rounds of ten pairs by default, seeking a
score near 50%. Each adjustment is capped at 150 Elo and Stockfish's supported
range. Pilot openings are separate from the final sample, and pilot scores never
enter the final estimate. This avoids some extreme scores but doesn't guarantee
lower variance.

## Result files

Each run creates a new directory under `build/metrics/`:

| Files | Contents |
| --- | --- |
| `summary.md`, `.csv`, `.json` | Table and numeric summaries |
| `performance.jsonl` | Position, move, depth, nodes and time for every search |
| `games.jsonl`, `game-searches.jsonl`, `games.pgn` | Outcomes, search telemetry and replayable games |
| `metadata.json` | Settings, selected positions, compiler/system information, book/file hashes and completion status |

I keep raw results with the table so a score can be traced back to its games.
Local outputs are ignored by Git; selected results can be copied into `results/`
for publication. CPU model, power settings and competing workloads should be
recorded alongside the automatic metadata.

## Tooling tests

Statistics tests need only Python:

```powershell
python .\tests\benchmark_tests.py
```

For native adapter integration, use the benchmark environment after building:

```powershell
$env:CHESSBOT_BENCH_INTEGRATION = '1'
.\build\metrics-venv\Scripts\python.exe .\tests\benchmark_tests.py
Remove-Item Env:CHESSBOT_BENCH_INTEGRATION
```

MSYS Python uses `bin/python.exe` instead of `Scripts/python.exe`. These tests
cover statistics, color pairing, forfeits, unfinished games and adapter telemetry.
Engine coverage is described in [TESTING.md](TESTING.md).
