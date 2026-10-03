#ifndef DATA_H
#define DATA_H

#include <stdint.h>
#include <stdbool.h>

extern const int mg_value[7];
extern const int eg_value[7];
extern const int phase_value[7];
extern const int mg_passed_bonus[8];
extern const int eg_passed_bonus[8];

extern const uint64_t adjacentFilesMask[8];
extern const uint64_t fileMask[8];

extern const int mg_pawn_table[64];
extern const int eg_pawn_table[64];
extern const int mg_knight_table[64];
extern const int eg_knight_table[64];
extern const int mg_bishop_table[64];
extern const int eg_bishop_table[64];
extern const int mg_rook_table[64];
extern const int eg_rook_table[64];
extern const int mg_queen_table[64];
extern const int eg_queen_table[64];
extern const int mg_king_table[64];
extern const int eg_king_table[64];

extern const uint64_t passedPawnMask[2][64];

extern const int* const mg_pst[7];
extern const int* const eg_pst[7];

extern const int DIRECTION_OFFSETS[8];
extern const int dirs[16];
extern const int zobristIndex[17];

extern const int bishopLengths[64];
extern const int rookLengths[64];

extern const int bishopOffsets[64];
extern const int rookOffsets[64];

extern const uint64_t bishopMasks[64];
extern const uint64_t rookMasks[64];

extern const uint64_t bishopMagic[64];
extern const uint64_t rookMagic[64];

extern const uint64_t kingAttacks[64];
extern const uint64_t knightAttacks[64];

//Carefull ! pawnAttacks has attacker's side convention
//ie, pawnAttacks[BLACK_INDEX][i] will show squares on the row below, the squares a black pawn on i would attack
//We keep this convention even on rows 1 and 8 to allow for a reverse lookup (more efficient)
extern const uint64_t pawnAttacks[2][64];

extern const uint64_t mvScore[256];
extern const int numSquaresToEdge[512];

extern const uint64_t polyglotRandom[781];
extern const uint64_t zobristRandom[781];

extern const int lmrTable[65*256];

extern const uint64_t bishopTable[5248];
extern const uint64_t rookTable[102400];

static inline uint64_t bishopAttacksFast(int square, uint64_t occupied){
    int index = ((occupied & bishopMasks[square]) * bishopMagic[square]) >> (64 - bishopLengths[square]);
    return bishopTable[bishopOffsets[square] + index];
}

static inline uint64_t rookAttacksFast(int square, uint64_t occupied){
    int index = ((occupied & rookMasks[square]) * rookMagic[square]) >> (64 - rookLengths[square]);
    return rookTable[rookOffsets[square] + index];
}

#endif
