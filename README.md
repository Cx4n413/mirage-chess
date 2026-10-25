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
SF-2600: 79.0%, 74–10–16

SF-2675: 68.0%, 61–14–25

SF-2750: 55.5%, 47–17–36

SF-2825: 53.5%, 44–19–37

SF-2900: 39.0%, 25–28–47
(tests ran with the current binary)
should be around 2820 ELO against stockfish, probably higher against human players

perft, a few mates, WAC tactics. `tests/match.py` plays it against another engine (if you dont wanna use a gui for some reason)

## Notes

- one thread only for now
- turn ponder off in the GUI
- standard chess, no 960 or variants

