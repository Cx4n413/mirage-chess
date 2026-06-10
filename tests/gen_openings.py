import random, sys
import chess, chess.engine, chess.pgn

SF = sys.argv[1]
N = 60
PLIES = 8

random.seed(20260610)
eng = chess.engine.SimpleEngine.popen_uci(SF)
seen = set()
lines = []
attempts = 0
while len(lines) < N and attempts < N * 6:
    attempts += 1
    board = chess.Board()
    ok = True
    for ply in range(PLIES):
        infos = eng.analyse(board, chess.engine.Limit(depth=10), multipv=4)
        cands = []
        for info in infos:
            score = info["score"].relative.score(mate_score=10000)
            if score is not None and score > -60:
                cands.append(info["pv"][0])
        if not cands:
            ok = False
            break
        board.push(random.choice(cands))
    if not ok:
        continue
    info = eng.analyse(board, chess.engine.Limit(depth=12))
    cp = info["score"].white().score(mate_score=10000)
    if cp is None or abs(cp) > 60:
        continue
    key = " ".join(m.uci() for m in board.move_stack)
    if key in seen:
        continue
    seen.add(key)
    lines.append(list(board.move_stack))
    print(f"{len(lines)}/{N}", end="\r", flush=True)
eng.quit()

with open("tests/openings.pgn", "w") as f:
    for moves in lines:
        game = chess.pgn.Game()
        node = game
        for mv in moves:
            node = node.add_variation(mv)
        f.write(str(game) + "\n\n")
print(f"\nwrote {len(lines)} openings to tests/openings.pgn")
