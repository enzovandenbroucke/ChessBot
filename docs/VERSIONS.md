# Version history

I kept earlier search implementations to compare each change with the previous
version. This repository includes v10–v17; the earlier versions are documented
below. The current engine is v17, with classical evaluation.

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

## Current comparisons

My [October 2026 results](../results/2026-10-02/README.md) compare all eight
included versions under one protocol: 3 seconds + 50 ms per move, no opening
book, paired colors, and Stockfish 18 at a fixed 2700 Elo or an adjusted setting.
The same run measures nodes/s and completed depth with 1000 ms search budgets.

v17 scored 48.7% against fixed SF, an estimated **2691 Elo** (95% interval:
2668–2715). Against v16 it scored 63.3%, or **+95 Elo** (68–122). The small
positive differences for v12 and v13 have intervals spanning zero, so this
sample does not establish an improvement for those transitions.

The [full tables and data](../results/2026-10-02/README.md) contain every version's
results. [METRICS.md](METRICS.md) describes the measurement and uncertainty
calculations. These comparisons use the shared board and evaluation modules,
not a reconstruction of every original release.

## Earlier measurements

I ran my historical tests against Stockfish 18 on an AMD Ryzen 7 3750H.
I didn't keep a full configuration and raw-game record for every run. The
figures below are my earlier estimates, with the original uncertainty values.

| Version | Estimate or comparison |
| --- | --- |
| v4 | +131 Elo vs v3 |
| v5 | +178 Elo vs v4 |
| v6 | +25 Elo vs v5 |
| v7 | +84 Elo vs v6 |
| v8 | 2507 ± 14 against SF; +79 Elo vs v7 |
| v9 | 2508 ± 15 against SF |
| v10 | 2545 ± 20 against SF; approximately 2× faster after the bitboard migration |
| v11 | 2642 ± 19 against SF; +117 Elo vs v10 |
| v12 | 2708 ± 18 against SF; +65 Elo vs v11 |
| v13 | No measurement recorded |
| v14 | 2720 ± 19 against SF |
| v15 | 2718 ± 17 against SF; +56 Elo vs v14 |
| v16 | 2714 ± 19 against SF |
| v17 | 2787 ± 16 at 3 s/game + 50 ms/move against SF; 2786 under the previous scenario |

I changed the timing protocol during development, so these figures aren't one
controlled comparison. They are estimates against limited-strength Stockfish,
not tournament ratings. The current sources also don't reproduce the original
board and evaluator for every historical version.

For the historical v17 result I used `CompareThread`, SF at 2770, four workers
and `test_pos_dataset_normal.txt`. The October 2026 benchmark uses different
starting positions, Stockfish setup and draw arbitration. Its estimates should
not be read as a direct gain or loss against the historical 2787 figure.
