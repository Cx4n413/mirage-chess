#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <cassert>

using Bitboard = uint64_t;
using Key      = uint64_t;

enum Color : int { WHITE, BLACK, COLOR_NB };
enum PieceType : int { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, PIECETYPE_NB };
enum Piece : int {
    W_PAWN, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING,
    B_PAWN, B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING,
    NO_PIECE
};

constexpr Color operator~(Color c) { return Color(c ^ 1); }
constexpr Piece makePiece(Color c, PieceType pt) { return Piece(c * 6 + pt); }
constexpr Color colorOf(Piece p) { return Color(p / 6); }
constexpr PieceType typeOf(Piece p) { return PieceType(p % 6); }

enum Square : int {
    SQ_A1, SQ_B1, SQ_C1, SQ_D1, SQ_E1, SQ_F1, SQ_G1, SQ_H1,
    SQ_A2 = 8,  SQ_E2 = 12, SQ_H2 = 15,
    SQ_A8 = 56, SQ_C8 = 58, SQ_D8 = 59, SQ_E8 = 60, SQ_F8 = 61, SQ_G8 = 62, SQ_H8 = 63,
    SQ_C1_ = 2, SQ_G1_ = 6,
    SQ_NONE = 64
};

constexpr int fileOf(int s) { return s & 7; }
constexpr int rankOf(int s) { return s >> 3; }
constexpr int relRank(Color c, int s) { int r = rankOf(s); return c == WHITE ? r : 7 - r; }
constexpr int flipSq(int s) { return s ^ 56; }

constexpr Bitboard FileABB = 0x0101010101010101ULL;
constexpr Bitboard FileHBB = FileABB << 7;
constexpr Bitboard Rank1BB = 0xFFULL;
constexpr Bitboard fileBB(int f) { return FileABB << f; }
constexpr Bitboard rankBB(int r) { return Rank1BB << (8 * r); }
constexpr Bitboard sqBB(int s)   { return 1ULL << s; }

inline int popcount(Bitboard b) { return __builtin_popcountll(b); }
inline int lsb(Bitboard b)      { assert(b); return __builtin_ctzll(b); }
inline int msb(Bitboard b)      { assert(b); return 63 - __builtin_clzll(b); }
inline int popLsb(Bitboard& b)  { int s = lsb(b); b &= b - 1; return s; }
inline bool moreThanOne(Bitboard b) { return b & (b - 1); }

constexpr Bitboard shiftN(Bitboard b)  { return b << 8; }
constexpr Bitboard shiftS(Bitboard b)  { return b >> 8; }
constexpr Bitboard shiftNE(Bitboard b) { return (b & ~FileHBB) << 9; }
constexpr Bitboard shiftNW(Bitboard b) { return (b & ~FileABB) << 7; }
constexpr Bitboard shiftSE(Bitboard b) { return (b & ~FileHBB) >> 7; }
constexpr Bitboard shiftSW(Bitboard b) { return (b & ~FileABB) >> 9; }

struct PRNG {
    uint64_t s;
    explicit PRNG(uint64_t seed) : s(seed) {}
    uint64_t rand64() {
        s ^= s >> 12; s ^= s << 25; s ^= s >> 27;
        return s * 2685821657736338717ULL;
    }
    uint64_t sparse() { return rand64() & rand64() & rand64(); }
};

extern Bitboard PATK[COLOR_NB][64];
extern Bitboard NATK[64];
extern Bitboard KATK[64];
extern Bitboard LN[64][64];
extern Bitboard BTW[64][64];

struct Magic {
    Bitboard  mask;
    Bitboard  magic;
    Bitboard* attacks;
    unsigned  shift;
    unsigned index(Bitboard occ) const { return unsigned(((occ & mask) * magic) >> shift); }
};
extern Magic RMG[64];
extern Magic BMG[64];

inline Bitboard rookAttacks(int s, Bitboard occ)   { const Magic& m = RMG[s];   return m.attacks[m.index(occ)]; }
inline Bitboard bishopAttacks(int s, Bitboard occ) { const Magic& m = BMG[s]; return m.attacks[m.index(occ)]; }
inline Bitboard queenAttacks(int s, Bitboard occ)  { return rookAttacks(s, occ) | bishopAttacks(s, occ); }

inline Bitboard attacksBB(PieceType pt, int s, Bitboard occ) {
    switch (pt) {
        case KNIGHT: return NATK[s];
        case BISHOP: return bishopAttacks(s, occ);
        case ROOK:   return rookAttacks(s, occ);
        case QUEEN:  return queenAttacks(s, occ);
        case KING:   return KATK[s];
        default:     return 0;
    }
}

inline bool aligned(int a, int b, int c) { return LN[a][b] & sqBB(c); }

namespace Zobrist {
    extern Key psq[12][64];
    extern Key enpassant[8];
    extern Key castling[16];
    extern Key side;
}

namespace Bitboards { void init(); }

#ifdef ENGINE_TU

Bitboard PATK[COLOR_NB][64];
Bitboard NATK[64];
Bitboard KATK[64];
Bitboard LN[64][64];
Bitboard BTW[64][64];
Magic RMG[64];
Magic BMG[64];

namespace Zobrist {
    Key psq[12][64];
    Key enpassant[8];
    Key castling[16];
    Key side;
}

namespace {

Bitboard RookTable[0x19000];
Bitboard BishopTable[0x1480];

Bitboard slowRay(PieceType pt, int sq, Bitboard occ) {
    Bitboard attacks = 0;
    const int rookDirs[4][2]   = {{1,0},{-1,0},{0,1},{0,-1}};
    const int bishopDirs[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};
    const auto& dirs = (pt == ROOK) ? rookDirs : bishopDirs;
    for (int d = 0; d < 4; d++) {
        int f = fileOf(sq) + dirs[d][0], r = rankOf(sq) + dirs[d][1];
        while (f >= 0 && f <= 7 && r >= 0 && r <= 7) {
            int s = r * 8 + f;
            attacks |= sqBB(s);
            if (occ & sqBB(s)) break;
            f += dirs[d][0]; r += dirs[d][1];
        }
    }
    return attacks;
}

void buildMg(PieceType pt, Magic magics[], Bitboard table[]) {
    PRNG rng(0x1234567887654321ULL);
    Bitboard occupancy[4096], reference[4096];
    int size = 0;
    for (int s = 0; s < 64; s++) {

        Bitboard edges = ((Rank1BB | rankBB(7)) & ~rankBB(rankOf(s)))
                       | ((FileABB | FileHBB)   & ~fileBB(fileOf(s)));
        Magic& m = magics[s];
        m.mask  = slowRay(pt, s, 0) & ~edges;
        m.shift = 64 - popcount(m.mask);
        m.attacks = (s == 0) ? table : magics[s - 1].attacks + size;

        size = 0;
        Bitboard b = 0;
        do {
            occupancy[size] = b;
            reference[size] = slowRay(pt, s, b);
            size++;
            b = (b - m.mask) & m.mask;
        } while (b);

        for (;;) {
            do { m.magic = rng.sparse(); }
            while (popcount((m.magic * m.mask) >> 56) < 6);
            bool ok = true;
            static int epoch[4096]; static int epochCounter = 0;
            epochCounter++;
            static Bitboard tmp[4096];
            for (int i = 0; i < size && ok; i++) {
                unsigned idx = m.index(occupancy[i]);
                if (epoch[idx] != epochCounter) { epoch[idx] = epochCounter; tmp[idx] = reference[i]; }
                else if (tmp[idx] != reference[i]) ok = false;
            }
            if (ok) {
                for (int i = 0; i < size; i++) m.attacks[m.index(occupancy[i])] = reference[i];
                break;
            }
        }
    }
}

}

void Bitboards::init() {
    for (int s = 0; s < 64; s++) {
        Bitboard b = sqBB(s);
        PATK[WHITE][s] = shiftNE(b) | shiftNW(b);
        PATK[BLACK][s] = shiftSE(b) | shiftSW(b);

        Bitboard k = 0;
        for (int df : {-2,-1,1,2}) for (int dr : {-2,-1,1,2})
            if (df*df + dr*dr == 5) {
                int f = fileOf(s)+df, r = rankOf(s)+dr;
                if (f >= 0 && f <= 7 && r >= 0 && r <= 7) k |= sqBB(r*8+f);
            }
        NATK[s] = k;

        k = 0;
        for (int df = -1; df <= 1; df++) for (int dr = -1; dr <= 1; dr++) {
            if (!df && !dr) continue;
            int f = fileOf(s)+df, r = rankOf(s)+dr;
            if (f >= 0 && f <= 7 && r >= 0 && r <= 7) k |= sqBB(r*8+f);
        }
        KATK[s] = k;
    }

    buildMg(ROOK, RMG, RookTable);
    buildMg(BISHOP, BMG, BishopTable);

    for (int a = 0; a < 64; a++)
        for (int b = 0; b < 64; b++) {
            LN[a][b] = BTW[a][b] = 0;
            if (a == b) continue;
            for (PieceType pt : {BISHOP, ROOK})
                if (attacksBB(pt, a, 0) & sqBB(b)) {
                    LN[a][b]    = (attacksBB(pt, a, 0) & attacksBB(pt, b, 0)) | sqBB(a) | sqBB(b);
                    BTW[a][b] = attacksBB(pt, a, sqBB(b)) & attacksBB(pt, b, sqBB(a));
                }
        }

    PRNG rng(0x9E3779B97F4A7C15ULL);
    for (int p = 0; p < 12; p++) for (int s = 0; s < 64; s++) Zobrist::psq[p][s] = rng.rand64();
    for (int f = 0; f < 8; f++)  Zobrist::enpassant[f] = rng.rand64();
    for (int i = 0; i < 16; i++) Zobrist::castling[i]  = rng.rand64();
    Zobrist::side = rng.rand64();
}

#endif
