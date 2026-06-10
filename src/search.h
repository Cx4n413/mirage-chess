#pragma once
#include "movegen.h"
#include "eval.h"
#include "tt.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>

constexpr int MAX_PLY = 128;

struct Limits {
    int     depth       = MAX_PLY;
    int64_t movetime    = 0;
    int64_t time[2]     = {0, 0};
    int64_t inc[2]      = {0, 0};
    int     movestogo   = 0;
    uint64_t nodes      = 0;
    bool    infinite    = false;
};

struct Line {
    int  len = 0;
    Move m[MAX_PLY];
};

struct SStack {
    Move  killers[2];
    Move  currentMove;
    Piece movedPiece;
    int   staticEval;
    int   ply;
};

class Eng {
public:
    std::atomic<bool> stopFlag{false};

    void wipeHist() {
        memset(mhist, 0, sizeof(mhist));
        memset(chist, 0, sizeof(chist));
        memset(cmove, 0, sizeof(cmove));
    }
    void runSearch(Position& pos, const Limits& limits);
    uint64_t perft(Position& pos, int depth, bool root);
    uint64_t nodes = 0;

private:
    using Clock = std::chrono::steady_clock;
    Clock::time_point startTime;
    int64_t hardLimit = 0, softLimit = 0;
    Limits lim;
    bool stopped = false;

    int16_t mhist[COLOR_NB][64 * 64];
    int16_t chist[12][64][12 * 64];
    Move    cmove[12][64];
    int     lmr[64][64];
    SStack stack[MAX_PLY + 4];

    int64_t elapsed() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - startTime).count();
    }
    void ckTime() {
        if ((nodes & 1023) != 0) return;
        if (stopFlag.load(std::memory_order_relaxed)) { stopped = true; return; }
        if (hardLimit && elapsed() >= hardLimit) stopped = true;
        if (lim.nodes && nodes >= lim.nodes) stopped = true;
    }

    void setupLmr() {
        for (int d = 1; d < 64; d++)
            for (int m = 1; m < 64; m++)
                lmr[d][m] = int(0.75 + std::log(d) * std::log(m) / 2.25);
        lmr[0][0] = 0;
    }

    void bumpHist(int16_t& h, int bonus) {
        h += bonus - int(h) * std::abs(bonus) / 8192;
    }

    int qScore(const Position& pos, Move m, const SStack* ss) {
        int from = fromSq(m), to = toSq(m);
        int s = mhist[pos.sideToMove()][from * 64 + to];
        if (ss->ply > 0 && (ss - 1)->currentMove != MOVE_NONE) {
            Piece pc = pos.pieceOn(from);
            s += chist[(ss - 1)->movedPiece][toSq((ss - 1)->currentMove)][pc * 64 + to];
        }
        return s;
    }

    void orderML(const Position& pos, MList& ml, Move ttMove, const SStack* ss) {
        Move counter = MOVE_NONE;
        if (ss->ply > 0 && (ss - 1)->currentMove != MOVE_NONE)
            counter = cmove[(ss - 1)->movedPiece][toSq((ss - 1)->currentMove)];

        for (int i = 0; i < ml.size; i++) {
            Move m = ml.list[i].move;
            if (m == ttMove)                       { ml.list[i].score = 2000000; continue; }
            if (pos.isCapture(m)) {
                PieceType victim = moveType(m) == ENPASSANT ? PAWN : typeOf(pos.pieceOn(toSq(m)));
                int base = SEEValue[victim] * 100 - SEEValue[typeOf(pos.pieceOn(fromSq(m)))] / 10;
                ml.list[i].score = (pos.see(m, 0) ? 1000000 : -1000000) + base;
            } else if (moveType(m) == PROMOTION) {
                ml.list[i].score = promoType(m) == QUEEN ? 1000000 + 90000 : -1100000;
            } else if (m == ss->killers[0])        { ml.list[i].score = 900000; }
            else if (m == ss->killers[1])          { ml.list[i].score = 850000; }
            else if (m == counter)                 { ml.list[i].score = 800000; }
            else                                   { ml.list[i].score = qScore(pos, m, ss); }
        }
    }

    Move pick(MList& ml, int idx) {
        int best = idx;
        for (int i = idx + 1; i < ml.size; i++)
            if (ml.list[i].score > ml.list[best].score) best = i;
        std::swap(ml.list[idx], ml.list[best]);
        return ml.list[idx].move;
    }

    int qs(Position& pos, int alpha, int beta, SStack* ss);
    int search(Position& pos, int alpha, int beta, int depth, SStack* ss,
               bool cutNode, Line* pv);
    void prInfo(int depth, int score, const Line& pv);
};

extern Eng eng;

#ifdef ENGINE_TU

Eng eng;

std::string mv2str(Move m) {
    if (m == MOVE_NONE) return "0000";
    std::string s;
    s += char('a' + fileOf(fromSq(m)));
    s += char('1' + rankOf(fromSq(m)));
    s += char('a' + fileOf(toSq(m)));
    s += char('1' + rankOf(toSq(m)));
    if (moveType(m) == PROMOTION) s += "nbrq"[promoType(m) - KNIGHT];
    return s;
}

uint64_t Eng::perft(Position& pos, int depth, bool root) {
    MList ml;
    genml(pos, ml, false);
    if (depth == 1 && !root) return ml.size;
    uint64_t total = 0;
    for (int i = 0; i < ml.size; i++) {
        uint64_t cnt;
        if (depth == 1) cnt = 1;
        else {
            pos.makeMove(ml.list[i].move);
            cnt = perft(pos, depth - 1, false);
            pos.unmakeMove(ml.list[i].move);
        }
        total += cnt;
        if (root) printf("%s: %llu\n", mv2str(ml.list[i].move).c_str(), (unsigned long long)cnt);
    }
    return total;
}

int Eng::qs(Position& pos, int alpha, int beta, SStack* ss) {
    nodes++;
    ckTime();
    if (stopped) return 0;
    if (pos.isDraw()) return 0;
    if (ss->ply >= MAX_PLY - 1) return pos.checkers() ? 0 : evscore(pos);

    const bool pvNode = beta - alpha > 1;
    const bool inCheck = pos.checkers();

    bool ttHit;
    TEnt* tte = TT.probe(pos.key(), ttHit);
    Move ttMove = ttHit ? tte->move : MOVE_NONE;
    int ttScore = ttHit ? tt2v(tte->score, ss->ply) : VALUE_NONE;
    if (!pvNode && ttHit && ttScore != VALUE_NONE
        && (tte->bound() & (ttScore >= beta ? BOUND_LOWER : BOUND_UPPER)))
        return ttScore;

    int bestScore, rawEval = VALUE_NONE;
    if (inCheck)
        bestScore = -VALUE_INFINITE;
    else {
        rawEval = (ttHit && tte->eval != VALUE_NONE) ? tte->eval : evscore(pos);
        bestScore = rawEval;
        if (ttHit && ttScore != VALUE_NONE
            && (tte->bound() & (ttScore > bestScore ? BOUND_LOWER : BOUND_UPPER)))
            bestScore = ttScore;
        if (bestScore >= beta) return bestScore;
        if (bestScore > alpha) alpha = bestScore;
    }

    MList ml;
    genml(pos, ml, true);
    if (inCheck && ml.size == 0) return -VALUE_MATE + ss->ply;
    orderML(pos, ml, ttMove, ss);

    Move bestMove = MOVE_NONE;
    for (int i = 0; i < ml.size; i++) {
        Move m = pick(ml, i);
        if (!inCheck) {
            if (!pos.see(m, 0)) continue;
            PieceType victim = moveType(m) == ENPASSANT ? PAWN
                             : pos.pieceOn(toSq(m)) != NO_PIECE ? typeOf(pos.pieceOn(toSq(m))) : PAWN;
            if (moveType(m) != PROMOTION && rawEval + SEEValue[victim] + 200 <= alpha)
                continue;
        }
        ss->currentMove = m;
        ss->movedPiece = pos.pieceOn(fromSq(m));
        pos.makeMove(m);
        int score = -qs(pos, -beta, -alpha, ss + 1);
        pos.unmakeMove(m);
        if (stopped) return 0;
        if (score > bestScore) {
            bestScore = score;
            if (score > alpha) {
                alpha = score;
                bestMove = m;
                if (alpha >= beta) break;
            }
        }
    }

    TT.save(tte, pos.key(), v2tt(bestScore, ss->ply), rawEval, bestMove, 0,
            bestScore >= beta ? BOUND_LOWER : BOUND_UPPER);
    return bestScore;
}

int Eng::search(Position& pos, int alpha, int beta, int depth, SStack* ss,
                     bool cutNode, Line* pv) {
    const bool pvNode = beta - alpha > 1;
    const bool rootNode = ss->ply == 0;
    if (pv) pv->len = 0;

    if (depth <= 0) return qs(pos, alpha, beta, ss);

    nodes++;
    ckTime();
    if (stopped) return 0;

    if (!rootNode) {
        if (pos.isDraw()) return 0;
        if (ss->ply >= MAX_PLY - 1) return pos.checkers() ? 0 : evscore(pos);

        alpha = std::max(alpha, -VALUE_MATE + ss->ply);
        beta  = std::min(beta,  VALUE_MATE - ss->ply - 1);
        if (alpha >= beta) return alpha;
    }

    const bool inCheck = pos.checkers();
    (ss + 1)->killers[0] = (ss + 1)->killers[1] = MOVE_NONE;

    bool ttHit;
    TEnt* tte = TT.probe(pos.key(), ttHit);
    Move ttMove = ttHit ? tte->move : MOVE_NONE;
    int ttScore = ttHit ? tt2v(tte->score, ss->ply) : VALUE_NONE;
    if (!pvNode && ttHit && tte->depth >= depth && ttScore != VALUE_NONE
        && (tte->bound() & (ttScore >= beta ? BOUND_LOWER : BOUND_UPPER)))
        return ttScore;

    int rawEval = VALUE_NONE, eval = VALUE_NONE;
    if (!inCheck) {
        rawEval = (ttHit && tte->eval != VALUE_NONE) ? tte->eval : evscore(pos);
        eval = rawEval;
        if (ttHit && ttScore != VALUE_NONE
            && (tte->bound() & (ttScore > eval ? BOUND_LOWER : BOUND_UPPER)))
            eval = ttScore;
    }
    ss->staticEval = rawEval;

    const bool improving = !inCheck && ss->ply >= 2
        && (ss - 2)->staticEval != VALUE_NONE && rawEval > (ss - 2)->staticEval;

    if (!pvNode && !inCheck && std::abs(beta) < MATE_IN_MAX) {

        if (depth <= 8 && eval - 80 * std::max(depth - improving, 1) >= beta)
            return eval;

        if (depth >= 3 && eval >= beta && rawEval >= beta
            && ss->ply >= 1 && (ss - 1)->currentMove != MOVE_NONE
            && pos.npMat(pos.sideToMove()) > 0) {
            int R = 3 + depth / 3 + std::min((eval - beta) / 200, 3);
            ss->currentMove = MOVE_NONE;
            ss->movedPiece = NO_PIECE;
            pos.makeNullMove();
            int v = -search(pos, -beta, -beta + 1, depth - R, ss + 1, !cutNode, nullptr);
            pos.unmakeNullMove();
            if (stopped) return 0;
            if (v >= beta) return v >= MATE_IN_MAX ? beta : v;
        }
    }

    if (depth >= 4 && !ttMove) depth--;

    MList ml;
    genml(pos, ml, false);
    if (ml.size == 0) return inCheck ? -VALUE_MATE + ss->ply : 0;
    orderML(pos, ml, ttMove, ss);

    int bestScore = -VALUE_INFINITE;
    Move bestMove = MOVE_NONE;
    int moveCount = 0;
    Move quietsTried[96];
    int quietCount = 0;
    Line childPV;

    for (int i = 0; i < ml.size; i++) {
        Move m = pick(ml, i);
        moveCount++;
        const bool isQuiet = pos.isQuiet(m);
        const bool givesChk = pos.givesChk(m);

        if (!rootNode && bestScore > -MATE_IN_MAX
            && pos.npMat(pos.sideToMove()) > 0) {
            if (isQuiet) {
                if (!pvNode && !inCheck && depth <= 8
                    && moveCount > (3 + depth * depth) / (2 - improving))
                    continue;
                if (!pvNode && !inCheck && depth <= 7 && !givesChk
                    && eval != VALUE_NONE && eval + 120 + 140 * depth <= alpha)
                    continue;
                if (depth <= 8 && !pos.see(m, -60 * depth))
                    continue;
            } else if (depth <= 8 && !pos.see(m, -180 * depth))
                continue;
        }

        int extension = (givesChk && pos.see(m, 0)) ? 1 : 0;

        ss->currentMove = m;
        ss->movedPiece = pos.pieceOn(fromSq(m));
        pos.makeMove(m);
        int newDepth = depth - 1 + extension;
        int score;

        if (depth >= 3 && moveCount > 1 + 2 * rootNode && isQuiet) {
            int r = lmr[std::min(depth, 63)][std::min(moveCount, 63)];
            r += !improving;
            r += cutNode;
            r -= pvNode;
            r -= givesChk;
            int d = std::clamp(newDepth - r, 1, newDepth);
            score = -search(pos, -alpha - 1, -alpha, d, ss + 1, true, nullptr);
            if (score > alpha && d < newDepth)
                score = -search(pos, -alpha - 1, -alpha, newDepth, ss + 1, !cutNode, nullptr);
        } else if (!pvNode || moveCount > 1)
            score = -search(pos, -alpha - 1, -alpha, newDepth, ss + 1, !cutNode, nullptr);

        if (pvNode && (moveCount == 1 || score > alpha)) {
            childPV.len = 0;
            score = -search(pos, -beta, -alpha, newDepth, ss + 1, false, &childPV);
        }

        pos.unmakeMove(m);
        if (stopped) return 0;

        if (score > bestScore) {
            bestScore = score;
            if (score > alpha) {
                bestMove = m;
                alpha = score;
                if (pv) {
                    pv->m[0] = m;
                    memcpy(pv->m + 1, childPV.m, childPV.len * sizeof(Move));
                    pv->len = childPV.len + 1;
                }
                if (alpha >= beta) {

                    int bonus = std::min(150 * depth - 100, 1600);
                    if (isQuiet) {
                        if (ss->killers[0] != m) {
                            ss->killers[1] = ss->killers[0];
                            ss->killers[0] = m;
                        }
                        Color us = pos.sideToMove();
                        bumpHist(mhist[us][fromSq(m) * 64 + toSq(m)], bonus);
                        if (ss->ply > 0 && (ss - 1)->currentMove != MOVE_NONE) {
                            Piece prevPc = (ss - 1)->movedPiece;
                            int prevTo = toSq((ss - 1)->currentMove);
                            cmove[prevPc][prevTo] = m;
                            bumpHist(chist[prevPc][prevTo][ss->movedPiece * 64 + toSq(m)], bonus);
                        }
                        for (int q = 0; q < quietCount; q++) {
                            Move qm = quietsTried[q];
                            bumpHist(mhist[us][fromSq(qm) * 64 + toSq(qm)], -bonus);
                            if (ss->ply > 0 && (ss - 1)->currentMove != MOVE_NONE) {
                                Piece prevPc = (ss - 1)->movedPiece;
                                int prevTo = toSq((ss - 1)->currentMove);
                                bumpHist(chist[prevPc][prevTo][pos.pieceOn(fromSq(qm)) * 64 + toSq(qm)], -bonus);
                            }
                        }
                    }
                    break;
                }
            }
        }
        if (isQuiet && quietCount < 96) quietsTried[quietCount++] = m;
    }

    Bound bound = bestScore >= beta ? BOUND_LOWER
                : (pvNode && bestMove != MOVE_NONE) ? BOUND_EXACT : BOUND_UPPER;
    TT.save(tte, pos.key(), v2tt(bestScore, ss->ply), rawEval, bestMove, depth, bound);
    return bestScore;
}

void Eng::prInfo(int depth, int score, const Line& pv) {
    int64_t ms = std::max<int64_t>(elapsed(), 1);
    printf("info depth %d score ", depth);
    if (std::abs(score) >= MATE_IN_MAX) {
        int mate = (VALUE_MATE - std::abs(score) + 1) / 2;
        printf("mate %d", score > 0 ? mate : -mate);
    } else
        printf("cp %d", score);
    printf(" nodes %llu nps %llu time %lld pv",
           (unsigned long long)nodes, (unsigned long long)(nodes * 1000 / ms), (long long)ms);
    for (int i = 0; i < pv.len; i++)
        printf(" %s", mv2str(pv.m[i]).c_str());
    printf("\n");
    fflush(stdout);
}

void Eng::runSearch(Position& pos, const Limits& limits) {

    lim = limits;
    nodes = 0;
    stopped = false;
    startTime = Clock::now();
    TT.newSearch();
    setupLmr();

    hardLimit = softLimit = 0;
    Color us = pos.sideToMove();
    if (limits.movetime)
        hardLimit = softLimit = std::max<int64_t>(limits.movetime - 30, 10);
    else if (limits.time[us]) {
        int mtg = limits.movestogo ? std::min(limits.movestogo, 40) : 25;
        int64_t t = limits.time[us], inc = limits.inc[us];
        int64_t alloc = t / mtg + 3 * inc / 4;
        hardLimit = std::min(4 * alloc, std::max<int64_t>(t - 50, t / 2));
        softLimit = std::min(alloc, hardLimit);
    }

    for (int i = 0; i < MAX_PLY + 4; i++) {
        stack[i] = SStack{};
        stack[i].currentMove = MOVE_NONE;
        stack[i].movedPiece = NO_PIECE;
        stack[i].staticEval = VALUE_NONE;
        stack[i].ply = i - 2;
    }
    SStack* ss = stack + 2;

    Move bestMove = MOVE_NONE;
    int prevScore = 0;

    for (int depth = 1; depth <= std::min(limits.depth, MAX_PLY - 1); depth++) {
        Line pv;
        int score;
        if (depth >= 4) {

            int delta = 18;
            int alpha = std::max(prevScore - delta, -VALUE_INFINITE);
            int beta  = std::min(prevScore + delta,  VALUE_INFINITE);
            for (;;) {
                score = search(pos, alpha, beta, depth, ss, false, &pv);
                if (stopped) break;
                if (score <= alpha) {
                    beta  = (alpha + beta) / 2;
                    alpha = std::max(score - delta, -VALUE_INFINITE);
                } else if (score >= beta)
                    beta = std::min(score + delta, VALUE_INFINITE);
                else break;
                delta += delta / 2;
            }
        } else
            score = search(pos, -VALUE_INFINITE, VALUE_INFINITE, depth, ss, false, &pv);

        if (stopped && depth > 1) break;
        if (pv.len > 0) bestMove = pv.m[0];
        prevScore = score;
        prInfo(depth, score, pv);

        if (softLimit && elapsed() > softLimit * 55 / 100) break;
        if (std::abs(score) >= MATE_IN_MAX && depth >= 10
            && VALUE_MATE - std::abs(score) <= depth - 4) break;
    }

    if (bestMove == MOVE_NONE) {
        MList ml;
        genml(pos, ml, false);
        if (ml.size) bestMove = ml.list[0].move;
    }
    printf("bestmove %s\n", mv2str(bestMove).c_str());
    fflush(stdout);
}

#endif
