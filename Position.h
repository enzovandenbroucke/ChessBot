#ifndef POSITION_H
#define POSITION_H

#include "Piece.h"
#include "Move.h"
#include "Data.h"
#include "Types.h"
#include "Zobrist.h"
#include "Eval.h"
#include "PolyglotZobrist.h"
#include "TTable.h"

extern PolyglotEntry* bookEntries;
extern long bookSize;

extern TTEntry* tTable;
extern uint64_t* pvTable;
extern int* pvLength;
extern bool flagOpening;
extern bool precomputed;

static inline bool hasNonPawnMaterial(Position* pos) {
    return( pos->pieces[!pos->whiteToMove][Knight] ||
            pos->pieces[!pos->whiteToMove][Bishop] ||
            pos->pieces[!pos->whiteToMove][Rook]   ||
            pos->pieces[!pos->whiteToMove][Queen]);
}

static inline void updateAggregateBitboards(Position* pos){
    pos->byColor[WHITE_INDEX] = pos->pieces[WHITE_INDEX][Pawn] | pos->pieces[WHITE_INDEX][Knight] | pos->pieces[WHITE_INDEX][Bishop] | pos->pieces[WHITE_INDEX][Rook] | pos->pieces[WHITE_INDEX][Queen] | pos->pieces[WHITE_INDEX][King];
    pos->byColor[BLACK_INDEX] = pos->pieces[BLACK_INDEX][Pawn] | pos->pieces[BLACK_INDEX][Knight] | pos->pieces[BLACK_INDEX][Bishop] | pos->pieces[BLACK_INDEX][Rook] | pos->pieces[BLACK_INDEX][Queen] | pos->pieces[BLACK_INDEX][King];
    pos->occupied = pos->byColor[WHITE_INDEX] | pos->byColor[BLACK_INDEX];
}

static inline bool pieceToMove(Position* pos,int p){
    return ((pos->whiteToMove && isWhite(p)) || (!pos->whiteToMove && isBlack(p)));
}

static inline bool validIndex(int i){
    return (0 <= i && i < 64);
}

static inline int gameState(Position* pos){
    int res = 0;
    if (pos->KCastle) res |= 1;
    if (pos->QCastle) res |= 2;
    if (pos->kCastle) res |= 4;
    if (pos->qCastle) res |= 8;

    if (pos->enPassant == -1) res |= 1 << 10;
    else                      res |= pos->enPassant << 4;

    res |= (pos->halfMove & MOVECOUNT_CACHE) << 11;
    return res;
}

void initOpeningBook(const char* filename);

void printBitboard(uint64_t bb);
void printBoard(Position* pos);
Position* allocateMemory();
void initBoard(Position* pos, char* fen);
void freePosition(Position* pos);
void freeGlobalData();

void squareToString(char* res,int square);
void printMove(uint64_t m);
void boardToFen(Position* pos,char* w);

bool isRepetition(Position* pos);
uint64_t rand64();

#endif
