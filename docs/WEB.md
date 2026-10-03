# Browser interface

The interface runs v17a locally as WebAssembly, with plain HTML, CSS and JavaScript.
Python serves files for the local preview; the engine runs in the browser.

## Setup and launch

On Windows, with Git and Python installed, run from the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\setup-web.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\web.ps1
```

Setup installs Emscripten **6.0.10** under `build/emsdk/` and only needs to run once.
It requires internet access and doesn't permanently change PATH. The
[official SDK guide](https://emscripten.org/docs/getting_started/downloads.html)
describes the toolchain.

Open <http://127.0.0.1:8080/>. Leave the terminal running; Ctrl+C stops the preview.
Use `-Port 8081` if the default port is occupied. `web.ps1 -SkipBuild` serves an
existing build.

## Controls

- Click or drag pieces. Yellow markers show legal moves; promotion offers all four pieces.
- Choose your color and clocks, then start a new game. You can resign at any time.
- Analysis mode allows moves for both sides and pauses the clocks.
- Undo/redo buttons and left/right arrow keys navigate history and enter analysis.
- Load or copy a FEN, flip the board, or choose French/English with the flags.

The default clocks are **5 minutes for you** and **5 seconds + 100 ms per move
for the engine**. Three presets fill the next game's settings:

| Preset | Human time | Engine time | Engine increment |
| --- | --- | --- | --- |
| Fast | 3 min | 3 s | 50 ms |
| Standard | 5 min | 5 s | 100 ms |
| Longer | 10 min | 10 s | 200 ms |

Undo/redo restores saved clock values. A FEN can also be supplied through a
URL-encoded `?fen=` parameter.

The evaluation bar shows the last completed search from White's perspective,
and follows the board orientation. `+M2` means mate in two for White; `-M2` means
mate for Black. The separate static score doesn't predict forced mates and may
differ from the search evaluation.

## Build and deployment

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build-web.ps1
```

Release uses `-O3`; `-Configuration Debug` enables `-O0 -g` and Emscripten assertions.
Pass `-Compiler 'C:\path\to\emcc.exe'` for an existing SDK, or
`web.ps1 -Python 'C:\path\to\python.exe'` to select the preview server's Python.

The output in `build/web/` contains the site, `engine.js`, `engine.wasm` and local
SVG artwork. It can be served by a static HTTP/HTTPS host. Opening `index.html`
directly through `file:` isn't supported. No opening book or NNUE weights are
included in the browser build.

I keep the game and search in separate workers so the interface remains responsive.
Background tabs can throttle execution, so browser timings aren't comparable to
native benchmark results. The interface follows the engine's existing draw rules.
Integration checks are described in [TESTING.md](TESTING.md); artwork credits
and terms are in [the piece notice](../web/pieces/NOTICE.md).
