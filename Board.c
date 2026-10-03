#include "Board.h"

static void incrAggregateBitboards(Position* pos, uint64_t m, int p,
                                 int pieceColor, bool boolIsEnPassant);

static inline void addMove(Position* pos, uint64_t m) {
    int startIndex = 256 * pos->currentDepth;
    int count = (int)pos->moves[startIndex] + 1;
    pos->moves[startIndex] = count;
    pos->moves[startIndex + count] = m;
}

static void genPawnMovesWhite(Position* pos, int state){
    uint64_t bbPawns = pos->pieces[WHITE_INDEX][Pawn];
    uint64_t singlePushs = (bbPawns << 8) & (~pos->occupied);
    uint64_t doublePushs = (singlePushs << 8) & (~pos->occupied) & (0b11111111ULL << 24);
    while(singlePushs){
        int sq = __builtin_ctzll(singlePushs);
        singlePushs &= singlePushs-1;
        if(sq > 55){
            addMove(pos,make(sq-8,sq,Queen | White ,Vacant,state,0,mvScore[8*Vacant|Pawn] + ((uint64_t) PROMOTION_BONUS << 41)));
            addMove(pos,make(sq-8,sq,Rook  | White,Vacant,state,0,mvScore[8*Vacant|Pawn] + ((uint64_t) UNDERPROMOTION_BONUS << 41)));
            addMove(pos,make(sq-8,sq,Knight| White,Vacant,state,0,mvScore[8*Vacant|Pawn] + ((uint64_t) UNDERPROMOTION_BONUS << 41)));
            addMove(pos,make(sq-8,sq,Bishop| White,Vacant,state,0,mvScore[8*Vacant|Pawn] + ((uint64_t) UNDERPROMOTION_BONUS << 41)));
        } else {
            addMove(pos,makeOrdinaryMove(sq-8,sq,Vacant,state,mvScore[8*Vacant|Pawn]));
        }
    }
    while(doublePushs){
        int sq = __builtin_ctzll(doublePushs);
        doublePushs &= doublePushs-1;
        addMove(pos,makeOrdinaryMove(sq-16,sq,Vacant,state,mvScore[8*Vacant|Pawn]));
    }
    while(bbPawns){
        int startSq = __builtin_ctzll(bbPawns);
        bbPawns &= bbPawns - 1;
        uint64_t attacks = pawnAttacks[WHITE_INDEX][startSq] & pos->byColor[BLACK_INDEX];
        if(((attacks >> (startSq+7)) & 1ULL) == 1ULL){
            if(startSq > 47){
                int q = pos->board[startSq+7];
                addMove(pos,make(startSq,startSq+7,Queen | White ,q,state,0,mvScore[8*q|Pawn] + ((uint64_t) PROMOTION_BONUS << 41)));
                addMove(pos,make(startSq,startSq+7,Rook  | White,q,state,0,mvScore[8*q|Pawn] + ((uint64_t) UNDERPROMOTION_BONUS << 41)));
                addMove(pos,make(startSq,startSq+7,Knight| White,q,state,0,mvScore[8*q|Pawn] + ((uint64_t) UNDERPROMOTION_BONUS << 41)));
                addMove(pos,make(startSq,startSq+7,Bishop| White,q,state,0,mvScore[8*q|Pawn] + ((uint64_t) UNDERPROMOTION_BONUS << 41)));

            } else {
                addMove(pos,makeOrdinaryMove(startSq,startSq+7,pos->board[startSq+7],state,mvScore[8*pos->board[startSq+7]|Pawn]));
            }
        }
        if(startSq < 55 && (((attacks >> (startSq + 9)) & 1ULL) == 1ULL)){
            if(startSq > 47){
                int q = pos->board[startSq+9];
                addMove(pos,make(startSq,startSq+9,Queen | White ,q,state,0,mvScore[8*q|Pawn] + ((uint64_t) PROMOTION_BONUS << 41)));
                addMove(pos,make(startSq,startSq+9,Rook  | White,q,state,0,mvScore[8*q|Pawn] + ((uint64_t) UNDERPROMOTION_BONUS << 41)));
                addMove(pos,make(startSq,startSq+9,Knight| White,q,state,0,mvScore[8*q|Pawn] + ((uint64_t) UNDERPROMOTION_BONUS << 41)));
                addMove(pos,make(startSq,startSq+9,Bishop| White,q,state,0,mvScore[8*q|Pawn] + ((uint64_t) UNDERPROMOTION_BONUS << 41)));
            } else {
                addMove(pos,makeOrdinaryMove(startSq,startSq+9,pos->board[startSq+9],state,mvScore[8*pos->board[startSq+9]|Pawn]));
            }
        }

        if(pos->enPassant >= 0 && pos->enPassant == startSq+7 && startSq%8 > 0)
            addMove(pos,makeMoveWithoutPromotion(startSq,startSq+7,Pawn | Black,state,1,mvScore[8*Pawn|Pawn]));
        if(pos->enPassant >= 0 && pos->enPassant == startSq+9 && startSq%8 < 7)
            addMove(pos,makeMoveWithoutPromotion(startSq,startSq+9,Pawn | Black,state,1,mvScore[8*Pawn|Pawn]));
    }
}

static void genPawnMovesBlack(Position* pos, int state){
    uint64_t bbPawns = pos->pieces[BLACK_INDEX][Pawn];
    uint64_t singlePushs = (bbPawns >> 8) & (~pos->occupied);
    uint64_t doublePushs = (singlePushs >> 8) & (~pos->occupied) & (0b11111111ULL << 32);
    while(singlePushs){
        int sq = __builtin_ctzll(singlePushs);
        singlePushs &= singlePushs-1;
        if(sq < 8){
            addMove(pos,make(sq+8,sq,Queen | Black ,Vacant,state,0,mvScore[8*Vacant|Pawn] + ((uint64_t) PROMOTION_BONUS << 41)));
            addMove(pos,make(sq+8,sq,Rook  | Black,Vacant,state,0,mvScore[8*Vacant|Pawn] + ((uint64_t) UNDERPROMOTION_BONUS << 41)));
            addMove(pos,make(sq+8,sq,Knight| Black,Vacant,state,0,mvScore[8*Vacant|Pawn] + ((uint64_t) UNDERPROMOTION_BONUS << 41)));
            addMove(pos,make(sq+8,sq,Bishop| Black,Vacant,state,0,mvScore[8*Vacant|Pawn] + ((uint64_t) UNDERPROMOTION_BONUS << 41)));
        } else {
            addMove(pos,makeOrdinaryMove(sq+8,sq,Vacant,state,mvScore[8*Vacant|Pawn]));
        }
    }
    while(doublePushs){
        int sq = __builtin_ctzll(doublePushs);
        doublePushs &= doublePushs-1;
        addMove(pos,makeOrdinaryMove(sq+16,sq,Vacant,state,mvScore[8*Vacant|Pawn]));
    }
    while(bbPawns){
        int startSq = __builtin_ctzll(bbPawns);
        bbPawns &= bbPawns - 1;
        uint64_t attacks = pawnAttacks[BLACK_INDEX][startSq] & pos->byColor[WHITE_INDEX];
        if(((attacks >> (startSq-7)) & 1ULL) == 1ULL){
            if(startSq < 16){
                int q = pos->board[startSq-7];
                addMove(pos,make(startSq,startSq-7,Queen | Black,q,state,0,mvScore[8*q|Pawn] + ((uint64_t) PROMOTION_BONUS << 41)));
                addMove(pos,make(startSq,startSq-7,Rook  | Black,q,state,0,mvScore[8*q|Pawn] + ((uint64_t) UNDERPROMOTION_BONUS << 41)));
                addMove(pos,make(startSq,startSq-7,Knight| Black,q,state,0,mvScore[8*q|Pawn] + ((uint64_t) UNDERPROMOTION_BONUS << 41)));
                addMove(pos,make(startSq,startSq-7,Bishop| Black,q,state,0,mvScore[8*q|Pawn] + ((uint64_t) UNDERPROMOTION_BONUS << 41)));
            } else {
                addMove(pos,makeOrdinaryMove(startSq,startSq-7,pos->board[startSq-7],state,mvScore[8*pos->board[startSq-7]|Pawn]));
            }
        }
        if(startSq > 8 && (((attacks >> (startSq - 9)) & 1ULL) == 1ULL)){
            if(startSq < 16){
                int q = pos->board[startSq-9];
                addMove(pos,make(startSq,startSq-9,Queen | Black,q,state,0,mvScore[8*q|Pawn] + ((uint64_t) PROMOTION_BONUS << 41)));
                addMove(pos,make(startSq,startSq-9,Rook  | Black,q,state,0,mvScore[8*q|Pawn] + ((uint64_t) UNDERPROMOTION_BONUS << 41)));
                addMove(pos,make(startSq,startSq-9,Knight| Black,q,state,0,mvScore[8*q|Pawn] + ((uint64_t) UNDERPROMOTION_BONUS << 41)));
                addMove(pos,make(startSq,startSq-9,Bishop| Black,q,state,0,mvScore[8*q|Pawn] + ((uint64_t) UNDERPROMOTION_BONUS << 41)));
            } else {
                addMove(pos,makeOrdinaryMove(startSq,startSq-9,pos->board[startSq-9],state,mvScore[8*pos->board[startSq-9]|Pawn]));
            }
        }

        //Now en passant check
        if(pos->enPassant >= 0 && pos->enPassant == startSq-7 && startSq % 8 < 7)
            addMove(pos,makeMoveWithoutPromotion(startSq,startSq-7,Pawn | White,state,1,mvScore[8*Pawn|Pawn]));
        if(pos->enPassant >= 0 && pos->enPassant == startSq-9 && startSq % 8 > 0)
            addMove(pos,makeMoveWithoutPromotion(startSq,startSq-9,Pawn | White,state,1,mvScore[8*Pawn|Pawn]));
    }
}

static void genKnightMoves(Position* pos,int sq, int colorIndex, int state){
    uint64_t bb = knightAttacks[sq] & (~pos->byColor[colorIndex]);
    while(bb){
        int destination = __builtin_ctzll(bb);
        bb &= bb-1;
        addMove(pos,makeOrdinaryMove(sq,destination,pos->board[destination],state,mvScore[8*pos->board[destination]|Knight]));
    }
}

uint64_t rookAttacks(int square, uint64_t occupied){
    uint64_t attackMask = 0;
    for(int i = 0; i < 4; i++){
        int d = DIRECTION_OFFSETS[i]; //need DIRECTION_OFFSETS rather than dirs by design of numSquaresToEdge
        int limit = numSquaresToEdge[square*8 + i];
        int currentSquare = square;
        for(int step = 1 ; step <= limit; step++){
            currentSquare += d;
            uint64_t bb = 1ULL << currentSquare;
            attackMask |= bb;
            if((bb & occupied) != 0) break;
        }
    }

    return attackMask;
}

uint64_t bishopAttacks(int square, uint64_t occupied){
    uint64_t attackMask = 0;
    for(int i = 4; i < 8; i++){
        int d = DIRECTION_OFFSETS[i]; //need DIRECTION_OFFSETS rather than dirs by design of numSquaresToEdge
        int limit = numSquaresToEdge[square*8 + i];
        int currentSquare = square;
        for(int step = 1 ; step <= limit; step++){
            currentSquare += d;
            uint64_t bb = 1ULL << currentSquare;
            attackMask |= bb;
            if((bb & occupied) != 0) break;
        }
    }

    return attackMask;
}

static void genBishopMoves(Position *pos,int i, int colorIndex, int state){
    uint64_t bb = bishopAttacksFast(i,pos->occupied) & (~pos->byColor[colorIndex]);
    while(bb){
        int destination = __builtin_ctzll(bb);
        bb &= bb-1;
        addMove(pos,makeOrdinaryMove(i,destination,pos->board[destination],state,mvScore[8*pos->board[destination]|Bishop]));
    }
}

static void genRookMoves(Position *pos,int i, int colorIndex, int state){
    uint64_t bb = rookAttacksFast(i,pos->occupied) & (~pos->byColor[colorIndex]);
    while(bb){
        int destination = __builtin_ctzll(bb);
        bb &= bb-1;
        addMove(pos,makeOrdinaryMove(i,destination,pos->board[destination],state,mvScore[8*pos->board[destination]|Rook]));
    }
}

static void genQueenMoves(Position *pos, int sq, int colorIndex, int state) {
    uint64_t bb = (rookAttacksFast(sq, pos->occupied) | bishopAttacksFast(sq, pos->occupied)) & ~pos->byColor[colorIndex];

    while (bb) {
        int destination = __builtin_ctzll(bb);
        bb &= bb - 1;

        addMove(pos, makeOrdinaryMove(sq, destination, pos->board[destination], state,
                           mvScore[8 * pos->board[destination] | Queen]));
    }
}

static void genKingMoves(Position* pos,int i, int colorIndex, int state){

    uint64_t bb = kingAttacks[i] & (~pos->byColor[colorIndex]);
    while(bb){
        int destination = __builtin_ctzll(bb);
        bb &= bb-1;
        addMove(pos,makeOrdinaryMove(i,destination,pos->board[destination],state,mvScore[8*pos->board[destination]|King]));
    }

    if (i == 4 && pos->KCastle && isVacant(pos->board[5]) && isVacant(pos->board[6]) &&
        !isAttacked(pos,4, true) && !isAttacked(pos,5, true) && !isAttacked(pos,6, true)){
        addMove(pos,makeOrdinaryMove(4, 6, pos->board[6], state,mvScore[8*pos->board[6]|King]));
    }
    if (i == 4 && pos->QCastle && isVacant(pos->board[3]) && isVacant(pos->board[2]) && isVacant(pos->board[1]) &&
        !isAttacked(pos,4, true) && !isAttacked(pos,3, true) && !isAttacked(pos,2, true)){
        addMove(pos,makeOrdinaryMove(4, 2, pos->board[2], state,mvScore[8*pos->board[2]|King]));
    }
    if (i == 60 && pos->kCastle && isVacant(pos->board[61]) && isVacant(pos->board[62]) &&
        !isAttacked(pos,60, false) && !isAttacked(pos,61, false) && !isAttacked(pos,62, false)){
        addMove(pos,makeOrdinaryMove(60, 62, pos->board[62], state,mvScore[8*pos->board[62]|King]));
    }
    if (i == 60 && pos->qCastle && isVacant(pos->board[59]) && isVacant(pos->board[58]) && isVacant(pos->board[57]) &&
        !isAttacked(pos,60, false) && !isAttacked(pos,59, false) && !isAttacked(pos,58, false)){
        addMove(pos,makeOrdinaryMove(60, 58, pos->board[58], state,mvScore[8*pos->board[58]|King]));
    }
}

void generatePLMoves(Position* pos){
    pos->moves[256*pos->currentDepth] = 0;
    // Stores all pseudo-legal moves in any given position in moves array
    int state = gameState(pos);
    int colorIndex = pos->whiteToMove ? WHITE_INDEX : BLACK_INDEX;
    if(pos->whiteToMove) genPawnMovesWhite(pos,state);
    else genPawnMovesBlack(pos,state);

    uint64_t bb = pos->pieces[colorIndex][Knight];
    while(bb){
        int sq = __builtin_ctzll(bb);
        bb &= bb-1;
        genKnightMoves(pos,sq,colorIndex,state);
    }
    bb = pos->pieces[colorIndex][Bishop];

    while(bb){
        int sq = __builtin_ctzll(bb);
        bb &= bb-1;
        genBishopMoves(pos,sq,colorIndex,state);
    }
    bb = pos->pieces[colorIndex][Rook];

    while(bb){
        int sq = __builtin_ctzll(bb);
        bb &= bb-1;
        genRookMoves(pos,sq,colorIndex,state);
    }

    bb = pos->pieces[colorIndex][Queen];
    while(bb){
        int sq = __builtin_ctzll(bb);
        bb &= bb-1;
        genQueenMoves(pos,sq,colorIndex,state);
    }
    bb = pos->pieces[colorIndex][King];
    while(bb){
        int sq = __builtin_ctzll(bb);
        bb &= bb-1;
        genKingMoves(pos,sq,colorIndex,state);
    }
}

static void genPawnCapturesWhite(Position* pos, int state){
    uint64_t bbPawns = pos->pieces[WHITE_INDEX][Pawn];
    while(bbPawns){
        int startSq = __builtin_ctzll(bbPawns);
        bbPawns &= bbPawns - 1;
        uint64_t attacks = pawnAttacks[WHITE_INDEX][startSq] & pos->byColor[BLACK_INDEX];
        if(((attacks >> (startSq+7)) & 1ULL) == 1ULL){
            if(startSq > 47){
                addMove(pos,make(startSq,startSq+7,Queen| White, pos->board[startSq+7],state,0,mvScore[8*pos->board[startSq+7]|Pawn] + ((uint64_t) PROMOTION_BONUS << 41)));
                addMove(pos,make(startSq,startSq+7,Rook| White,  pos->board[startSq+7],state,0,mvScore[8*pos->board[startSq+7]|Pawn] + ((uint64_t) UNDERPROMOTION_BONUS << 41)));
                addMove(pos,make(startSq,startSq+7,Knight| White,pos->board[startSq+7],state,0,mvScore[8*pos->board[startSq+7]|Pawn] + ((uint64_t) UNDERPROMOTION_BONUS << 41)));
                addMove(pos,make(startSq,startSq+7,Bishop| White,pos->board[startSq+7],state,0,mvScore[8*pos->board[startSq+7]|Pawn] + ((uint64_t) UNDERPROMOTION_BONUS << 41)));

            } else {
                addMove(pos,makeOrdinaryMove(startSq,startSq+7,pos->board[startSq+7],state,mvScore[8*pos->board[startSq+7]|Pawn]));
            }
        }
        if(startSq < 55 && (((attacks >> (startSq + 9)) & 1ULL) == 1ULL)){
            if(startSq > 47){
                addMove(pos,make(startSq,startSq+9,Queen| White, pos->board[startSq+9],state,0,mvScore[8*pos->board[startSq+9]|Pawn] + ((uint64_t) PROMOTION_BONUS << 41)));
                addMove(pos,make(startSq,startSq+9,Rook| White,  pos->board[startSq+9],state,0,mvScore[8*pos->board[startSq+9]|Pawn] + ((uint64_t) UNDERPROMOTION_BONUS << 41)));
                addMove(pos,make(startSq,startSq+9,Knight| White,pos->board[startSq+9],state,0,mvScore[8*pos->board[startSq+9]|Pawn] + ((uint64_t) UNDERPROMOTION_BONUS << 41)));
                addMove(pos,make(startSq,startSq+9,Bishop| White,pos->board[startSq+9],state,0,mvScore[8*pos->board[startSq+9]|Pawn] + ((uint64_t) UNDERPROMOTION_BONUS << 41)));

            } else {
                addMove(pos,makeOrdinaryMove(startSq,startSq+9,pos->board[startSq+9],state,mvScore[8*pos->board[startSq+9]|Pawn]));
            }
        }

        if(pos->enPassant >= 0 && pos->enPassant == startSq+7 && startSq%8 > 0)
            addMove(pos,makeMoveWithoutPromotion(startSq,startSq+7,Pawn | Black,state,1,mvScore[8*Pawn|Pawn]));
        if(pos->enPassant >= 0 && pos->enPassant == startSq+9 && startSq%8 < 7)
            addMove(pos,makeMoveWithoutPromotion(startSq,startSq+9,Pawn | Black,state,1,mvScore[8*Pawn|Pawn]));
    }
}

static void genPawnCapturesBlack(Position* pos, int state){
    uint64_t bbPawns = pos->pieces[BLACK_INDEX][Pawn];
    while(bbPawns){
        int startSq = __builtin_ctzll(bbPawns);
        bbPawns &= bbPawns - 1;
        uint64_t attacks = pawnAttacks[BLACK_INDEX][startSq] & pos->byColor[WHITE_INDEX];
        if(((attacks >> (startSq-7)) & 1ULL) == 1ULL){
            if(startSq < 16){
                addMove(pos,make(startSq,startSq-7,Queen| Black, pos->board[startSq-7],state,0,mvScore[8*pos->board[startSq-7]|Pawn] + ((uint64_t) PROMOTION_BONUS << 41)));
                addMove(pos,make(startSq,startSq-7,Rook| Black,  pos->board[startSq-7],state,0,mvScore[8*pos->board[startSq-7]|Pawn] + ((uint64_t) UNDERPROMOTION_BONUS << 41)));
                addMove(pos,make(startSq,startSq-7,Knight| Black,pos->board[startSq-7],state,0,mvScore[8*pos->board[startSq-7]|Pawn] + ((uint64_t) UNDERPROMOTION_BONUS << 41)));
                addMove(pos,make(startSq,startSq-7,Bishop| Black,pos->board[startSq-7],state,0,mvScore[8*pos->board[startSq-7]|Pawn] + ((uint64_t) UNDERPROMOTION_BONUS << 41)));

            } else {
                addMove(pos,makeOrdinaryMove(startSq,startSq-7,pos->board[startSq-7],state,mvScore[8*pos->board[startSq-7]|Pawn]));
            }
        }
        if(startSq > 8 && (((attacks >> (startSq - 9)) & 1ULL) == 1ULL)){
            if(startSq < 16){
                addMove(pos,make(startSq,startSq-9,Queen| Black, pos->board[startSq-9],state,0,mvScore[8*pos->board[startSq-9]|Pawn] + ((uint64_t) PROMOTION_BONUS << 41)));
                addMove(pos,make(startSq,startSq-9,Rook| Black,  pos->board[startSq-9],state,0,mvScore[8*pos->board[startSq-9]|Pawn] + ((uint64_t) UNDERPROMOTION_BONUS << 41)));
                addMove(pos,make(startSq,startSq-9,Knight| Black,pos->board[startSq-9],state,0,mvScore[8*pos->board[startSq-9]|Pawn] + ((uint64_t) UNDERPROMOTION_BONUS << 41)));
                addMove(pos,make(startSq,startSq-9,Bishop| Black,pos->board[startSq-9],state,0,mvScore[8*pos->board[startSq-9]|Pawn] + ((uint64_t) UNDERPROMOTION_BONUS << 41)));

            } else {
                addMove(pos,makeOrdinaryMove(startSq,startSq-9,pos->board[startSq-9],state,mvScore[8*pos->board[startSq-9]|Pawn]));
            }
        }

        //Now en passant check
        if(pos->enPassant >= 0 && pos->enPassant == startSq-7 && startSq % 8 < 7)
            addMove(pos,makeMoveWithoutPromotion(startSq,startSq-7,Pawn | White,state,1,mvScore[8*Pawn|Pawn]));
        if(pos->enPassant >= 0 && pos->enPassant == startSq-9 && startSq % 8 > 0)
            addMove(pos,makeMoveWithoutPromotion(startSq,startSq-9,Pawn | White,state,1,mvScore[8*Pawn|Pawn]));
    }
}

static void genKnightCaptures(Position* pos,int sq, int colorIndex, int state){
    uint64_t bb = knightAttacks[sq] & pos->byColor[colorIndex^1];
    while(bb){
        int destination = __builtin_ctzll(bb);
        bb &= bb-1;
        addMove(pos,makeOrdinaryMove(sq,destination,pos->board[destination],state,mvScore[8*pos->board[destination]|Knight]));
    }
}

static void genBishopCaptures(Position *pos,int i, int colorIndex, int state){
    uint64_t bb = bishopAttacksFast(i,pos->occupied) & pos->byColor[colorIndex ^ 1];
    while(bb){
        int destination = __builtin_ctzll(bb);
        bb &= bb-1;
        addMove(pos,makeOrdinaryMove(i,destination,pos->board[destination],state,mvScore[8*pos->board[destination]|Bishop]));
    }
}

static void genRookCaptures(Position *pos,int i, int colorIndex, int state){
    uint64_t bb = rookAttacksFast(i,pos->occupied) & pos->byColor[colorIndex ^ 1];
    while(bb){
        int destination = __builtin_ctzll(bb);
        bb &= bb-1;
        addMove(pos,makeOrdinaryMove(i,destination,pos->board[destination],state,mvScore[8*pos->board[destination]|Rook]));
    }
}

static void genQueenCaptures(Position *pos, int i,int colorIndex, int state){
    uint64_t bb = (rookAttacksFast(i, pos->occupied) | bishopAttacksFast(i, pos->occupied)) & pos->byColor[colorIndex ^ 1];

    while (bb) {
        int destination = __builtin_ctzll(bb);
        bb &= bb - 1;

        addMove(pos, makeOrdinaryMove(i, destination, pos->board[destination], state,
                           mvScore[8 * pos->board[destination] | Queen]));
    }
}

void genQuietQueenPromotionsWhite(Position* pos, int state) {
    uint64_t bb = (pos->pieces[WHITE_INDEX][Pawn] << 8) & ~pos->occupied;

    while (bb) {
        int sq = __builtin_ctzll(bb);
        bb &= bb - 1;

        if (sq > 55) {
            addMove(pos, make(sq - 8, sq, Queen | White, Vacant, state, 0,
                              mvScore[8 * Vacant | Pawn] +
                              ((uint64_t)PROMOTION_BONUS << 41)));
        }
    }
}

void genQuietQueenPromotionsBlack(Position* pos, int state) {
    uint64_t bb = (pos->pieces[BLACK_INDEX][Pawn] >> 8) & ~pos->occupied;

    while (bb) {
        int sq = __builtin_ctzll(bb);
        bb &= bb - 1;

        if (sq < 8) {
            addMove(pos, make(sq + 8, sq, Queen | Black, Vacant, state, 0,
                              mvScore[8 * Vacant | Pawn] +
                              ((uint64_t)PROMOTION_BONUS << 41)));
        }
    }
}

static void genKingCaptures(Position* pos,int i, int colorIndex, int state){
    uint64_t bb = kingAttacks[i] & pos->byColor[colorIndex^1];
    while(bb){
        int destination = __builtin_ctzll(bb);
        bb &= bb-1;
        addMove(pos,makeOrdinaryMove(i,destination,pos->board[destination],state,mvScore[8*pos->board[destination]|King]));
    }
}

void generatePLCaptures(Position* pos){
    pos->moves[256*pos->currentDepth] = 0;
    // Stores all pseudo-legal moves in any given position in moves array
    int state = gameState(pos);
    int colorIndex = pos->whiteToMove ? WHITE_INDEX : BLACK_INDEX;

    if(pos->whiteToMove) genPawnCapturesWhite(pos,state);
    else genPawnCapturesBlack(pos,state);

    uint64_t bb = pos->pieces[colorIndex][Knight];
    while(bb){
        int sq = __builtin_ctzll(bb);
        bb &= bb-1;
        genKnightCaptures(pos,sq,colorIndex,state);
    }
    bb = pos->pieces[colorIndex][Bishop];
    while(bb){
        int sq = __builtin_ctzll(bb);
        bb &= bb-1;
        genBishopCaptures(pos,sq,colorIndex,state);
    }
    bb = pos->pieces[colorIndex][Rook];
    while(bb){
        int sq = __builtin_ctzll(bb);
        bb &= bb-1;
        genRookCaptures(pos,sq,colorIndex,state);
    }
    bb = pos->pieces[colorIndex][Queen];
    while(bb){
        int sq = __builtin_ctzll(bb);
        bb &= bb-1;
        genQueenCaptures(pos,sq,colorIndex,state);
    }
    bb = pos->pieces[colorIndex][King];
    while(bb){
        int sq = __builtin_ctzll(bb);
        bb &= bb-1;
        genKingCaptures(pos,sq,colorIndex,state);
    }
}

static void smallMove(Position* pos,uint64_t m)
{
    // ASSUMPTION : m is a pseudo-legal or legal move
    // 1. Save
    int p = pos->board[prev(m)];
    bool boolIsEnPassant = isEnPassant(m);

    // 2. Counters also works with pos->enPassant

    // 3. Main move
    pos->board[next(m)] = p;
    pos->board[prev(m)] = (Vacant);
    int pieceIndex = uncolor(p);
    int colorPiece = colorIndex(p);
    pos->pieces[colorPiece][pieceIndex] ^= (1ULL << next(m));
    pos->pieces[colorPiece][pieceIndex] ^= (1ULL << prev(m));

    // 4. Special moves

    // B. En passant
    if (boolIsEnPassant)
    {
        int delta = next(m) % 8 - prev(m) % 8;
        pos->board[prev(m) + delta] = (Vacant);
        pos->pieces[oppositeColorIndex(p)][Pawn] ^= (1ULL << (prev(m) + delta));
    } else if (!isVacant(captured(m))){
        pos->pieces[oppositeColorIndex(p)][uncolor(captured(m))] ^= (1ULL << next(m));
    }

    // C. Promotion
    if (isPawn(p) && (next(m) / 8 == 0 || next(m) / 8 == 7)){
        pos->board[next(m)] = promotion(m);
        pos->pieces[colorPiece][Pawn] ^= (1ULL << next(m));
        pos->pieces[colorPiece][uncolor(promotion(m))] ^= (1ULL << next(m));
    }

    // D. Castle

    else if (isKing(p)){
        if (next(m) == prev(m) + 2){
            // Kingside
            pos->board[prev(m) + 1] = pos->board[prev(m) + 3];
            pos->board[prev(m) + 3] = (Vacant);
            pos->pieces[colorPiece][Rook] ^= (1ULL << (prev(m) + 1));
            pos->pieces[colorPiece][Rook] ^= (1ULL << (prev(m) + 3));
        } else if (next(m) == prev(m) - 2){
            // Queenside
            pos->board[prev(m) - 1] = pos->board[prev(m) - 4];
            pos->board[prev(m) - 4] = (Vacant);
            pos->pieces[colorPiece][Rook] ^= (1ULL << (prev(m) - 1));
            pos->pieces[colorPiece][Rook] ^= (1ULL << (prev(m) - 4));
        }
    }

    // 5. Update castling rights and King Locations
    if (isKing(p)){
        if (isWhite(p)) pos->KSquare = next(m);
        else pos->kSquare = next(m);
    }

    incrAggregateBitboards(pos,m,p,colorIndex(p),boolIsEnPassant);
    // 6. End of turn
}

static void smallUnmove(Position* pos,uint64_t m)
{
    // ASSUMPTION : m is a pseudo-legal or legal move
    // 1. Save
    int p = pos->board[next(m)];
    bool boolIsEnPassant = isEnPassant(m);

    // 3. Main move
    pos->board[prev(m)] = p;
    pos->board[next(m)] = captured(m);
    int pieceIndex = uncolor(p);
    int colorPiece = colorIndex(p);
    pos->pieces[colorPiece][pieceIndex] ^= (1ULL << prev(m));
    pos->pieces[colorPiece][pieceIndex] ^= (1ULL << next(m));

    if (boolIsEnPassant){
        int delta = next(m) % 8 - prev(m) % 8;
        pos->board[next(m)] = Vacant;
        pos->pieces[oppositeColorIndex(p)][Pawn] ^= (1ULL << (prev(m) + delta));
        if (isWhite(p)){
            pos->board[prev(m) + delta] = Pawn | Black;
        } else {
            pos->board[prev(m) + delta] = Pawn | White;
        }
    } else if (!isVacant(captured(m))){
        pos->pieces[oppositeColorIndex(p)][uncolor(captured(m))] ^= (1ULL << next(m));
    }

    // 4. Special moves

    // C. Promotion
    if (!isVacant(promotion(m)))
    {
        pos->pieces[colorPiece][Pawn] ^= (1ULL << prev(m));
        pos->pieces[colorPiece][uncolor(promotion(m))] ^= (1ULL << prev(m));
        if (isWhite(p)){
            pos->board[prev(m)] = Pawn | White;
        } else {
            pos->board[prev(m)] = Pawn | Black;
        }
    }

    // D. Castle
    else if (isKing(p))
    {
        if (next(m) == prev(m) + 2)
        {
            // Kingside
            pos->board[prev(m) + 3] = pos->board[prev(m) + 1];
            pos->board[prev(m) + 1] = Vacant;
            pos->pieces[colorPiece][Rook] ^= (1ULL << (prev(m) + 3));
            pos->pieces[colorPiece][Rook] ^= (1ULL << (prev(m) + 1));
        }
        else if (next(m) == prev(m) - 2)
        {
            // Queenside
            pos->board[prev(m) - 4] = pos->board[prev(m) - 1];
            pos->board[prev(m) - 1] = Vacant;
            pos->pieces[colorPiece][Rook] ^= (1ULL << (prev(m) - 4));
            pos->pieces[colorPiece][Rook] ^= (1ULL << (prev(m) - 1));
        }
    }

    // 5. Update castling rights and King Locations
    if (isKing(p))
    {
        if (isWhite(p))
            pos->KSquare = prev(m);
        else
            pos->kSquare = prev(m);
    }

    incrAggregateBitboards(pos,m,p,colorIndex(p),boolIsEnPassant);
    // 6. End of turn
}

static bool pseudoLegalToLegal(Position* pos,uint64_t m){
    bool res;
    if (pos->whiteToMove){
        smallMove(pos,m);
        res = isAttacked(pos,pos->KSquare, true);
        smallUnmove(pos,m);
    } else {
        smallMove(pos,m);
        res = isAttacked(pos,pos->kSquare, false);
        smallUnmove(pos,m);
    }
    return !res;
}

void generateMoves(Position* pos){
    generatePLMoves(pos);
    int legalMoveCount = 0;
    int totalPLMoves = pos->moves[256 * pos->currentDepth];
    for (int i = 1; i <= totalPLMoves; i++){
        uint64_t m = pos->moves[i + pos->currentDepth * 256];
        if (pseudoLegalToLegal(pos,m)){
            legalMoveCount++;
            pos->moves[legalMoveCount + pos->currentDepth * 256] = m;
        }
    }
    pos->moves[256 * pos->currentDepth] = legalMoveCount;
}

void generateCaptures(Position* pos){
    generatePLCaptures(pos);
    int legalCaptureCount = 0;
    int totalPLCaptures = pos->moves[256 * pos->currentDepth];
    for (int i = 1; i <= totalPLCaptures; i++){
        uint64_t m = pos->moves[i + pos->currentDepth * 256];
        if (pseudoLegalToLegal(pos,m)){
            legalCaptureCount++;
            pos->moves[legalCaptureCount + pos->currentDepth * 256] = m;
        }
    }
    pos->moves[256 * pos->currentDepth] = legalCaptureCount;
}

static void incrAggregateBitboards(Position* pos, uint64_t m,int p,int pieceColor,bool boolIsEnPassant){
    uint64_t fromTo = (1ULL << prev(m)) | (1ULL << next(m));

    /* Moving piece: remove it from one square, add it to the other. */
        pos->byColor[pieceColor] ^= fromTo;

    /* Remove/restore the captured piece. */
    if (boolIsEnPassant) {
        int delta = next(m) % 8 - prev(m) % 8;
        pos->byColor[oppositeColorIndex(p)] ^= 1ULL << (prev(m) + delta);
    }
    else if (!isVacant(captured(m))) {
        pos->byColor[oppositeColorIndex(p)] ^= 1ULL << next(m);
    }

    /* The rook also changes squares during castling. */
    if (isKing(p) && next(m) == prev(m) + 2) {
        pos->byColor[pieceColor] ^=
            (1ULL << (prev(m) + 1)) |
            (1ULL << (prev(m) + 3));
    }
    else if (isKing(p) && next(m) == prev(m) - 2) {
        pos->byColor[pieceColor] ^=
            (1ULL << (prev(m) - 1)) |
            (1ULL << (prev(m) - 4));
    }

    pos->occupied = pos->byColor[WHITE_INDEX] | pos->byColor[BLACK_INDEX];
}

void move(Position* pos,uint64_t m){
    // ASSUMPTION : m is a pseudo-legal or legal move
    // 1. Save
    int p = pos->board[prev(m)];
    bool isCapture = !isVacant(pos->board[next(m)]);
    bool boolIsEnPassant = isEnPassant(m);
    int pieceIndex = uncolor(p);
    int pieceColor = colorIndex(p);

    applyMoveEvaluation(pos,m);

    pos->hashHistory[pos->historyPly] = pos->currentHash;
    pos->pawnKeyHistory[pos->historyPly] = pos->pawnKey;
    pos->historyPly++;
    pos->currentHash = newHash(pos,m,p);

    // Pawn-key update
    if (isPawn(p)) {
        int pawnIdx = (zobristIndex[p] - 1) * 64;
        pos->pawnKey ^= zobristRandom[prev(m) + pawnIdx];
        if (isVacant(promotion(m))) {
            pos->pawnKey ^= zobristRandom[next(m) + pawnIdx];
        }
    }

    if (boolIsEnPassant) {
        int delta = next(m) % 8 - prev(m) % 8;
        int epPawnIdx = (zobristIndex[Pawn | oppositeColor(p)] - 1) * 64;
        pos->pawnKey ^= zobristRandom[(prev(m) + delta) + epPawnIdx];
    } else if (!isVacant(captured(m)) && isPawn(captured(m))) {
        int capIdx = (zobristIndex[captured(m)] - 1) * 64;
        pos->pawnKey ^= zobristRandom[next(m) + capIdx];
    }

    // 2. Counters also works with pos->enPassant
    if (isPawn(p) || isCapture) pos->halfMove = 0;
    else pos->halfMove++;

    if (!pos->whiteToMove) pos->fullMove++;

    pos->enPassant = -1;

    // 3. Main move
    pos->board[next(m)] = p;
    pos->board[prev(m)] = (Vacant);
    pos->pieces[pieceColor][pieceIndex] ^= (1ULL << next(m));
    pos->pieces[pieceColor][pieceIndex] ^= (1ULL << prev(m));
    if(isCapture && !boolIsEnPassant){
        pos->pieces[oppositeColorIndex(p)][uncolor(captured(m))] ^= (1ULL << next(m));
    } else if (boolIsEnPassant){
        int delta = next(m) % 8 - prev(m) % 8;
        pos->board[prev(m) + delta] = (Vacant);
        pos->pieces[oppositeColorIndex(p)][Pawn] ^= (1ULL << (prev(m) + delta));
    }

    // 4. Special moves

    // A. Pawn double push
    if (isPawn(p) && (next(m) == prev(m) + 16 || next(m) == prev(m) - 16)){
        pos->enPassant = (next(m) + prev(m)) / 2;
    }

    // B. En passant : above

    // C. Promotion
    if (isPawn(p) && (next(m) / 8 == 0 || next(m) / 8 == 7)){
        pos->board[next(m)] = promotion(m);
        pos->pieces[pieceColor][Pawn] ^= (1ULL << next(m));
        pos->pieces[pieceColor][uncolor(promotion(m))] ^= (1ULL << next(m));
    }

    // D. Castle
    else if (isKing(p)){
        if (next(m) == prev(m) + 2){
            // Kingside
            pos->board[prev(m) + 1] = pos->board[prev(m) + 3];
            pos->board[prev(m) + 3] = (Vacant);
            pos->pieces[pieceColor][Rook] ^= (1ULL << (prev(m) + 1));
            pos->pieces[pieceColor][Rook] ^= (1ULL << (prev(m) + 3));
        }
        else if (next(m) == prev(m) - 2)
        {
            // Queenside
            pos->board[prev(m) - 1] = pos->board[prev(m) - 4];
            pos->board[prev(m) - 4] = (Vacant);
            pos->pieces[pieceColor][Rook] ^= (1ULL << (prev(m) - 1));
            pos->pieces[pieceColor][Rook] ^= (1ULL << (prev(m) - 4));
        }
    }

    // 5. Update castling rights and King Locations
    if (isKing(p) && isWhite(p)){
            pos->KSquare = next(m);
            if(pos->KCastle){
                pos->KCastle = false;
            }
            if(pos->QCastle){
                pos->QCastle = false;
            }
    } else if (isKing(p) && !isWhite(p)){
        pos->kSquare = next(m);
        if(pos->kCastle){
            pos->kCastle = false;
        }
        if(pos->qCastle){
            pos->qCastle = false;
        }
    }

    if ((prev(m) == 56 || next(m) == 56) && pos->qCastle){
        pos->qCastle = false;
    }
    if ((prev(m) == 63 || next(m) == 63) && pos->kCastle){
        pos->kCastle = false;
    }
    if ((prev(m) == 0 || next(m) == 0) && pos->QCastle){
        pos->QCastle = false;
    }
    if ((prev(m) == 7 || next(m) == 7) && pos->KCastle){
        pos->KCastle = false;
    }

    // 6. End of turn
    pos->whiteToMove = !pos->whiteToMove;

    incrAggregateBitboards(pos,m,p,pieceColor,boolIsEnPassant);
}

void unmove(Position* pos,uint64_t m){
    // ASSUMPTION : m is a pseudo-legal or legal move
    // 1. Save
    int p = pos->board[next(m)];
    bool boolIsEnPassant = isEnPassant(m);

    undoMoveEvaluation(pos,m);

    pos->historyPly--;
    pos->currentHash = pos->hashHistory[pos->historyPly];
    pos->pawnKey = pos->pawnKeyHistory[pos->historyPly];

    // 2.
    pos->halfMove = hCount(m);
    if (pos->whiteToMove)
        pos->fullMove--;

    pos->enPassant = enPassantValue(m);

    // 3. Main move
    pos->board[prev(m)] = p;
    pos->board[next(m)] = captured(m);
    int pieceIndex = uncolor(p);
    int pieceColor = colorIndex(p);
    pos->pieces[pieceColor][pieceIndex] ^= (1ULL << next(m));
    pos->pieces[pieceColor][pieceIndex] ^= (1ULL << prev(m));
    if (boolIsEnPassant){
        int delta = next(m) % 8 - prev(m) % 8;
        pos->board[next(m)] = Vacant;
        pos->board[prev(m) + delta] = Pawn | oppositeColor(p);
        pos->pieces[oppositeColorIndex(p)][Pawn] ^= (1ULL << (prev(m) + delta));
    } else if (captured(m) != Vacant) {
        pos->pieces[oppositeColorIndex(p)][uncolor(captured(m))] ^= (1ULL << next(m));
    }

    // 4. Special moves

    // C. Promotion
    if (!isVacant(promotion(m))){
            pos->board[prev(m)] = Pawn | color(p);
            pos->pieces[colorIndex(p)][Pawn] ^= (1ULL << prev(m));
            pos->pieces[colorIndex(p)][pieceIndex] ^= (1ULL << prev(m));
    }

    // D. Castle
    else if (isKing(p))
    {
        if (next(m) == prev(m) + 2)
        {
            // Kingside
            pos->board[prev(m) + 3] = pos->board[prev(m) + 1];
            pos->board[prev(m) + 1] = Vacant;
            pos->pieces[pieceColor][Rook] ^= (1ULL << (prev(m) + 1));
            pos->pieces[pieceColor][Rook] ^= (1ULL << (prev(m) + 3));
        }
        else if (next(m) == prev(m) - 2)
        {
            // Queenside
            pos->board[prev(m) - 4] = pos->board[prev(m) - 1];
            pos->board[prev(m) - 1] = Vacant;
            pos->pieces[pieceColor][Rook] ^= (1ULL << (prev(m) - 1));
            pos->pieces[pieceColor][Rook] ^= (1ULL << (prev(m) - 4));
        }
    }

    // 5. Update castling rights and King Locations
    if (isKing(p) && isWhite(p)){
        pos->KSquare = prev(m);
    } else if (isKing(p) && !isWhite(p)){
        pos->kSquare = prev(m);
    }
    if(!pos->KCastle && KCas(m)){
        pos->KCastle = KCas(m);
    }
        if(!pos->QCastle && QCas(m)){
        pos->QCastle = QCas(m);
    }
        if(!pos->kCastle && kCas(m)){
        pos->kCastle = kCas(m);
    }
        if(!pos->qCastle && qCas(m)){
        pos->qCastle = qCas(m);
    }
    // 6. End of turn
    pos->whiteToMove = !pos->whiteToMove;
    incrAggregateBitboards(pos,m,p,pieceColor,boolIsEnPassant);
}

