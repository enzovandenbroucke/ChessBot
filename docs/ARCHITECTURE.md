# Architecture

I use both a 64-square mailbox and bitboards. The mailbox gives direct access
to a piece on a square; bitboards make attack queries and occupancy operations
cheap. Sliding attacks use magic multiplication and precomputed lookup tables.

## Modules

| Files | Role |
| --- | --- |
| `Types.h` | Position state, search limits and table entries |
| `Piece.h`, `Move.h` | Piece codes and packed move accessors |
| `Position.c/.h` | Initialization, FEN, allocation, repetition history and opening books |
| `Board.c/.h` | Attacks, move generation, move application and reversal |
| `Data.c/.h` | Attack tables, magic numbers, masks and evaluation constants |
| `Zobrist.c/.h`, `PolyglotZobrist.h` | Internal hashes and opening-book keys |
| `Eval.c/.h` | Classical evaluation and incremental material/PST updates |
| `TTable.h` | Transposition-table lookup, replacement and mate-score conversion |
| `Bot.h`, `bots/v*.h` | Shared search helpers and versioned search implementations |
| `web/`, `ui/Game.c/.h` | Browser interface, WebAssembly bridge and game state |
| `benchmarks/` | Version adapters, paired matches and measurement summaries |
| `CompareThread.c`, `bots/stockfish.h` | Older native calibration runner |
| `tests/`, `helpers/` | Automated checks and development tools |

Search functions live in versioned headers included by each program. The build
scripts list compilation units explicitly, so historical entry points aren't
compiled accidentally.

## Representation and search

Squares run from a1 = 0 to h8 = 63. White and Black have bitboard indices 0 and 1.
I use accessors for piece and move fields rather than exposing their encoding
throughout the engine.

A full 64-bit move stores its origin, destination, promotion, captured piece,
ordering score and the state needed to undo it. The transposition table uses a
compact move. Move buffers, history and search data use fixed arrays to avoid
repeated allocations during recursion.

v17's entry point is `getBestMoveDynamic17`, with time arguments in milliseconds.
It uses iterative deepening, aspiration windows and a dynamic time budget.
The recursive search makes pseudo-legal moves and checks legality afterward;
the interface uses legal move generation directly.

`evalNew` blends middlegame and endgame material/PST scores with pawn structure,
bishop pair, rook activity and king safety. Pawn terms have their own hash cache.
The score is relative to the side to move; `evalNewWhite` converts it for display.
I kept mobility and earlier evaluation routines for experiments, but mobility
isn't enabled in the current evaluator.

## Isolation

The browser keeps game state and engine search in separate workers. Search uses
a position snapshot, so it doesn't modify the board being displayed. Cancelling
a search terminates its worker.

The benchmarks run games serially with separate engine processes. The older
native runner parallelizes independent games across threads; it doesn't parallelize
one search tree. Search histories, PV arrays, killers and the pawn cache are
thread-local, while compiled tables and opening-book data are shared.
