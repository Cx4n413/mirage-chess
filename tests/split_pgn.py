import chess.pgn, sys, collections

src = sys.argv[1] if len(sys.argv) > 1 else "tests/gauntlet.pgn"
out = {}
counts = collections.Counter()
results = collections.defaultdict(lambda: [0, 0, 0])

with open(src) as f:
    while True:
        game = chess.pgn.read_game(f)
        if game is None:
            break
        w, b = game.headers["White"], game.headers["Black"]
        opp = w if w.startswith("SF-") else b
        if opp not in out:
            out[opp] = open(f"tests/gauntlet_{opp}.pgn", "w")
        out[opp].write(str(game) + "\n\n")
        counts[opp] += 1
        r = game.headers["Result"]
        sing_white = w == "Singularity"
        if r == "1/2-1/2":
            results[opp][1] += 1
        elif (r == "1-0") == sing_white:
            results[opp][0] += 1
        else:
            results[opp][2] += 1

for f in out.values():
    f.close()
print(f"{'opponent':>10} {'games':>6} {'W':>4} {'D':>4} {'L':>4} {'score':>7}")
for opp in sorted(counts):
    w, d, l = results[opp]
    n = counts[opp]
    print(f"{opp:>10} {n:>6} {w:>4} {d:>4} {l:>4} {100*(w+0.5*d)/n:>6.1f}%")
