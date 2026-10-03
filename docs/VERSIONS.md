# Version history

I kept earlier search implementations to compare each change with the previous
version. This repository includes v10–v17a; the earlier versions are documented
below. The current engine is v17a, with classical evaluation.

## Development

| Version | Change | Purpose |
| --- | --- | --- |
| v0 | Negamax, alpha-beta, iterative deepening | Search progressively deeper within a time budget |
| v1 | Quiescence search | Extend evaluation through tactical exchanges |
| v2 | Capture ordering, incremental static evaluation | Improve move ordering and reduce repeated evaluation work |
| v3 | Zobrist hashing and transposition table | Reuse previously searched positions |
| v4 | Null-move pruning | Reduce search in positions with a strong static advantage |
| v5 | PeSTO piece-square tables and piece values | Introduce positional, phase-dependent evaluation |
| v6 | Polyglot opening book, originally Komodo | Use book moves and diversify openings |
| v7 | Principal-variation ordering, depth-preferred TT replacement | Reuse promising lines and deeper entries |
| v8 | Late legality checks | Integrate legality checking with search |
| v9 | Killer moves | Prioritize quiet moves that previously caused cutoffs |
| v10 | Bitboard migration | Accelerate board operations and sliding attacks |
| v11 | History, aspiration windows, check extensions, reverse futility and delta pruning | Improve ordering and selective search |
| v12 | Static Exchange Evaluation (SEE) | Estimate capture sequences for search decisions |
| v13 | Dynamic late-move reduction formula | Adjust reductions with depth and move order |
| v14 | Futility pruning, razoring, bishop-pair bonus | Further selective search and evaluation terms |
| v15 | Fixes, pawn structure and mobility experiments | Improve the classical evaluator |
| v16 | Late-move pruning, countermoves, internal iterative reduction, singular extensions | Further search selectivity |
| v17 | King safety, rook activity and dynamic time management | Improve evaluation and allocation of thinking time |

These are the stages of development, rather than isolated feature switches.
The searches now share the current board and evaluation modules: v15 and v17 use
`evalNew`, while v16 uses `eval`. I also experimented with mobility, but it isn't
enabled in the current evaluator.

## Earlier measurements

I ran my historical tests against Stockfish 18 on an AMD Ryzen 7 3750H.
I didn't keep a full configuration and raw-game record for every run. The
figures below are my earlier estimates, with the original uncertainty values.

| Version | Estimate or comparison |
| --- | --- |
| v4 | +131 Elo vs v3 |
| v5 | +178 Elo vs v4 |
| v6 | +25 Elo; reference not recorded |
| v7 | +84 Elo; reference not recorded |
| v8 | 2507 ± 14; +79 in my notes |
| v9 | 2508 ± 15 |
| v10 | 2545 ± 20; approximately 2× faster after the bitboard migration |
| v11 | 2642 ± 19; vs v10: 534 W / 256 D / 210 L, +117 ± 19 |
| v12 | 2708 ± 18; vs v11: 452 W / 282 D / 266 L, +65 ± 18 |
| v13 | No measurement recorded |
| v14 | 2720 ± 19 at 100 ms/move; 2789 ± 18 at 1000 ms/move |
| v15 | 2718 ± 17; +56 ± 19 vs v14 |
| v16 | 2714 ± 19 |
| v17 | 2787 ± 16 at 3 s/game + 50 ms/move; 2786 under the previous scenario |

I changed the timing protocol during development, so these figures aren't one
controlled comparison. They are estimates against limited-strength Stockfish,
not tournament ratings. The current sources also don't reproduce the original
board and evaluator for every historical version.

Additional comparison totals from my notes are v4 vs v3 `468-425-107`, v5 vs v4
`545-382-73`, v6 `260-553-187`, v7 `500-238-262` and v8 `477-271-252`.
I didn't label the order of wins, draws and losses alongside those totals.

## Current comparisons

I use the [metrics tooling](METRICS.md) to compare the included versions under a
common protocol, saving raw searches, games and configuration alongside the table.
It measures matches against Stockfish and the previous version, nodes/s, and
completed search depth at a fixed budget.

I kept `ancientEvalCompute`, `tapered_eval`, `evalQS` and `evaluateMobility` for
earlier evaluation experiments. The included searches don't call them today.
