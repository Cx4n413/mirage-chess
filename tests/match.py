import sys, time, argparse
import chess, chess.engine

OPENINGS = [
    [],
    ["e2e4", "c7c5", "g1f3", "d7d6"],
    ["d2d4", "g8f6", "c2c4", "e7e6"],
    ["e2e4", "e7e5", "g1f3", "b8c6", "f1b5", "a7a6"],
    ["d2d4", "d7d5", "c2c4", "e7e6"],
    ["e2e4", "c7c6", "d2d4", "d7d5"],
]

def play_game(eng_a, eng_b, opening, base, inc, a_is_white):
    board = chess.Board()
    for mv in opening:
        board.push(chess.Move.from_uci(mv))
    clocks = {chess.WHITE: base, chess.BLACK: base}
    engines = {chess.WHITE: eng_a if a_is_white else eng_b,
               chess.BLACK: eng_b if a_is_white else eng_a}
    while not board.is_game_over(claim_draw=True):
        eng = engines[board.turn]
        limit = chess.engine.Limit(
            white_clock=clocks[chess.WHITE], black_clock=clocks[chess.BLACK],
            white_inc=inc, black_inc=inc)
        t0 = time.time()
        result = eng.play(board, limit)
        spent = time.time() - t0
        clocks[board.turn] += inc - spent
        if clocks[board.turn] < -0.5:
            return (0 if board.turn == (chess.WHITE if a_is_white else chess.BLACK) else 1), "time forfeit"
        board.push(result.move)
        if board.fullmove_number > 250:
            return 0.5, "adjudicated draw (move limit)"
    r = board.result(claim_draw=True)
    score_white = {"1-0": 1.0, "0-1": 0.0, "1/2-1/2": 0.5}[r]
    score_a = score_white if a_is_white else 1.0 - score_white
    return score_a, r

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("engine_a")
    ap.add_argument("engine_b")
    ap.add_argument("--elo-b", type=int, default=0, help="limit engine B strength")
    ap.add_argument("--games", type=int, default=6)
    ap.add_argument("--base", type=float, default=5.0)
    ap.add_argument("--inc", type=float, default=0.05)
    args = ap.parse_args()

    eng_a = chess.engine.SimpleEngine.popen_uci(args.engine_a)
    eng_b = chess.engine.SimpleEngine.popen_uci(args.engine_b)
    if args.elo_b:
        eng_b.configure({"UCI_LimitStrength": True, "UCI_Elo": args.elo_b})

    total = 0.0
    for g in range(args.games):
        opening = OPENINGS[(g // 2) % len(OPENINGS)]
        a_white = (g % 2 == 0)
        try:
            score, info = play_game(eng_a, eng_b, opening, args.base, args.inc, a_white)
        except chess.engine.EngineError as e:
            print(f"game {g+1}: ENGINE ERROR {e}", flush=True)
            score, info = 0.0, "engine error"
        total += score
        tag = {1.0: "WIN ", 0.5: "DRAW", 0.0: "LOSS"}[score]
        print(f"game {g+1}: [{tag}] as {'White' if a_white else 'Black'} ({info})", flush=True)

    print(f"\nScore: {total}/{args.games} ({100*total/args.games:.0f}%) vs "
          f"{'Elo ' + str(args.elo_b) if args.elo_b else 'full strength'}")
    eng_a.quit()
    eng_b.quit()

if __name__ == "__main__":
    main()
