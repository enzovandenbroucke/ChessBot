# Credits and sources

I built ChessBot while learning from the resources below. I wrote the engine's
evaluation functions myself, using the PeSTO example to understand how to use
the tables. I also adapted algorithm examples and pseudocode to my own board
and move representations.

| Resource | How I used it |
| --- | --- |
| Sebastian Lague's [Chess Coding Adventure](https://github.com/SebLague/Chess-Coding-Adventure) | Inspiration for the original Java project |
| [Chess Programming Wiki](https://chessprogramming.org/Main_Page) | Search techniques, evaluation ideas and perft reference positions |
| Ronald Friederich's [PeSTO tables](https://chessprogramming.org/PeSTO%27s_Evaluation_Function) | Piece values and piece-square tables |
| [Polyglot format specification](https://hgm.nubati.net/book_format.html) | Random64 constants and opening-book hashing |
| [Komodo opening-book collection](https://github.com/gmcheems-org/free-opening-books) | Optional book used during development; not bundled |
| [Stockfish](https://stockfishchess.org/) | Calibration opponent; I use Stockfish 18 |
| Colin M. L. Burnett's [chess pieces](https://en.wikipedia.org/wiki/User:Cburnett/GFDL_images/Chess) | Browser artwork; [BSD notice and sources](web/pieces/NOTICE.md) |
| Niklas Fiekas's [python-chess](https://github.com/niklasf/python-chess) | Independent benchmark referee, PGN export and Stockfish communication |

AI tools helped me debug difficult cases, discuss architecture and prioritize
search improvements. I also used them while preparing documentation, build
scripts, tests and the browser interface.

## Polyglot and opening books

I obtained the Polyglot constants from the format specification. I no longer
remember whether I adapted its sample hashing code or implemented it from the
description with AI assistance. The page releases its sample code into the
public domain; this is separate from the GPL Polyglot engine.

The Komodo download links to [Donna's opening books](https://github.com/michaeldv/donna_opening_books),
which credits Salvo Spitaleri. I keep the book external rather than redistributing
it here. Stockfish and python-chess are also installed separately.

## Artwork and licensing

The browser uses twelve Cburnett SVGs under the BSD option offered on their
Wikimedia Commons pages. I added a `viewBox` where needed for scaling, without
changing the drawings. The original source links, file hashes and full notice
are kept in [web/pieces/NOTICE.md](web/pieces/NOTICE.md) and its `sources.json`.

I haven't added a general license for my own code. Third-party resources retain
their own licenses and notices.
