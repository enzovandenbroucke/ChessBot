#ifndef BOARD_H
#define BOARD_H

#include "Piece.h"
#include "Move.h"
#include "Data.h"
#include "Types.h"
#include "Zobrist.h"
#include "Eval.h"
#include "PolyglotZobrist.h"
#include "TTable.h"
#include "Position.h"

static inline bool isAttacked(Position* pos,int square, bool whiteFlag){
    int friendlyColorIndex = (int)!whiteFlag;
    int oppositeColorIndex = (int)whiteFlag; //no branching optimization
    bool res = false;
    res = res || ((pawnAttacks[friendlyColorIndex][square] & pos->pieces[oppositeColorIndex][Pawn]) != 0);
    res = res || ((knightAttacks[square] & pos->pieces[oppositeColorIndex][Knight]) != 0);
    res = res || ((kingAttacks[square] & pos->pieces[oppositeColorIndex][King]) != 0);
    res = res || ((bishopAttacksFast(square, pos->occupied) & (pos->pieces[oppositeColorIndex][Bishop] | pos->pieces[oppositeColorIndex][Queen])) != 0);
    res = res || ((rookAttacksFast(square, pos->occupied) & (pos->pieces[oppositeColorIndex][Rook] | pos->pieces[oppositeColorIndex][Queen])) != 0);
    return res;
}

static inline bool isInCheck(Position* pos){
    return (pos->whiteToMove ? isAttacked(pos, pos->KSquare, true) : isAttacked(pos, pos->kSquare, false));
}
static inline bool canCaptureKing(Position* pos){
    return (!pos->whiteToMove ? isAttacked(pos, pos->KSquare, true) : isAttacked(pos, pos->kSquare, false));
}
static inline bool canStepOn(Position* pos,int destination, bool whiteFlag)
{
    return ( (!isWhite(pos->board[destination]) && whiteFlag) || (!isBlack(pos->board[destination]) && !whiteFlag));
}

static inline bool canCapture(int p,bool whiteFlag){
    return ((isWhite(p) && !whiteFlag) || (isBlack(p) && whiteFlag));
}

static inline void makeNullMove(Position* pos, NullMoveState* s) {
    s->prevEP = pos->enPassant;
    s->prevHash = pos->currentHash;
    pos->whiteToMove = !pos->whiteToMove;
    pos->currentDepth++;
    pos->enPassant = -1;
    pos->currentHash ^= zobristRandom[12 * 64];

    if (s->prevEP != -1) {
        int epRank = s->prevEP / 8;
        int pawnSq = (epRank == 2) ? s->prevEP + 8 : s->prevEP - 8;
        int col2 = pawnSq % 8;
        int expectedPawn = Pawn | (epRank == 2 ? Black : White);

        if ((col2 > 0 && pos->board[pawnSq - 1] == expectedPawn) ||
            (col2 < 7 && pos->board[pawnSq + 1] == expectedPawn)) {
            pos->currentHash ^= zobristRandom[12 * 64 + 5 + col2];
        }
    }
}

static inline void unmakeNullMove(Position* pos, const NullMoveState* s) {
    pos->currentDepth--;
    pos->whiteToMove = !pos->whiteToMove;
    pos->enPassant = s->prevEP;
    pos->currentHash = s->prevHash;
}

uint64_t rookAttacks(int square, uint64_t occupied);
uint64_t bishopAttacks(int square, uint64_t occupied);

void generatePLMoves(Position *pos);
void generateMoves(Position *pos);

void generatePLCaptures(Position *pos);
void generateCaptures(Position *pos);

void genQuietQueenPromotionsWhite(Position* pos, int state);
void genQuietQueenPromotionsBlack(Position* pos, int state);

void move(Position *pos,uint64_t m);
void unmove(Position *pos,uint64_t m);

// Legacy perft driver, implemented in the excluded test.c entry point.
uint64_t countMoves(Position *pos,int depth);

#endif
