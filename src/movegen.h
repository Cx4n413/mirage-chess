#pragma once
#include "position.h"

struct EMove {
    Move move;
    int  score;
};

struct MList {
    EMove list[256];
    int     size = 0;
    void add(Move m) { list[size++].move = m; }
    EMove* begin() { return list; }
    EMove* end()   { return list + size; }
};

void genml(const Position& pos, MList& ml, bool capturesOnly);

#ifdef ENGINE_TU

void genml(const Position& pos, MList& ml, bool capturesOnly) {
    const Color us = pos.sideToMove(), them = ~us;
    const int ksq = pos.kingSq(us);
    const Bitboard occ = pos.pieces(), own = pos.pieces(us), enemy = pos.pieces(them);
    const Bitboard checkers = pos.checkers();
    const Bitboard pinned = pos.blkFor(us) & own;
    const bool inCheck = checkers != 0;
    if (inCheck) capturesOnly = false;

    {
        Bitboard att = KATK[ksq] & ~own;
        if (capturesOnly) att &= enemy;
        Bitboard occNoK = occ ^ sqBB(ksq);
        while (att) {
            int to = popLsb(att);
            if (!pos.isHit(to, them, occNoK))
                ml.add(makeMoveFT(ksq, to));
        }
    }
    if (inCheck && moreThanOne(checkers))
        return;

    Bitboard target;
    if (inCheck) {
        int csq = lsb(checkers);
        target = BTW[ksq][csq] | sqBB(csq);
    } else
        target = ~own;

    const Bitboard quietTarget = target & ~occ;
    const Bitboard capTarget   = target & enemy;

    {
        const Bitboard pawns = pos.pieces(us, PAWN);
        const int up = pawnPush(us);
        const Bitboard rank8 = rankBB(us == WHITE ? 7 : 0);
        const Bitboard rank3 = rankBB(us == WHITE ? 2 : 5);

        auto pinOk = [&](int from, int to) {
            return !(pinned & sqBB(from)) || (LN[ksq][from] & sqBB(to));
        };
        auto addPromos = [&](int from, int to, bool capsMode) {
            ml.add(makeSpecial(PROMOTION, from, to, QUEEN));
            ml.add(makeSpecial(PROMOTION, from, to, KNIGHT));
            if (!capsMode) {
                ml.add(makeSpecial(PROMOTION, from, to, ROOK));
                ml.add(makeSpecial(PROMOTION, from, to, BISHOP));
            }
        };

        Bitboard b1 = (us == WHITE ? shiftN(pawns) : shiftS(pawns)) & ~occ;
        Bitboard b2 = (us == WHITE ? shiftN(b1 & rank3) : shiftS(b1 & rank3)) & quietTarget;
        Bitboard pushes = b1 & (capturesOnly ? (target & rank8) : target);
        while (pushes) {
            int to = popLsb(pushes);
            int from = to - up;
            if (!pinOk(from, to)) continue;
            if (sqBB(to) & rank8) addPromos(from, to, capturesOnly);
            else ml.add(makeMoveFT(from, to));
        }
        while (b2) {
            int to = popLsb(b2);
            int from = to - 2 * up;
            if (!pinOk(from, to)) continue;
            ml.add(makeMoveFT(from, to));
        }

        for (int side = 0; side < 2; side++) {
            int delta = (us == WHITE) ? (side ? 7 : 9) : (side ? -9 : -7);
            Bitboard caps = (us == WHITE ? (side ? shiftNW(pawns) : shiftNE(pawns))
                                         : (side ? shiftSW(pawns) : shiftSE(pawns)))
                          & capTarget;
            while (caps) {
                int to = popLsb(caps);
                int from = to - delta;
                if (!pinOk(from, to)) continue;
                if (sqBB(to) & rank8) addPromos(from, to, false);
                else ml.add(makeMoveFT(from, to));
            }
        }

        int ep = pos.epSquare();
        if (ep != SQ_NONE) {
            Bitboard cands = PATK[them][ep] & pawns;
            while (cands) {
                int from = popLsb(cands);
                int capsq = ep - up;
                Bitboard occAfter = (occ ^ sqBB(from) ^ sqBB(capsq)) | sqBB(ep);
                bool attacked =
                       (rookAttacks(ksq, occAfter)   & pos.pieces(them, ROOK, QUEEN))
                    || (bishopAttacks(ksq, occAfter) & pos.pieces(them, BISHOP, QUEEN))
                    || (NATK[ksq] & pos.pieces(them, KNIGHT))
                    || (PATK[us][ksq] & pos.pieces(them, PAWN) & ~sqBB(capsq));
                if (!attacked)
                    ml.add(makeSpecial(ENPASSANT, from, ep));
            }
        }
    }

    {
        Bitboard knights = pos.pieces(us, KNIGHT) & ~pinned;
        Bitboard mask = capturesOnly ? capTarget : target;
        while (knights) {
            int from = popLsb(knights);
            Bitboard att = NATK[from] & mask;
            while (att) ml.add(makeMoveFT(from, popLsb(att)));
        }
    }

    for (PieceType pt : {BISHOP, ROOK, QUEEN}) {
        Bitboard sliders = pos.pieces(us, pt);
        Bitboard mask = capturesOnly ? capTarget : target;
        while (sliders) {
            int from = popLsb(sliders);
            Bitboard att = attacksBB(pt, from, occ) & mask;
            if (pinned & sqBB(from)) att &= LN[ksq][from];
            while (att) ml.add(makeMoveFT(from, popLsb(att)));
        }
    }

    if (!inCheck && !capturesOnly) {
        int cr = pos.castlingRights();
        auto pathClear = [&](std::initializer_list<int> sqs) {
            for (int s : sqs) if (occ & sqBB(s)) return false;
            return true;
        };
        auto pathSafe = [&](std::initializer_list<int> sqs) {
            for (int s : sqs) if (pos.isHit(s, them, occ)) return false;
            return true;
        };
        if (us == WHITE) {
            if ((cr & WHITE_OO)  && pathClear({5, 6})    && pathSafe({5, 6}))
                ml.add(makeSpecial(CASTLING, SQ_E1, 6));
            if ((cr & WHITE_OOO) && pathClear({1, 2, 3}) && pathSafe({2, 3}))
                ml.add(makeSpecial(CASTLING, SQ_E1, 2));
        } else {
            if ((cr & BLACK_OO)  && pathClear({61, 62})     && pathSafe({61, 62}))
                ml.add(makeSpecial(CASTLING, SQ_E8, 62));
            if ((cr & BLACK_OOO) && pathClear({57, 58, 59}) && pathSafe({58, 59}))
                ml.add(makeSpecial(CASTLING, SQ_E8, 58));
        }
    }
}

#endif
