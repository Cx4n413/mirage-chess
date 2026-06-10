#pragma once
#include "bitboard.h"
#include <sstream>

enum MoveType { NORMAL = 0, PROMOTION = 1, ENPASSANT = 2, CASTLING = 3 };
using Move = uint16_t;
constexpr Move MOVE_NONE = 0;

constexpr Move makeMoveFT(int from, int to) { return Move((from << 6) | to); }
constexpr Move makeSpecial(MoveType t, int from, int to, PieceType promo = KNIGHT) {
    return Move((t << 14) | ((promo - KNIGHT) << 12) | (from << 6) | to);
}
constexpr int fromSq(Move m)        { return (m >> 6) & 63; }
constexpr int toSq(Move m)          { return m & 63; }
constexpr MoveType moveType(Move m) { return MoveType(m >> 14); }
constexpr PieceType promoType(Move m) { return PieceType(((m >> 12) & 3) + KNIGHT); }

constexpr int SEEValue[7] = { 100, 320, 330, 500, 950, 30000, 0 };

constexpr int pawnPush(Color c) { return c == WHITE ? 8 : -8; }

enum { WHITE_OO = 1, WHITE_OOO = 2, BLACK_OO = 4, BLACK_OOO = 8 };

struct Undo {
    Key      key;
    int      castlingRights;
    int      epSquare;
    int      rule50;
    int      pliesFromNull;
    Piece    captured;
    Bitboard checkers;
    Bitboard blockers[COLOR_NB];
};

constexpr int MAX_GAME_PLY = 2048;

class Position {
public:
    void set(const std::string& fen);
    std::string fen() const;

    Bitboard pieces() const                          { return colbb[WHITE] | colbb[BLACK]; }
    Bitboard pieces(Color c) const                   { return colbb[c]; }
    Bitboard pieces(PieceType pt) const              { return pcbb[pt]; }
    Bitboard pieces(Color c, PieceType pt) const     { return colbb[c] & pcbb[pt]; }
    Bitboard pieces(Color c, PieceType a, PieceType b) const { return colbb[c] & (pcbb[a] | pcbb[b]); }
    Piece    pieceOn(int s) const                    { return mbox[s]; }
    int      kingSq(Color c) const                   { return lsb(pieces(c, KING)); }
    Color    sideToMove() const                      { return stm; }
    Key      key() const                             { return st[sp].key; }
    int      epSquare() const                        { return st[sp].epSquare; }
    int      castlingRights() const                  { return st[sp].castlingRights; }
    int      rule50() const                          { return st[sp].rule50; }
    Bitboard checkers() const                        { return st[sp].checkers; }
    Bitboard blkFor(Color c) const          { return st[sp].blockers[c]; }
    Piece    captured() const                        { return st[sp].captured; }
    int      gamePlies() const                       { return gpcount; }

    bool isCapture(Move m) const { return mbox[toSq(m)] != NO_PIECE || moveType(m) == ENPASSANT; }
    bool isQuiet(Move m) const   { return !isCapture(m) && moveType(m) != PROMOTION; }

    int npMat(Color c) const {
        return 320 * popcount(pieces(c, KNIGHT)) + 330 * popcount(pieces(c, BISHOP))
             + 500 * popcount(pieces(c, ROOK))   + 950 * popcount(pieces(c, QUEEN));
    }

    Bitboard atkTo(int s, Bitboard occ) const {
        return (PATK[BLACK][s] & pieces(WHITE, PAWN))
             | (PATK[WHITE][s] & pieces(BLACK, PAWN))
             | (NATK[s]      & pcbb[KNIGHT])
             | (KATK[s]        & pcbb[KING])
             | (rookAttacks(s, occ)   & (pcbb[ROOK]   | pcbb[QUEEN]))
             | (bishopAttacks(s, occ) & (pcbb[BISHOP] | pcbb[QUEEN]));
    }
    bool isHit(int s, Color c, Bitboard occ) const {
        return (PATK[~c][s] & pieces(c, PAWN))
            || (NATK[s]   & pieces(c, KNIGHT))
            || (KATK[s]     & pieces(c, KING))
            || (rookAttacks(s, occ)   & pieces(c, ROOK, QUEEN))
            || (bishopAttacks(s, occ) & pieces(c, BISHOP, QUEEN));
    }

    void makeMove(Move m);
    void unmakeMove(Move m);
    void makeNullMove();
    void unmakeNullMove();

    bool givesChk(Move m) const;
    bool see(Move m, int threshold) const;
    bool isDraw() const;

private:
    void putPiece(Piece p, int s) {
        mbox[s] = p;
        pcbb[typeOf(p)] |= sqBB(s);
        colbb[colorOf(p)] |= sqBB(s);
    }
    void removePiece(int s) {
        Piece p = mbox[s];
        pcbb[typeOf(p)] &= ~sqBB(s);
        colbb[colorOf(p)] &= ~sqBB(s);
        mbox[s] = NO_PIECE;
    }
    void movePiece(int from, int to) {
        Piece p = mbox[from];
        Bitboard ft = sqBB(from) | sqBB(to);
        pcbb[typeOf(p)] ^= ft;
        colbb[colorOf(p)] ^= ft;
        mbox[from] = NO_PIECE;
        mbox[to] = p;
    }
    void refreshChk();
    Key mkKey() const;

    Bitboard  pcbb[PIECETYPE_NB];
    Bitboard  colbb[COLOR_NB];
    Piece     mbox[64];
    Color     stm;
    int       gpcount;
    int       sp;
    Undo st[MAX_GAME_PLY];
    int       crmask[64];
};

#ifdef ENGINE_TU

void Position::refreshChk() {
    Undo& s = st[sp];
    s.checkers = atkTo(kingSq(stm), pieces()) & pieces(~stm);
    for (Color c : {WHITE, BLACK}) {
        s.blockers[c] = 0;
        int ksq = kingSq(c);
        Bitboard snipers = ((rookAttacks(ksq, 0)   & pieces(~c, ROOK, QUEEN))
                          | (bishopAttacks(ksq, 0) & pieces(~c, BISHOP, QUEEN)));
        Bitboard occupancy = pieces() ^ snipers;
        while (snipers) {
            int sn = popLsb(snipers);
            Bitboard b = BTW[ksq][sn] & occupancy;
            if (b && !moreThanOne(b))
                s.blockers[c] |= b;
        }
    }
}

Key Position::mkKey() const {
    Key k = 0;
    for (int s = 0; s < 64; s++)
        if (mbox[s] != NO_PIECE) k ^= Zobrist::psq[mbox[s]][s];
    k ^= Zobrist::castling[st[sp].castlingRights];
    if (st[sp].epSquare != SQ_NONE
        && (PATK[~stm][st[sp].epSquare] & pieces(stm, PAWN)))
        k ^= Zobrist::enpassant[fileOf(st[sp].epSquare)];
    if (stm == BLACK) k ^= Zobrist::side;
    return k;
}

void Position::set(const std::string& fen) {
    memset(pcbb, 0, sizeof(pcbb));
    memset(colbb, 0, sizeof(colbb));
    for (int s = 0; s < 64; s++) mbox[s] = NO_PIECE;
    sp = 0;
    gpcount = 0;
    Undo& s0 = st[0];
    s0 = Undo{};
    s0.epSquare = SQ_NONE;
    s0.captured = NO_PIECE;

    std::istringstream ss(fen);
    std::string token;
    ss >> token;
    int sq = SQ_A8;
    const std::string pieceChars = "PNBRQKpnbrqk";
    for (char ch : token) {
        if (ch == '/') sq -= 16;
        else if (isdigit(ch)) sq += ch - '0';
        else {
            size_t idx = pieceChars.find(ch);
            putPiece(Piece(idx), sq);
            sq++;
        }
    }

    ss >> token;
    stm = (token == "w") ? WHITE : BLACK;

    ss >> token;
    s0.castlingRights = 0;
    for (char ch : token) {
        if (ch == 'K') s0.castlingRights |= WHITE_OO;
        else if (ch == 'Q') s0.castlingRights |= WHITE_OOO;
        else if (ch == 'k') s0.castlingRights |= BLACK_OO;
        else if (ch == 'q') s0.castlingRights |= BLACK_OOO;
    }

    ss >> token;
    s0.epSquare = (token != "-" && token.size() == 2)
                ? (token[0] - 'a') + 8 * (token[1] - '1') : (int)SQ_NONE;

    s0.rule50 = 0;
    int fullmove = 1;
    if (ss >> s0.rule50) ss >> fullmove;
    gpcount = std::max(0, 2 * (fullmove - 1) + (stm == BLACK));

    for (int i = 0; i < 64; i++) crmask[i] = 15;
    crmask[SQ_E1] &= ~(WHITE_OO | WHITE_OOO);
    crmask[SQ_H1] &= ~WHITE_OO;
    crmask[SQ_A1] &= ~WHITE_OOO;
    crmask[SQ_E8] &= ~(BLACK_OO | BLACK_OOO);
    crmask[SQ_H8] &= ~BLACK_OO;
    crmask[SQ_A8] &= ~BLACK_OOO;

    s0.key = mkKey();
    refreshChk();
}

std::string Position::fen() const {
    std::ostringstream ss;
    const std::string pieceChars = "PNBRQKpnbrqk";
    for (int r = 7; r >= 0; r--) {
        int empty = 0;
        for (int f = 0; f < 8; f++) {
            Piece p = mbox[r * 8 + f];
            if (p == NO_PIECE) empty++;
            else {
                if (empty) { ss << empty; empty = 0; }
                ss << pieceChars[p];
            }
        }
        if (empty) ss << empty;
        if (r) ss << '/';
    }
    ss << (stm == WHITE ? " w " : " b ");
    int cr = st[sp].castlingRights;
    if (!cr) ss << '-';
    else {
        if (cr & WHITE_OO)  ss << 'K';
        if (cr & WHITE_OOO) ss << 'Q';
        if (cr & BLACK_OO)  ss << 'k';
        if (cr & BLACK_OOO) ss << 'q';
    }
    if (st[sp].epSquare != SQ_NONE)
        ss << ' ' << char('a' + fileOf(st[sp].epSquare)) << char('1' + rankOf(st[sp].epSquare));
    else ss << " -";
    ss << ' ' << st[sp].rule50 << ' ' << 1 + (gpcount - (stm == BLACK)) / 2;
    return ss.str();
}

void Position::makeMove(Move m) {
    const Undo& prev = st[sp];
    Undo& s = st[++sp];
    s.castlingRights = prev.castlingRights;
    s.rule50         = prev.rule50 + 1;
    s.pliesFromNull  = prev.pliesFromNull + 1;
    s.epSquare       = SQ_NONE;
    s.captured       = NO_PIECE;
    Key k = prev.key ^ Zobrist::side;
    if (prev.epSquare != SQ_NONE
        && (PATK[~stm][prev.epSquare] & pieces(stm, PAWN)))
        k ^= Zobrist::enpassant[fileOf(prev.epSquare)];

    Color us = stm, them = ~us;
    int from = fromSq(m), to = toSq(m);
    MoveType mt = moveType(m);
    Piece pc = mbox[from];

    if (mt == CASTLING) {
        bool kingside = to > from;
        int rfrom = kingside ? from + 3 : from - 4;
        int rto   = kingside ? from + 1 : from - 1;
        Piece rook = mbox[rfrom];
        movePiece(from, to);
        movePiece(rfrom, rto);
        k ^= Zobrist::psq[pc][from] ^ Zobrist::psq[pc][to]
           ^ Zobrist::psq[rook][rfrom] ^ Zobrist::psq[rook][rto];
    } else {
        int capsq = (mt == ENPASSANT) ? to - pawnPush(us) : to;
        if (mbox[capsq] != NO_PIECE) {
            s.captured = mbox[capsq];
            k ^= Zobrist::psq[s.captured][capsq];
            removePiece(capsq);
            s.rule50 = 0;
        }
        movePiece(from, to);
        k ^= Zobrist::psq[pc][from] ^ Zobrist::psq[pc][to];

        if (typeOf(pc) == PAWN) {
            s.rule50 = 0;
            if ((to ^ from) == 16) {
                int ep = (from + to) / 2;
                s.epSquare = ep;
                if (PATK[us][ep] & pieces(them, PAWN))
                    k ^= Zobrist::enpassant[fileOf(ep)];
            } else if (mt == PROMOTION) {
                Piece promo = makePiece(us, promoType(m));
                removePiece(to);
                putPiece(promo, to);
                k ^= Zobrist::psq[pc][to] ^ Zobrist::psq[promo][to];
            }
        }
    }

    int newCr = s.castlingRights & crmask[from] & crmask[to];
    if (newCr != s.castlingRights) {
        k ^= Zobrist::castling[s.castlingRights] ^ Zobrist::castling[newCr];
        s.castlingRights = newCr;
    }

    s.key = k;
    stm = them;
    gpcount++;
    refreshChk();
}

void Position::unmakeMove(Move m) {
    Color us = ~stm;
    int from = fromSq(m), to = toSq(m);
    MoveType mt = moveType(m);

    if (mt == CASTLING) {
        bool kingside = to > from;
        int rfrom = kingside ? from + 3 : from - 4;
        int rto   = kingside ? from + 1 : from - 1;
        movePiece(to, from);
        movePiece(rto, rfrom);
    } else {
        if (mt == PROMOTION) {
            removePiece(to);
            putPiece(makePiece(us, PAWN), from);
        } else {
            movePiece(to, from);
        }
        if (st[sp].captured != NO_PIECE) {
            int capsq = (mt == ENPASSANT) ? to - pawnPush(us) : to;
            putPiece(st[sp].captured, capsq);
        }
    }
    sp--;
    stm = us;
    gpcount--;
}

void Position::makeNullMove() {
    const Undo& prev = st[sp];
    Undo& s = st[++sp];
    s = prev;
    s.key ^= Zobrist::side;
    if (prev.epSquare != SQ_NONE
        && (PATK[~stm][prev.epSquare] & pieces(stm, PAWN)))
        s.key ^= Zobrist::enpassant[fileOf(prev.epSquare)];
    s.epSquare = SQ_NONE;
    s.rule50++;
    s.pliesFromNull = 0;
    s.captured = NO_PIECE;
    stm = ~stm;
    gpcount++;
    refreshChk();
}

void Position::unmakeNullMove() {
    sp--;
    stm = ~stm;
    gpcount--;
}

bool Position::givesChk(Move m) const {
    Color us = stm, them = ~us;
    int from = fromSq(m), to = toSq(m);
    int ksq = kingSq(them);
    Bitboard kbb = sqBB(ksq);
    Bitboard occ = pieces();
    MoveType mt = moveType(m);
    PieceType pt = typeOf(mbox[from]);

    if (mt == CASTLING) {
        bool kingside = to > from;
        int rfrom = kingside ? from + 3 : from - 4;
        int rto   = kingside ? from + 1 : from - 1;
        Bitboard occAfter = (occ ^ sqBB(from) ^ sqBB(rfrom)) | sqBB(to) | sqBB(rto);
        return rookAttacks(rto, occAfter) & kbb;
    }
    if (mt == ENPASSANT) {
        int capsq = to - pawnPush(us);
        Bitboard occAfter = (occ ^ sqBB(from) ^ sqBB(capsq)) | sqBB(to);
        return (PATK[us][to] & kbb)
            || (rookAttacks(ksq, occAfter)   & pieces(us, ROOK, QUEEN))
            || (bishopAttacks(ksq, occAfter) & pieces(us, BISHOP, QUEEN));
    }

    Bitboard occAfter = (occ ^ sqBB(from)) | sqBB(to);
    PieceType movedAs = (mt == PROMOTION) ? promoType(m) : pt;
    if (movedAs == PAWN) {
        if (PATK[us][to] & kbb) return true;
    } else if (attacksBB(movedAs, to, occAfter) & kbb)
        return true;

    if ((blkFor(them) & sqBB(from)) && !aligned(from, to, ksq))
        return true;
    return false;
}

bool Position::see(Move m, int threshold) const {
    if (moveType(m) != NORMAL)
        return 0 >= threshold;
    int from = fromSq(m), to = toSq(m);

    int swap = (mbox[to] == NO_PIECE ? 0 : SEEValue[typeOf(mbox[to])]) - threshold;
    if (swap < 0) return false;
    swap = SEEValue[typeOf(mbox[from])] - swap;
    if (swap <= 0) return true;

    Bitboard occ = pieces() ^ sqBB(from) ^ sqBB(to);
    Color c = stm;
    Bitboard attackers = atkTo(to, occ);
    int res = 1;

    for (;;) {
        c = ~c;
        attackers &= occ;
        Bitboard stmAtt = attackers & pieces(c);
        if (!stmAtt) break;
        res ^= 1;
        Bitboard bb;
        if ((bb = stmAtt & pieces(PAWN))) {
            if ((swap = SEEValue[PAWN] - swap) < res) break;
            occ ^= sqBB(lsb(bb));
            attackers |= bishopAttacks(to, occ) & (pcbb[BISHOP] | pcbb[QUEEN]);
        } else if ((bb = stmAtt & pieces(KNIGHT))) {
            if ((swap = SEEValue[KNIGHT] - swap) < res) break;
            occ ^= sqBB(lsb(bb));
        } else if ((bb = stmAtt & pieces(BISHOP))) {
            if ((swap = SEEValue[BISHOP] - swap) < res) break;
            occ ^= sqBB(lsb(bb));
            attackers |= bishopAttacks(to, occ) & (pcbb[BISHOP] | pcbb[QUEEN]);
        } else if ((bb = stmAtt & pieces(ROOK))) {
            if ((swap = SEEValue[ROOK] - swap) < res) break;
            occ ^= sqBB(lsb(bb));
            attackers |= rookAttacks(to, occ) & (pcbb[ROOK] | pcbb[QUEEN]);
        } else if ((bb = stmAtt & pieces(QUEEN))) {
            if ((swap = SEEValue[QUEEN] - swap) < res) break;
            occ ^= sqBB(lsb(bb));
            attackers |= (bishopAttacks(to, occ) & (pcbb[BISHOP] | pcbb[QUEEN]))
                       | (rookAttacks(to, occ)   & (pcbb[ROOK]   | pcbb[QUEEN]));
        } else
            return (attackers & ~pieces(c)) ? bool(res ^ 1) : bool(res);
    }
    return bool(res);
}

bool Position::isDraw() const {
    const Undo& s = st[sp];
    if (s.rule50 >= 100) return true;

    int end = std::min(s.rule50, s.pliesFromNull);
    for (int i = 4; i <= end; i += 2)
        if (st[sp - i].key == s.key)
            return true;

    if (!pcbb[PAWN] && !pcbb[ROOK] && !pcbb[QUEEN]) {
        int minors = popcount(pcbb[KNIGHT] | pcbb[BISHOP]);
        if (minors <= 1) return true;
    }
    return false;
}

#endif
