#ifndef ZOBRIST_H
#define ZOBRIST_H

#include "Piece.h"
#include "Move.h"
#include "Data.h"
#include "Types.h"

uint64_t computeHash(Position* pos);
uint64_t newHash(Position* pos, uint64_t m, int p);

#endif