#ifndef MOVE_H
#define MOVE_H

#include "Piece.h"

#define SQUARE_CACHE      0x3F      // 6 bits (0b111111)
#define PIECE_CACHE       0x1F      // 5 bits (0b11111)
#define CASTLING_CACHE    0x0F      // 4 bits (0b1111)
#define EN_PASSANT_CACHE  0x3F      // 6 bits (0b111111)
#define MOVECOUNT_CACHE   0x7F      // 7 bits (0b1111111)

#define SMALL_MOVE_CACHE 0x0FFF // 12bits (0b111111111111)
#define WITHOUT_MVSCORE_CACHE (((uint64_t) 1 << 41 ) - 1)
#define KILLERMOVE1_BONUS ((uint64_t) 110 << 41)
#define KILLERMOVE2_BONUS ((uint64_t) 100 << 41)

#define UNDERPROMOTION_BONUS 64
#define PROMOTION_BONUS 128

static inline uint64_t make(int prev, int next, int promotion, int captured, int state, int isEnPassant,uint64_t mvScore){
    uint64_t m = (  (uint64_t)prev |
                    ((uint64_t)next << 6) |
                    ((uint64_t)captured << 12) |
                    ((uint64_t)promotion << 17) |
                    ((uint64_t)state << 22) |
                    ((uint64_t)isEnPassant << 40) |
                               mvScore);
    return m;
}

static inline uint64_t makeMoveWithoutPromotion(int prev, int next, int captured, int state, int isEnPassant,uint64_t mvScore) {
    return make(prev, next, Vacant, captured, state, isEnPassant,mvScore);
}

static inline uint64_t makeOrdinaryMove(int prev, int next, int captured, int state,uint64_t mvScore) {
    return make(prev, next, Vacant, captured, state, 0, mvScore);
}

static inline int prev(uint64_t m) {
    return (int)(m & SQUARE_CACHE);
}

static inline int next(uint64_t m) {
    return (int)((m >> 6) & SQUARE_CACHE);
}

static inline int captured(uint64_t m) {
    return (int)((m >> 12) & PIECE_CACHE);
}

static inline int promotion(uint64_t m) {
    return (int)((m >> 17) & PIECE_CACHE);
}

static inline int castlingRights(uint64_t m) {
    return (int)((m >> 22) & CASTLING_CACHE);
}

static inline bool KCas(uint64_t m) { return ((m >> 22) & 1) == 1; }
static inline bool QCas(uint64_t m) { return ((m >> 23) & 1) == 1; }
static inline bool kCas(uint64_t m) { return ((m >> 24) & 1) == 1; }
static inline bool qCas(uint64_t m) { return ((m >> 25) & 1) == 1; }

static inline int enPassantValue(uint64_t m) {
    if (((m >> 32) & 1) == 1) return -1;
    else return (int)((m >> 26) & EN_PASSANT_CACHE);
}

static inline int hCount(uint64_t m) {
    return (int)((m >> 33) & MOVECOUNT_CACHE);
}

static inline bool isEnPassant(uint64_t m) {
    return ((m >> 40) & 1) == 1;
}

static inline bool isNullMove(uint64_t m) {
    return (m == 0);
}

static inline uint16_t toSmallMove(uint64_t m){
    return (uint16_t)(
        (m & SMALL_MOVE_CACHE) |
        (((m >> 17) & 0x07) << 12)
    );
}

#endif // MOVE_H
