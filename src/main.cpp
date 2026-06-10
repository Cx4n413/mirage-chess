#define ENGINE_TU
#include "bitboard.h"
#include "position.h"
#include "movegen.h"
#include "eval.h"
#include "tt.h"
#include "search.h"
#include <iostream>
#include <thread>

namespace {

const char* StartFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

Position rootPos;
std::thread searchThread;

Move uciToMove(const Position& pos, const std::string& s) {
    MList ml;
    genml(pos, ml, false);
    for (int i = 0; i < ml.size; i++)
        if (mv2str(ml.list[i].move) == s)
            return ml.list[i].move;
    return MOVE_NONE;
}

void stopSearchThread() {
    eng.stopFlag = true;
    if (searchThread.joinable()) searchThread.join();
}

void cmdPosition(std::istringstream& ss) {
    std::string token, fen;
    ss >> token;
    if (token == "startpos") {
        fen = StartFEN;
        ss >> token;
    } else if (token == "fen") {
        while (ss >> token && token != "moves")
            fen += token + " ";
    }
    rootPos.set(fen);
    while (ss >> token) {
        Move m = uciToMove(rootPos, token);
        if (m == MOVE_NONE) break;
        rootPos.makeMove(m);
    }
}

void cmdGo(std::istringstream& ss) {
    Limits limits;
    std::string token;
    int64_t v;
    int perftDepth = 0;
    while (ss >> token) {
        if      (token == "depth"     && ss >> v) limits.depth = int(v);
        else if (token == "movetime"  && ss >> v) limits.movetime = v;
        else if (token == "wtime"     && ss >> v) limits.time[WHITE] = std::max<int64_t>(v, 1);
        else if (token == "btime"     && ss >> v) limits.time[BLACK] = std::max<int64_t>(v, 1);
        else if (token == "winc"      && ss >> v) limits.inc[WHITE] = v;
        else if (token == "binc"      && ss >> v) limits.inc[BLACK] = v;
        else if (token == "movestogo" && ss >> v) limits.movestogo = int(v);
        else if (token == "nodes"     && ss >> v) limits.nodes = uint64_t(v);
        else if (token == "infinite")             limits.infinite = true;
        else if (token == "perft"     && ss >> v) perftDepth = int(v);
    }
    if (perftDepth) {
        auto start = std::chrono::steady_clock::now();
        uint64_t n = eng.perft(rootPos, perftDepth, true);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now() - start).count();
        printf("\nNodes: %llu  Time: %lld ms  NPS: %llu\n",
               (unsigned long long)n, (long long)ms,
               (unsigned long long)(n * 1000 / std::max<int64_t>(ms, 1)));
        fflush(stdout);
        return;
    }
    stopSearchThread();
    eng.stopFlag = false;
    searchThread = std::thread([limits] { eng.runSearch(rootPos, limits); });
}

void cmdBench() {
    const char* fens[] = {
        StartFEN,
        "r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
        "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
        "r2q1rk1/ppp2ppp/3bbn2/3p4/8/1P1BP1N1/PBPN1PPP/R2Q1RK1 b - - 4 10",
        "4k3/8/8/8/8/8/4P3/4K3 w - - 0 1",
        "8/8/1p1k4/p4p2/P2K1P2/1P6/8/8 w - - 0 1",
        "6k1/5ppp/8/8/8/8/5PPP/3R2K1 w - - 0 1",
    };
    uint64_t total = 0;
    auto start = std::chrono::steady_clock::now();
    for (const char* f : fens) {
        Position p;
        p.set(f);
        Limits limits;
        limits.depth = 13;
        eng.runSearch(p, limits);
        total += eng.nodes;
    }
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now() - start).count();
    printf("bench: %llu nodes  %lld ms  %llu nps\n",
           (unsigned long long)total, (long long)ms,
           (unsigned long long)(total * 1000 / std::max<int64_t>(ms, 1)));
    fflush(stdout);
}

void printBoard() {
    const std::string pc = "PNBRQKpnbrqk";
    for (int r = 7; r >= 0; r--) {
        printf("%d  ", r + 1);
        for (int f = 0; f < 8; f++) {
            Piece p = rootPos.pieceOn(r * 8 + f);
            printf("%c ", p == NO_PIECE ? '.' : pc[p]);
        }
        printf("\n");
    }
    printf("   a b c d e f g h\n%s\nkey: %llx  eval: %d\n",
           rootPos.fen().c_str(), (unsigned long long)rootPos.key(), evscore(rootPos));
    fflush(stdout);
}

}

int main(int argc, char* argv[]) {
    std::ios_base::sync_with_stdio(false);
    Bitboards::init();
    TT.resize(64);
    rootPos.set(StartFEN);

    if (argc > 1 && std::string(argv[1]) == "bench") { cmdBench(); return 0; }

    std::string line, token;
    while (std::getline(std::cin, line)) {
        if (line.size() >= 3 && (unsigned char)line[0] == 0xEF
            && (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF)
            line.erase(0, 3);
        std::istringstream ss(line);
        token.clear();
        ss >> token;
        if (token == "uci") {
            printf("id name Mirage 1.0\nid author bigback\n");
            printf("option name Hash type spin default 64 min 1 max 16384\n");
            printf("option name Threads type spin default 1 min 1 max 1\n");
            printf("uciok\n");
            fflush(stdout);
        }
        else if (token == "isready")    { printf("readyok\n"); fflush(stdout); }
        else if (token == "ucinewgame") { stopSearchThread(); TT.clear(); eng.wipeHist(); }
        else if (token == "setoption") {
            std::string name, value;
            ss >> token;
            ss >> name;
            ss >> token;
            ss >> value;
            if (name == "Hash") { stopSearchThread(); TT.resize(std::stoul(value)); }
        }
        else if (token == "position") { stopSearchThread(); cmdPosition(ss); }
        else if (token == "go")       cmdGo(ss);
        else if (token == "stop")     stopSearchThread();
        else if (token == "bench")    { stopSearchThread(); cmdBench(); }
        else if (token == "d")        printBoard();
        else if (token == "eval")     { printf("eval: %d cp (stm)\n", evscore(rootPos)); fflush(stdout); }
        else if (token == "quit")     break;
    }
    stopSearchThread();
    return 0;
}
