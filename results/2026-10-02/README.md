# Version benchmarks: October 2026

I measured v10–v17 on October 2–3, 2026, using the same protocol for all
eight versions. These results are from my first complete benchmark run
(`20261002-020212`), **without an opening book**.

**v17 scored 48.7% against Stockfish 18 set to 2700 Elo**, giving an estimated
2691 Elo (95% interval: 2668–2715). Against v16 it scored 63.3%, a relative
gain of +95 Elo (68–122). These estimates describe this test protocol, not
tournament ratings.

## Conditions

- AMD Ryzen 7 3750H, Windows 11, GCC 16.1.0; `-O3` without `-march=native`.
- Matches: **3 seconds + 50 ms per move**, serial play, colors reversed on
  the same 250 starting positions; no opening book.
- Stockfish 18: one thread, 32 MiB hash. ChessBot: 32 MiB search TT.
- Starting positions: `test_pos_dataset_opening.txt`, shuffled with seed
  `20261002`. Thirty separate positions are reserved for adjusted-SF pilots.
- Performance: eight positions × three repetitions, 1000 ms per search;
  warm-up excluded and search caches cleared between measurements.

See the [benchmark guide](../../docs/METRICS.md) for timing, arbitration and
uncertainty calculations. All versions share the current board and evaluation
modules; they do not reconstruct every historical implementation.

## Stockfish at 2700 Elo

W/D/L means wins, draws and losses from the tested version’s perspective.
Score counts a draw as half a point. Intervals are approximate 95% intervals.

| Version | W / D / L | Score | Estimated Elo | 95% interval | Counted games |
| --- | --- | ---: | ---: | --- | ---: |
| v10 | 41 / 186 / 273 | 26.8% | 2525 | 2500–2551 | 500 |
| v11 | 90 / 159 / 251 | 33.9% | 2584 | 2557–2611 | 500 |
| v12 | 101 / 177 / 222 | 37.9% | 2614 | 2589–2641 | 500 |
| v13 | 90 / 154 / 256 | 33.4% | 2580 | 2551–2608 | 500 |
| v14 | 84 / 154 / 262 | 32.2% | 2571 | 2543–2596 | 500 |
| v15 | 96 / 200 / 204 | 39.2% | 2624 | 2600–2647 | 500 |
| v16 | 124 / 160 / 216 | 40.8% | 2635 | 2609–2662 | 500 |
| v17 | 137 / 211 / 150 | 48.7% | 2691 | 2668–2715 | 498 |

Each comparison schedules 500 games. In v17’s fixed-SF match, one game
reached the 512-ply cap; its entire color pair is excluded, leaving 498
counted games. All other fixed-SF comparisons count 500.

## Against the previous version

| Match | W / D / L | Score | ΔElo | 95% interval |
| --- | --- | ---: | ---: | --- |
| v11 vs v10 | 302 / 112 / 86 | 71.6% | +161 | +132 to +191 |
| v12 vs v11 | 190 / 131 / 179 | 51.1% | +8 | -17 to +33 |
| v13 vs v12 | 194 / 127 / 179 | 51.5% | +10 | -15 to +37 |
| v14 vs v13 | 242 / 103 / 155 | 58.7% | +61 | +33 to +89 |
| v15 vs v14 | 251 / 81 / 168 | 58.3% | +58 | +31 to +86 |
| v16 vs v15 | 246 / 80 / 174 | 57.2% | +50 | +24 to +79 |
| v17 vs v16 | 261 / 111 / 128 | 63.3% | +95 | +68 to +122 |

All predecessor comparisons count 500 games. v10 has no measured predecessor.
The intervals for v12 and v13 include zero: their small positive scores do
not establish a clear improvement in this sample.

## Adjusted Stockfish

For each version I tune Stockfish on three pilot rounds of ten color pairs,
then freeze its Elo and measure 500 fresh games. Pilot results are excluded.
These are separate estimates; I do not average them with the fixed-SF results.

| Version | SF setting | W / D / L | Score | Estimated Elo | 95% interval | Counted games |
| --- | ---: | --- | ---: | ---: | --- | ---: |
| v10 | 2483 | 147 / 155 / 196 | 45.1% | 2449 | 2422–2474 | 498 |
| v11 | 2592 | 140 / 174 / 186 | 45.4% | 2560 | 2535–2584 | 500 |
| v12 | 2504 | 243 / 118 / 139 | 60.4% | 2577 | 2549–2607 | 500 |
| v13 | 2488 | 227 / 119 / 154 | 57.3% | 2539 | 2514–2567 | 500 |
| v14 | 2609 | 128 / 138 / 234 | 39.4% | 2534 | 2508–2561 | 500 |
| v15 | 2522 | 199 / 164 / 137 | 56.2% | 2565 | 2540–2590 | 500 |
| v16 | 2487 | 236 / 135 / 129 | 60.7% | 2563 | 2537–2589 | 500 |
| v17 | 2612 | 201 / 180 / 119 | 58.2% | 2669 | 2643–2694 | 500 |

v10’s adjusted-SF comparison excludes one unfinished color pair. The other
comparisons count 500 games. Changing the SF reference can change the
calibrated estimate; this procedure does not guarantee lower variance.

## Fixed-budget search performance

| Version | Nodes/s | Mean depth | Median depth | Mean depth/s |
| --- | ---: | ---: | ---: | ---: |
| v10 | 3,413,323 | 12.88 | 9.5 | 12.7 |
| v11 | 4,067,217 | 14.04 | 11.0 | 13.9 |
| v12 | 4,406,993 | 14.62 | 11.0 | 14.5 |
| v13 | 4,088,000 | 15.75 | 13.5 | 15.6 |
| v14 | 3,294,251 | 16.08 | 13.5 | 15.9 |
| v15 | 3,001,924 | 17.96 | 14.0 | 17.8 |
| v16 | 2,943,395 | 20.29 | 15.5 | 20.1 |
| v17 | 2,751,798 | 19.75 | 15.0 | 19.5 |

Each version has 24 measured searches. Nodes/s counts quiescence and
interrupted-iteration work. Depth is the last completed iteration; depth/s
is not a linear measure of strength. Search depth also depends on pruning,
so I compare these numbers alongside the match results.

## Data

| File | Contents |
| --- | --- |
| [summary.csv](summary.csv), [summary.json](summary.json) | Original numeric summaries, intervals and pilot results |
| [metadata.json](metadata.json) | Settings, selected positions, environment and source/binary hashes |
| [performance.jsonl](performance.jsonl) | All 192 fixed-budget search measurements |
| [games.jsonl.gz](games.jsonl.gz) | All 11,980 attempted games, including pilots and unfinished games |
| [games.pgn.gz](games.pgn.gz) | Replayable games, compressed with gzip |

I preserved the original summaries and compressed game files without changing
their contents. In metadata I replaced local filesystem paths with relative
paths or executable names. File hashes identify the original inputs and results.
The full per-move search log remains local; its hash is recorded in metadata.
Source hashes retain the filenames used during the run. The two latest search
headers have since been renamed to `bots/v16t.h` and `bots/v17t.h`; the mapping
is recorded in metadata.

My [earlier measurements](../../docs/VERSIONS.md#earlier-measurements) used
different conditions and should not be compared directly with this table.
