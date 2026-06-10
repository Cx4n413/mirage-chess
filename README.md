# Mirage

A UCI chess engine in C++. Bitboards, magic move gen, alpha-beta with the usual
pruning, NNUE-free hand-tuned eval
single file per stage, no dependencies

## Build

```
./build.ps1
```

Needs g++ (mingw w64 works). Outputs `singularity.exe` (because i didn't know what to name it)

## Use

Load the exe in any UCI GC Or talk to
it directly by opening the exe. Ex:

```
position startpos
go movetime 3000
```

`d` prints the board, `eval` prints the static eval, `go perft 6` runs perft.

## Tests

```
python tests/run_tests.py ./singularity.exe
```

perft, a few mates, WAC tactics. `tests/match.py` plays it against another engine (if you dont wanna use a gui for some reason)

## Notes

- one thread only for now
- turn ponder off in the GUI
- standard chess, no 960 or variants


why do summer bridge when you can code a shitty chess bot *cries in fau student*
