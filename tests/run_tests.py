import subprocess, sys, time, re

ENGINE = sys.argv[1] if len(sys.argv) > 1 else "../singularity.exe"

PERFT_SUITE = [
    ("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 5, 4865609),
    ("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 6, 119060324),
    ("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 5, 193690690),
    ("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 6, 11030083),
    ("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 5, 15833292),
    ("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 4, 2103487),
    ("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 4, 3894594),
]

MATE_SUITE = [
    ("6k1/5ppp/8/8/8/8/5PPP/1R4K1 w - - 0 1", "b1b8", 1),
    ("k7/8/8/8/8/8/6R1/K6R w - - 0 1", None, 2),
    ("8/8/8/8/8/4k3/8/Q3K3 w - - 0 1", None, 8),
]

TACTIC_SUITE = [
    ("2rr3k/pp3pp1/1nnqbN1p/3pN3/2pP4/2P3Q1/PPB4P/R4RK1 w - - 0 1", "g3g6"),
    ("8/7p/5k2/5p2/p1p2P2/Pr1pPK2/1P1R3P/8 b - - 0 1", "b3b2"),
    ("5rk1/1ppb3p/p1pb4/6q1/3P1p1r/2P1R2P/PP1BQ1P1/5RKN w - - 0 1", "e3g3"),
    ("r1bq2rk/pp3pbp/2p1p1pQ/7P/3P4/2PB1N2/PP3PPR/2KR4 w - - 0 1", "h6h7"),
    ("5k2/6pp/p1qN4/1p1p4/3P4/2PKP2Q/PP3r2/3R4 b - - 0 1", "c6c4"),
]

class Engine:
    def __init__(self, path):
        self.p = subprocess.Popen([path], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  text=True, bufsize=1)
        self.send("uci")
        self.wait_for("uciok")

    def send(self, cmd):
        self.p.stdin.write(cmd + "\n")
        self.p.stdin.flush()

    def wait_for(self, token, timeout=300):
        lines = []
        start = time.time()
        while time.time() - start < timeout:
            line = self.p.stdout.readline()
            if not line:
                break
            lines.append(line.strip())
            if line.startswith(token):
                return lines
        raise TimeoutError(f"engine never sent: {token}")

    def quit(self):
        self.send("quit")
        self.p.wait(timeout=10)

def test_perft(e):
    print("=== PERFT ===")
    ok = True
    for fen, depth, expected in PERFT_SUITE:
        e.send(f"position fen {fen}")
        e.send(f"go perft {depth}")
        t0 = time.time()
        lines = e.wait_for("Nodes:")
        dt = time.time() - t0
        m = re.search(r"Nodes: (\d+)", lines[-1])
        got = int(m.group(1))
        status = "OK " if got == expected else "FAIL"
        if got != expected:
            ok = False
        print(f"  [{status}] d{depth} {fen[:40]:42s} got {got:>12,} want {expected:>12,} ({dt:.1f}s)")
    return ok

def search_bestmove(e, fen, movetime=3000):
    e.send(f"position fen {fen}")
    e.send(f"go movetime {movetime}")
    lines = e.wait_for("bestmove")
    best = lines[-1].split()[1]
    mate = None
    for ln in reversed(lines):
        m = re.search(r"score mate (-?\d+)", ln)
        if m:
            mate = int(m.group(1))
            break
        if "score cp" in ln:
            break
    return best, mate

def test_mates(e):
    print("=== MATES ===")
    ok = True
    for fen, bestmove, n in MATE_SUITE:
        best, mate = search_bestmove(e, fen, 3000)
        good = (mate is not None and 0 < mate <= n) and (bestmove is None or best == bestmove)
        if not good:
            ok = False
        print(f"  [{'OK ' if good else 'FAIL'}] want mate<={n}, got mate={mate} best={best}")
    return ok

def test_tactics(e):
    print("=== TACTICS (WAC subset, 3s each) ===")
    hits = 0
    for fen, bm in TACTIC_SUITE:
        best, _ = search_bestmove(e, fen, 3000)
        hit = best == bm
        hits += hit
        print(f"  [{'HIT ' if hit else 'MISS'}] want {bm} got {best}")
    print(f"  {hits}/{len(TACTIC_SUITE)}")
    return hits >= len(TACTIC_SUITE) - 1

if __name__ == "__main__":
    e = Engine(ENGINE)
    results = []
    results.append(("perft", test_perft(e)))
    results.append(("mates", test_mates(e)))
    results.append(("tactics", test_tactics(e)))
    e.quit()
    print("\n=== SUMMARY ===")
    allok = True
    for name, ok in results:
        print(f"  {name}: {'PASS' if ok else 'FAIL'}")
        allok &= ok
    sys.exit(0 if allok else 1)
