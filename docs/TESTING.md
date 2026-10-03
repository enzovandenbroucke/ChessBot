# Tests

From the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Configuration Release -Extended
```

The script builds and runs the engine tests, and reports compilation or test
failures. It accepts `-Compiler` like `build.ps1`. These checks need no Stockfish,
opening book or network access.

## Engine coverage

- Packed moves, compact encoding and mate-score conversion.
- FEN round trips and agreement between mailbox, bitboards and occupancy.
- Incremental hashes and evaluation compared with full recomputation.
- Move application and reversal, including castling, en passant and promotions.
- Checkmate, stalemate and a deterministic sequence of moves followed by rollback.
- Six reference perft positions, through depth 3 in quick mode or depth 4 in
  extended mode, using both move-generation paths.

The expected perft counts come from the [Chess Programming Wiki](https://chessprogramming.org/Perft_Results).
The extended suite passed **86,660 checks** with GCC 16.1.0 and `-O3` on Windows.
That count includes repeated assertions across moves and positions, rather than
86,660 separate test cases.

These tests focus on the shared board and evaluation modules. They don't measure
playing strength or exhaustively test search and concurrency.

## Browser integration

After building the WebAssembly module, run:

```powershell
node .\tests\web_tests.mjs
```

The suite loads the actual compiled module and checks legal moves, FEN rejection,
history, special moves, search isolation, mate distances and clock accounting.
See [WEB.md](WEB.md) for setup. I also check the board on desktop and mobile layouts.

## Benchmark tooling

The benchmark tests cover score/Elo calculations, color pairing, incomplete-game
handling, forfeits and the adapters for all eight versions. Commands are in
[METRICS.md](METRICS.md). Short integration trials validate the tooling; the longer
paired matches provide the strength measurements.
