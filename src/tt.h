#pragma once
#include "position.h"
#include <vector>

constexpr int VALUE_INFINITE = 32001;
constexpr int VALUE_NONE     = 32002;
constexpr int VALUE_MATE     = 32000;
constexpr int MATE_IN_MAX    = VALUE_MATE - 256;

enum Bound : uint8_t { BOUND_NONE = 0, BOUND_UPPER = 1, BOUND_LOWER = 2, BOUND_EXACT = 3 };

inline int v2tt(int v, int ply) {
    return v >= MATE_IN_MAX ? v + ply : v <= -MATE_IN_MAX ? v - ply : v;
}
inline int tt2v(int v, int ply) {
    return v == VALUE_NONE ? VALUE_NONE
         : v >= MATE_IN_MAX ? v - ply : v <= -MATE_IN_MAX ? v + ply : v;
}

struct TEnt {
    Key      key;
    int16_t  score;
    int16_t  eval;
    Move     move;
    uint8_t  depth;
    uint8_t  genBound;

    Bound bound() const { return Bound(genBound & 3); }
};

class HashTbl {
public:
    void resize(size_t mb) {
        count = mb * 1024 * 1024 / sizeof(TEnt);
        table.assign(count, TEnt{});
    }
    void clear() { table.assign(count, TEnt{}); generation = 0; }
    void newSearch() { generation += 4; }

    TEnt* probe(Key key, bool& found) {
        TEnt* tte = &table[index(key)];
        found = (tte->key == key && tte->bound() != BOUND_NONE);
        return tte;
    }

    void save(TEnt* tte, Key key, int score, int eval, Move move, int depth, Bound bound) {
        if (move || tte->key != key)
            tte->move = move;
        if (bound == BOUND_EXACT || tte->key != key
            || depth + 4 > tte->depth
            || (tte->genBound & 0xFC) != generation) {
            tte->key      = key;
            tte->score    = int16_t(score);
            tte->eval     = int16_t(eval);
            tte->depth    = uint8_t(std::max(depth, 0));
            tte->genBound = uint8_t(generation | bound);
        }
    }

    int hashfull() const {
        int cnt = 0;
        for (size_t i = 0; i < 1000 && i < count; i++)
            if (table[i].bound() != BOUND_NONE && (table[i].genBound & 0xFC) == generation)
                cnt++;
        return cnt;
    }

private:
    size_t index(Key key) const {
        return size_t((unsigned __int128)key * (unsigned __int128)count >> 64);
    }
    std::vector<TEnt> table;
    size_t  count = 0;
    uint8_t generation = 0;
};

extern HashTbl TT;

#ifdef ENGINE_TU
HashTbl TT;
#endif
