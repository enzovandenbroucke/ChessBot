# ChessBot

A chess engine I built in C to learn about game-tree search, data representation
and performance. The current version, **v17**, can be played in the browser through
WebAssembly.

## Why I built it

I started ChessBot in Java after watching Sebastian Lague's chess programming
videos. I wanted to practice object-oriented programming on a project I could
visualize and improve through experiments: make two versions play against each
other and compare their results.

Moving to C gave me a reason to learn more about memory locality, fixed arrays,
allocation costs and the work done inside the search loop. Move generation was
one of the hardest parts; perft and divide tests helped me track down mistakes.
I also learned that a search optimization recommended in the literature does not
necessarily improve my engine, so I kept earlier versions for comparison.

## What's inside

- Magic bitboards for sliding attacks, using magic numbers I searched myself.
- Iterative deepening, alpha-beta/PVS and quiescence search.
- Zobrist hashing and transposition tables.
- Move ordering with SEE, killer moves, history and countermoves.
- Selective search: null-move pruning, late-move reductions, futility pruning
  and singular extensions.
- Classical evaluation based on PeSTO tables, with pawn structure, bishop pair,
  rook activity and king safety terms.
- Dynamic time management and optional Polyglot opening books.

The browser interface supports play, analysis, clocks, undo/redo and FEN loading.
The C engine runs locally in the browser; there is no server-side engine.

## Try it yourself

On Windows, install Git and Python, then run these commands from the repository
root. Setup installs the Emscripten toolchain locally; it only needs to run once.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\setup-web.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\web.ps1
```

Open <http://127.0.0.1:8080/>. See the [browser guide](docs/WEB.md) for controls
and build options.

## Build and test

For the native programs, I use Windows x64 with MinGW-w64 GCC and PowerShell.
Put the compiler's `bin` directory on PATH, then run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Configuration Release -Extended
```

Release builds use `-O3` without `-march=native`. The tests cover move encoding,
FEN, special moves, state restoration, hashes, evaluation and reference perft
positions. [Build details](docs/BUILD.md) and [test coverage](docs/TESTING.md)
are documented separately.

## Results and development

I benchmarked v10–v17 under a common protocol on October 2–3, 2026. Matches use
**3 seconds + 50 ms per move**, reversed colors and **no opening book**.
Against Stockfish 18 set to 2700, **v17 scored 48.7%**, giving an estimated
**2691 Elo** (95% interval: 2668–2715). It scored **63.3% against v16**, a
relative gain of **+95 Elo** (68–122).

| Version | SF score | Estimated Elo | ΔElo vs previous | Nodes/s (millions) | Mean depth |
| --- | ---: | ---: | ---: | ---: | ---: |
| v10 | 26.8% | 2525 | — | 3.41 | 12.88 |
| v11 | 33.9% | 2584 | +161 | 4.07 | 14.04 |
| v12 | 37.9% | 2614 | +8 | 4.41 | 14.62 |
| v13 | 33.4% | 2580 | +10 | 4.09 | 15.75 |
| v14 | 32.2% | 2571 | +61 | 3.29 | 16.08 |
| v15 | 39.2% | 2624 | +58 | 3.00 | 17.96 |
| v16 | 40.8% | 2635 | +50 | 2.94 | 20.29 |
| v17 | 48.7% | 2691 | +95 | 2.75 | 19.75 |

SF scores use the same 2700 setting. Performance uses 24 searches per version
at 1000 ms each. These Elo estimates describe the test protocol, not tournament
ratings; small gains such as v12 and v13 remain uncertain. The
[full results](results/2026-10-02/README.md) include W/D/L, confidence intervals,
adjusted-SF estimates, median depth, depth/s and the underlying data. The
[benchmark guide](docs/METRICS.md) explains the protocol, and the
[version history](docs/VERSIONS.md) keeps my earlier measurements separately.

I'd like to explore NNUE evaluation next and add a standalone UCI interface.
The NNUE experiment is not included here.

## Credits

I learned from Sebastian Lague's videos and the Chess Programming Wiki. PeSTO
tables are by Ronald Friederich; the piece artwork is by Cburnett. Sources and
third-party notices are in [ACKNOWLEDGMENTS.md](ACKNOWLEDGMENTS.md).

[Enzo Vandenbroucke](https://github.com/enzovandenbroucke)
