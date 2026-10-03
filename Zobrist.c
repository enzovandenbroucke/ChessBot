#include "Zobrist.h"

/*
zobristRandom is a size 781 array of random long ints divided into 4 categories :
    one per piece type (non Vacant) and per square on the board : 12 * 64 = 768 in total
    one to indicate black to move
    four for castling rights in the order K,Q,k,q
    eight for the file of the enPassant square (if any)
*/
uint64_t computeHash(Position *pos){
    uint64_t h = 0;
    for(int sq = 0; sq < 64; sq++){
        int p = pos->board[sq];
        if(!isVacant(p)){
            h ^= zobristRandom[sq + (zobristIndex[p]-1)*64];
        }
    }

    if(!pos->whiteToMove) h ^= zobristRandom[12*64];

    if(pos->KCastle) h ^= zobristRandom[12*64+1];
    if(pos->QCastle) h ^= zobristRandom[12*64+2];
    if(pos->kCastle) h ^= zobristRandom[12*64+3];
    if(pos->qCastle) h ^= zobristRandom[12*64+4];

    if(pos->enPassant != -1) {
        int sq = pos->enPassant;
        int epRank = sq / 8;
        int pawnSq = (epRank == 2) ? sq + 8 : sq - 8;
        int col2 = pawnSq % 8;
        int attackerColor = (epRank == 2) ? Black : White;
        int expectedPawn = Pawn | attackerColor;

        if( (col2 > 0 && pos->board[pawnSq - 1] == expectedPawn) ||
            (col2 < 7 && pos->board[pawnSq + 1] == expectedPawn) ) {
            h ^= zobristRandom[12*64 + 5 + col2];
        }
    }

    return h;
}

uint64_t newHash(Position* pos, uint64_t m, int p){
    int i = zobristIndex[p]-1; //To bring back the piece index btw 0 and 11
    uint64_t h = pos->currentHash;
    bool isCapture = !isVacant(captured(m));
    bool flagEnPassant = isEnPassant(m);

    h ^= zobristRandom[prev(m)+i*64];
    h ^= zobristRandom[next(m)+i*64];

    if(flagEnPassant){
        int delta = next(m) % 8 - prev(m) % 8;
        int epIdx = zobristIndex[Pawn | oppositeColor(p)] - 1;
        h ^= zobristRandom[(prev(m) + delta) + epIdx * 64];
    } else if (isCapture){
        h ^= zobristRandom[next(m) + (zobristIndex[captured(m)]-1)*64];
    }

    h ^= zobristRandom[12*64]; //alternates btw white and black to move

    if (isKing(p)) //update castling rights
    {
        if (isWhite(p))
        {
            if(pos->KCastle){
                h ^= zobristRandom[12*64+1];

            }
            if(pos->QCastle){
                h ^= zobristRandom[12*64+2];
            }
        }
        else
        {
            if(pos->kCastle){
                h ^= zobristRandom[12*64+3];
            }
            if(pos->qCastle){
                h ^= zobristRandom[12*64+4];
            }
        }
    }

    if (isKing(p)) { //Castling
        int j = zobristIndex[Rook | (isWhite(p) ? White : Black)] - 1;
        if (next(m) == prev(m) + 2) {
            h ^= zobristRandom[prev(m) + 3 + j * 64];
            h ^= zobristRandom[prev(m) + 1 + j * 64];
        } else if (next(m) == prev(m) - 2) {
            h ^= zobristRandom[prev(m) - 4 + j * 64];
            h ^= zobristRandom[prev(m) - 1 + j * 64];
        }
    }

    if (!isVacant(promotion(m))) { //Promotion fix (cancel pawn move and puts promoted piece instead)
        h ^= zobristRandom[next(m) + i * 64];
        int j = zobristIndex[promotion(m)] - 1;
        h ^= zobristRandom[next(m) + j * 64];
    }

    if ((prev(m) == 56 || next(m) == 56) && pos->qCastle){
        h ^= zobristRandom[12*64+4];
    }
    if ((prev(m) == 63 || next(m) == 63) && pos->kCastle){
        h ^= zobristRandom[12*64+3];
    }
    if ((prev(m) == 0 || next(m) == 0) && pos->QCastle){
        h ^= zobristRandom[12*64+2];
    }
    if ((prev(m) == 7 || next(m) == 7) && pos->KCastle){
        h ^= zobristRandom[12*64+1];
    }
    int col = prev(m)%8;
    if(isPawn(p) &&  (next(m)-prev(m) == 16 || next(m)-prev(m) == -16) &&
        ((col > 0 && pos->board[next(m)-1] == invertColor(p)) || (col < 7 && pos->board[next(m)+1] == invertColor(p))) ){
        h ^= zobristRandom[12*64+5+(col)]; // add new en Passant if any enPassant is possible
    }

    int sq = pos->enPassant;
    if(sq != -1) { // We remove the en Passant only if it was there before, that is, if it was possible before
        int epRank = sq / 8;

        int pawnSq = (epRank == 2) ? sq + 8 : sq - 8;
        int col2 = pawnSq % 8;

        int attackerColor = (epRank == 2) ? Black : White;
        int expectedPawn = Pawn | attackerColor;

        if( (col2 > 0 && pos->board[pawnSq - 1] == expectedPawn) ||
            (col2 < 7 && pos->board[pawnSq + 1] == expectedPawn) ) {
            h ^= zobristRandom[12*64 + 5 + col2];
        }
    }

    return h;
}