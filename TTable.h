#ifndef TTABLE_H
#define TTABLE_H

#include "Piece.h"
#include "Move.h"
#include "Data.h"
#include "Types.h"

static inline int scoreToTT(int score, int ply) {
    if (score > MATE_THRESHOLD) return score + ply;
    if (score < -MATE_THRESHOLD) return score - ply;
    return score;
}

static inline int scoreFromTT(int score, int ply) {
    if (score > MATE_THRESHOLD) return score - ply;
    if (score < -MATE_THRESHOLD) return score + ply;
    return score;
}

static inline void clearTableSize(TTEntry* t, uint64_t size) {
    memset(t, 0, size * sizeof(TTEntry));
}

static inline void addEntry(TTEntry* t, uint64_t size, uint64_t key, int evaluation, uint16_t bestResponse, uint8_t depth, uint8_t flag){
    TTEntry* slot = &t[key & (size-1)];
    if (slot->key == key) {
        if (slot->depth > depth) return;
    } else {
        if (slot->key != 0 && slot->depth > depth) return;
    }
    slot->key = key;
    slot->eval = evaluation;
    slot->bestResponse = bestResponse;
    slot->depth = depth;
    slot->flag = flag;
}

static inline TTEntry getEntry(TTEntry* t, uint64_t size, uint64_t key){
    return t[key & (size-1)];
}

#endif