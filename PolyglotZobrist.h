#ifndef POLYGLOT_ZOBRIST_H
#define POLYGLOT_ZOBRIST_H

#include "Piece.h"
#include "Move.h"
#include "Data.h"
#include "Types.h"

/*
 * Fixed Random64 constants and format specification:
 * https://hgm.nubati.net/book_format.html
 * The page places its sample code in the public domain. The exact origin of
 * this implementation remains uncertain; see ACKNOWLEDGMENTS.md.
 * polyglotRandom[] provides stable keys for opening books. zobristRandom[]
 * is initialized separately for the engine's transposition table.
 *
 * Layout : RandomPiece (0..767), RandomCastle (768..771),
 *          RandomEnPassant (772..779), RandomTurn (780).
 */

/*
 * Polyglot piece indices alternate Black/White for each type:
 * pawn, knight, bishop, rook, queen, king (indices 0..11).
 * Piece.h types are Pawn=1 through King=6.
 */
static inline uint64_t polyglotHash(Position* pos){
    uint64_t h = 0;

    for (int sq = 0; sq < 64; sq++){
        int p = pos->board[sq];
        if (isVacant(p)) continue;
        int type = p & PieceCache;               // 1..6
        int pivot = isWhite(p) ? 1 : 0;           // Polyglot color order: Black first, White second.
        int pieceIndex = (type - 1) * 2 + pivot;  // 0..11
        h ^= polyglotRandom[64 * pieceIndex + sq];
    }

    // Castling keys: White kingside, White queenside, Black kingside, Black queenside.
    if (pos->KCastle) h ^= polyglotRandom[768];
    if (pos->QCastle) h ^= polyglotRandom[769];
    if (pos->kCastle) h ^= polyglotRandom[770];
    if (pos->qCastle) h ^= polyglotRandom[771];

    // Hash en passant only when an adjacent pawn can capture, regardless of pins.
    if (pos->enPassant != -1){
        int sq = pos->enPassant;
        int epRank = sq / 8;
        int pawnSq = (epRank == 2) ? sq + 8 : sq - 8;
        int col2 = pawnSq % 8;
        int attackerColor = (epRank == 2) ? Black : White;
        int expectedPawn = Pawn | attackerColor;

        if ( (col2 > 0 && pos->board[pawnSq - 1] == expectedPawn) ||
             (col2 < 7 && pos->board[pawnSq + 1] == expectedPawn) ) {
            h ^= polyglotRandom[772 + col2];
        }
    }

    // Polyglot turn key is present when White is to move.
    if (pos->whiteToMove) h ^= polyglotRandom[780];

    return h;
}

static inline uint64_t computePawnHash(Position* pos) {
    uint64_t h = 0;

    // White pawn keys.
    uint64_t wPawns = pos->pieces[WHITE_INDEX][Pawn];
    int wPawnIdx = zobristIndex[Pawn | White] - 1;
    while (wPawns) {
        int sq = __builtin_ctzll(wPawns);
        wPawns &= wPawns - 1;
        h ^= zobristRandom[sq + wPawnIdx * 64];
    }

    // Black pawn keys.
    uint64_t bPawns = pos->pieces[BLACK_INDEX][Pawn];
    int bPawnIdx = zobristIndex[Pawn | Black] - 1;
    while (bPawns) {
        int sq = __builtin_ctzll(bPawns);
        bPawns &= bPawns - 1;
        h ^= zobristRandom[sq + bPawnIdx * 64];
    }

    return h;
}

#endif
