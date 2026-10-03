#ifndef TYPES_H
#define TYPES_H

#include <assert.h>
#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include "Piece.h"
#include "Move.h"
#include "Data.h"

#define MAX_DEPTH 64
#define BONUS_CASTLE 40
#define BONUS_CASTLING_RIGHT 15
#define WHITE_INDEX 0
#define BLACK_INDEX 1
#define FIRST_ROW 0xFFULL
#define FIRST_COLUMN 0x101010101010101ULL

#define MAX_MOVES 218
#define START_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

#define FLAG_EXACT 0 // Exact score (found between alpha and beta)
#define FLAG_ALPHA 1 // Upper limit (bad node, fail-low)
#define FLAG_BETA  2 // Lower limit (provoked a beta-cutoff, fail-high)
#define TT_SIZE 0x800000
#define TT_SIZE_THREADED 0x200000
#define TT_UNKNOWN_SCORE 200000

#define ROOK_MOVES_SIZE 102400
#define BISHOP_MOVES_SIZE 5248
#define TIME_TO_THINK 100 //100ms

#define INF 300000
#define MATE_THRESHOLD (INF - MAX_DEPTH)

// Cross-platform real-time measurement in milliseconds
#if defined(__EMSCRIPTEN__)
    #include <emscripten/emscripten.h>
    static inline uint64_t get_real_time_ms() {
        return (uint64_t)emscripten_get_now();
    }
#elif defined(_WIN32)
    #include <windows.h>
    static inline uint64_t get_real_time_ms() {
        return GetTickCount64();
    }
#else
    #include <time.h>
    static inline uint64_t get_real_time_ms() {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
    }
#endif

#pragma pack(push, 1)
struct TTEntry {
    uint64_t key;
    int32_t eval;
    uint16_t bestResponse;
    uint8_t depth;
    uint8_t flag;
};
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint64_t key;
    uint16_t move;
    uint16_t weight;
    uint32_t learn;
} PolyglotEntry;
#pragma pack(pop)

typedef struct TTEntry TTEntry;

struct Position {
    // --- MOVE STORAGE ---
    uint64_t moves[256 * MAX_DEPTH];
    uint64_t hashHistory[2048];
    uint64_t pawnKeyHistory[2048];

    // --- BOARD STATE ---
    int board[64];                        // Mailbox (simplicity)
    uint64_t pieces[2][7];               // Bitboards [color][type]
    uint64_t byColor[2];                 // Aggregates per color
    uint64_t occupied;                    // All pieces

    // --- HASH / SEARCH ---
    uint64_t currentHash;
    int historyPly, currentDepth;
    uint64_t pawnKey;

    // --- EVAL ---
    int mgEval, egEval, phase;

    // --- GAME STATE ---
    int halfMove, fullMove, enPassant;
    int KSquare, kSquare;
    bool whiteToMove;
    bool KCastle, QCastle, kCastle, qCastle;

    // --- ENGINE STATE ---
    bool inBook, stopSearch;
    int nodeCount;
    uint64_t timeLimit, startTime;
    uint64_t targetTime, maxTime;
};

typedef struct Position Position;

typedef struct {
    int prevEP;
    uint64_t prevHash;
} NullMoveState;

typedef struct {
    uint64_t key;
    int16_t mgScore;
    int16_t egScore;
    uint8_t passedPawns; // Reserved file mask for passed pawns
} PawnEntry;

#endif
