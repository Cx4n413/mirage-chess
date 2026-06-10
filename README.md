# Mirage

A UCI chess engine in C++. Bitboards, magic move gen, alpha-beta with the usual
pruning, NNUE-free hand-tuned eval. Single file per stage, no deps.

## Build

```
./build.ps1
```

Needs g++ (mingw-w64 works). Outputs `singularity.exe`.

## Use

Load `singularity.exe` in any UCI GUI (Arena, cutechess, BanksiaGUI). Or talk to
it directly:

```
position startpos
go movetime 3000
```

`d` prints the board, `eval` prints the static eval, `go perft 6` runs perft.

## Tests

```
python tests/run_tests.py ./singularity.exe
```

perft, a few mates, WAC tactics. `tests/match.py` plays it against another engine.

## Notes

- one thread only for now
- turn ponder off in the GUI
- standard chess, no 960
