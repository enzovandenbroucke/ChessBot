# Development tools

I kept these tools from development, including the experiments I used to search
magic numbers. The engine uses the generated tables directly.

| File | Purpose | Status |
| --- | --- | --- |
| `gen_header.c` | Convert text tables to C headers | Built with `build.ps1 -Target gen-header` |
| `Magic.c` | Search magic numbers and generate attack tables | Historical tool; some globals/types need adapting to the current headers |
| `Test.c` | Print perft counts for each root move | Depends on the old `countMoves()` implementation in the excluded `test.c` |
| `sort.c` | Compare integer sorting routines | Standalone experiment, not used by the search |

For the supported perft and engine checks, use `test.ps1` instead. See
[TESTING.md](../docs/TESTING.md).
