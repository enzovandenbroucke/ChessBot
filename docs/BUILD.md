# Native build

I use Windows x64, MinGW-w64 GCC with pthread support, and PowerShell. The code
uses GNU extensions; the older calibration runner also uses Windows process APIs.
I haven't tested native builds with MSVC or Linux.

Put the compiler's `bin` directory on PATH for both compilation and execution.
From the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Configuration Release -Extended
```

`ExecutionPolicy Bypass` applies only to that PowerShell process. If scripts are
already permitted, you can use `.\build.ps1` and `.\test.ps1` directly.

## Targets and options

| Target | Output | Purpose |
| --- | --- | --- |
| `tests` | `engine-tests.exe` | Engine checks and reference perft |
| `compare` | `compare.exe` | Older multithreaded Stockfish calibration runner |
| `gen-header` | `gen-header.exe` | Convert text tables into C headers |

`all` builds these three targets. Outputs go to `build/Release/` or `build/Debug/`.
Release uses `-std=gnu11 -O3 -Wall -Wextra`, without `-march=native`; Debug uses
`-O0 -g`. Assertions remain enabled.

```powershell
.\build.ps1 -Target tests -Configuration Debug
.\build.ps1 -Compiler 'C:\path\to\gcc.exe'
```

Compilation doesn't run the programs. Use `test.ps1` for tests, the
[browser guide](WEB.md) for WebAssembly, and [METRICS.md](METRICS.md) for the
configurable version benchmarks.

## Calibration resources

The older runner uses `stockfish.exe` and `test_pos_dataset_normal.txt` from the
working directory. Run it from the repository root:

```powershell
.\build\Release\compare.exe
```

Its settings are in `CompareThread.c`: v17, 500 games, four workers,
3 seconds + 50 ms per move, Stockfish at 2770 Elo. For new comparisons I use
`metrics.ps1`, which exposes these settings as command-line options.

An opening book is optional. The native loader looks for `resources/komodo.bin`,
or a path set with:

```powershell
$env:CHESSBOT_BOOK = 'C:\path\to\book.bin'
```

Without a book, the engine searches normally. Books and Stockfish executables
are not bundled. Attack and evaluation tables are already compiled into the
engine; the generation tools don't need to run before building.
