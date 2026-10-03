#ifndef PIECE_H
#define PIECE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#define Vacant 0x10
#define Pawn   1
#define Knight 2
#define Bishop 3
#define Rook   4
#define Queen  5
#define King   6

#define Black  0x08
#define White  0

#define PieceCache 0x07
#define ColorCache 0x18

#define PAWN_VALUE 100
#define KNIGHT_VALUE 300
#define BISHOP_VALUE 320
#define ROOK_VALUE 500
#define QUEEN_VALUE 900
#define KING_VALUE 20000

static inline bool isVacant(int p) {
    return ((p & ColorCache) == Vacant);
}

static inline bool isWhite(int p) {
    return ((p & ColorCache) == White);
}

static inline bool isBlack(int p) {
    return ((p & ColorCache) == Black);
}

static inline bool isPawn(int p) {
    return ((p & PieceCache) == Pawn);
}

static inline bool isKnight(int p) {
    return ((p & PieceCache) == Knight);
}

static inline bool isBishop(int p) {
    return ((p & PieceCache) == Bishop);
}

static inline bool isRook(int p) {
    return ((p & PieceCache) == Rook);
}

static inline bool isQueen(int p) {
    return ((p & PieceCache) == Queen);
}

static inline bool isKing(int p) {
    return ((p & PieceCache) == King);
}

static inline bool isOpposed(int p, int q) {
    return ((isWhite(p) && isBlack(q)) || (isWhite(q) && isBlack(p)));
}

static inline int color(int p){
    return (p & ColorCache);
}

static const int PIECE_VALUES[8] = {
    0,
    PAWN_VALUE,
    KNIGHT_VALUE,
    BISHOP_VALUE,
    ROOK_VALUE,
    QUEEN_VALUE,
    KING_VALUE,
    0
};

static inline int pieceValue(int p) {
    return PIECE_VALUES[p & PieceCache];
}

static inline int invertColor(int p){
    return p ^ 0b1000;
}

static inline int oppositeColor(int p){
    return color(invertColor(p));
}

static inline int uncolor(int p){
    return p & PieceCache;
}

static inline int colorIndex(int p){
    return (p >> 3) & 1;
}

static inline int oppositeColorIndex(int p){
    return colorIndex(p) ^ 1;
}

#endif // PIECE_H
