#include "Eval.h"

/* Evaluation implemented by the author, informed by CPW's PeSTO example.
 * See ACKNOWLEDGMENTS.md for source attribution.
 */

#define PAWN_TT_SIZE 16384
#define MG_ISOLATED_PENALTY 12
#define EG_ISOLATED_PENALTY 18
#define MG_DOUBLED_PENALTY  5
#define EG_DOUBLED_PENALTY  8
#define SAFETY_TABLE_MAX ((int)(sizeof(safetyTable) / sizeof(safetyTable[0])) - 1)

static const int MOBILITY_MG[7] = { 0, 0, 2, 1, 1, 1, 0 };
static const int MOBILITY_EG[7] = { 0, 0, 2, 2, 1, 1, 0 };
static const int MOBILITY_BASE[7] = { 0, 0, 2, 3, 3, 4, 0 };
static const int ATTACK_WEIGHT[7] = { 0, 0, 2, 2, 3, 5, 0 };
// Nonlinear penalties for weighted attacks on the king zone.
static const int safetyTable[] = {
      0,   0,   1,   2,   3,   5,   7,   9,  12,  15,
     18,  22,  26,  31,  37,  43,  50,  58,  67,  77,
     88, 100, 115, 130, 150, 170, 195, 220, 250, 280,
    315, 350, 390, 430, 475, 520, 570, 620, 675, 730,
    790, 850, 920, 1000
};

__thread PawnEntry pawnTable[PAWN_TT_SIZE];

static inline int pstMirror(int sq){ return sq ^ 56; }

/*
 * Recompute material/PST scores from the piece bitboards, blend by phase,
 * and return the score from the side-to-move perspective.
 */
int tapered_eval(Position* pos){
    int mgW = 0, mgB = 0, egW = 0, egB = 0, phase = 0;

    for(int p = Pawn; p <= King; p++){
        uint64_t bb = pos->pieces[WHITE_INDEX][p];
        while(bb){
            int sq = __builtin_ctzll(bb);
            bb &= bb-1;
            int flipped = pstMirror(sq);
            mgW += mg_value[p] + mg_pst[p][flipped];
            egW += eg_value[p] + eg_pst[p][flipped];
            phase += phase_value[p];
        }
        bb = pos->pieces[BLACK_INDEX][p];
        while(bb){
            int sq = __builtin_ctzll(bb);
            bb &= bb-1;
            mgB += mg_value[p] + mg_pst[p][sq];
            egB += eg_value[p] + eg_pst[p][sq];
            phase += phase_value[p];
        }
    }

    if(phase > 24) phase = 24;

    int mgScore = mgW - mgB;
    int egScore = egW - egB;
    int score = (mgScore * phase + egScore * (24 - phase)) / 24;

    return pos->whiteToMove ? score : -score;
}

int ancientEvalCompute(Position *pos){
    int res = 0;
    for(int p = Pawn; p <= King; p++){
        int nbW = __builtin_popcountll(pos->pieces[WHITE_INDEX][p]);
        int nbB = __builtin_popcountll(pos->pieces[BLACK_INDEX][p]);
        res +=  (nbW - nbB) * pieceValue(p);
    }
    return res;
}

void recomputeEvaluation(Position* pos){
    int mgW = 0, mgB = 0, egW = 0, egB = 0, phase = 0;
    for(int p = Pawn; p <= King; p++){
        uint64_t bb = pos->pieces[WHITE_INDEX][p];
        while(bb){
            int sq = __builtin_ctzll(bb);
            bb &= bb - 1;
            int mirror = sq ^ 56;
            mgW += mg_value[p] + mg_pst[p][mirror];
            egW += eg_value[p] + eg_pst[p][mirror];
            phase += phase_value[p];
        }
        bb = pos->pieces[BLACK_INDEX][p];
        while(bb){
            int sq = __builtin_ctzll(bb);
            bb &= bb - 1;
            mgB += mg_value[p] + mg_pst[p][sq];
            egB += eg_value[p] + eg_pst[p][sq];
            phase += phase_value[p];
        }
    }
    pos->mgEval = mgW - mgB;
    pos->egEval = egW - egB;
    pos->phase = phase;
}

void applyMoveEvaluation(Position *pos,uint64_t m){
    int deltaPhase = 0;
    int mgDelta = 0;
    int egDelta = 0;

    int p = pos->board[prev(m)];
    int mirror = (pos->whiteToMove) ? 56 : 0;
    bool isCapture = !isVacant(captured(m));
    bool flagEnPassant = isEnPassant(m);
    bool isPromotion = !isVacant(promotion(m));

    mgDelta += mg_pst[p&PieceCache][next(m)^mirror] -  mg_pst[p&PieceCache][prev(m)^mirror];
    egDelta += eg_pst[p&PieceCache][next(m)^mirror] -  eg_pst[p&PieceCache][prev(m)^mirror];

    if(isPromotion){
        deltaPhase += phase_value[promotion(m) & PieceCache];
        mgDelta += mg_value[promotion(m) & PieceCache] - mg_value[Pawn];
        mgDelta += mg_pst[promotion(m) & PieceCache][next(m)^mirror] - mg_pst[Pawn][next(m)^mirror];
        egDelta += eg_value[promotion(m) & PieceCache] - eg_value[Pawn];
        egDelta += eg_pst[promotion(m) & PieceCache][next(m)^mirror] - eg_pst[Pawn][next(m)^mirror];
    }
    if(flagEnPassant){
        int delta = next(m) % 8 - prev(m) % 8;
        deltaPhase -= phase_value[Pawn];
        mgDelta += mg_value[Pawn];
        mgDelta += mg_pst[Pawn][(prev(m)+delta)^(56-mirror)];
        egDelta += eg_value[Pawn];
        egDelta += eg_pst[Pawn][(prev(m)+delta)^(56-mirror)];
    } else if(isCapture){
        deltaPhase -= phase_value[captured(m) & PieceCache];
        mgDelta += mg_value[captured(m) & PieceCache];
        mgDelta += mg_pst[captured(m) & PieceCache][next(m)^(56-mirror)];
        egDelta += eg_value[captured(m) & PieceCache];
        egDelta += eg_pst[captured(m) & PieceCache][next(m)^(56-mirror)];
    }

    else if (isKing(p)){
        if (next(m) == prev(m) + 2){
            mgDelta += mg_pst[Rook][(next(m)-1)^mirror] -  mg_pst[Rook][(next(m)+1)^mirror];
            egDelta += eg_pst[Rook][(next(m)-1)^mirror] -  eg_pst[Rook][(next(m)+1)^mirror];
        }
        else if (next(m) == prev(m) - 2){
            mgDelta += mg_pst[Rook][(next(m)+1)^mirror] -  mg_pst[Rook][(next(m)-2)^mirror];
            egDelta += eg_pst[Rook][(next(m)+1)^mirror] -  eg_pst[Rook][(next(m)-2)^mirror];
        }
    }

    pos->phase += deltaPhase;
    if(!pos->whiteToMove){ mgDelta = -mgDelta ; egDelta = -egDelta;}
    pos->mgEval += mgDelta;
    pos->egEval += egDelta;
}

void undoMoveEvaluation(Position *pos,uint64_t m){
    int deltaPhase = 0;
    int mgDelta = 0;
    int egDelta = 0;

    int p = pos->board[next(m)];
    int mirror = (pos->whiteToMove) ? 0 : 56;
    bool isCapture = !isVacant(captured(m));
    bool flagEnPassant = isEnPassant(m);
    bool isPromotion = !isVacant(promotion(m));

    int q = isPromotion ? Pawn : (p&PieceCache);

    mgDelta -= mg_pst[q][next(m)^mirror] -  mg_pst[q][prev(m)^mirror];
    egDelta -= eg_pst[q][next(m)^mirror] -  eg_pst[q][prev(m)^mirror];

    if(isPromotion){
        deltaPhase -= phase_value[promotion(m) & PieceCache];
        mgDelta -= mg_value[promotion(m) & PieceCache] - mg_value[Pawn];
        mgDelta -= mg_pst[promotion(m) & PieceCache][next(m)^mirror] - mg_pst[Pawn][next(m)^mirror];
        egDelta -= eg_value[promotion(m) & PieceCache] - eg_value[Pawn];
        egDelta -= eg_pst[promotion(m) & PieceCache][next(m)^mirror] - eg_pst[Pawn][next(m)^mirror];
    }
    if(flagEnPassant){
        int delta = next(m) % 8 - prev(m) % 8;
        deltaPhase += phase_value[Pawn];
        mgDelta -= mg_value[Pawn];
        mgDelta -= mg_pst[Pawn][(prev(m)+delta)^(56-mirror)];
        egDelta -= eg_value[Pawn];
        egDelta -= eg_pst[Pawn][(prev(m)+delta)^(56-mirror)];
    } else if(isCapture){
        deltaPhase += phase_value[captured(m) & PieceCache];
        mgDelta -= mg_value[captured(m) & PieceCache];
        mgDelta -= mg_pst[captured(m) & PieceCache][next(m)^(56-mirror)];
        egDelta -= eg_value[captured(m) & PieceCache];
        egDelta -= eg_pst[captured(m) & PieceCache][next(m)^(56-mirror)];
    } else if (isKing(p)){
        if (next(m) == prev(m) + 2){
            mgDelta -= mg_pst[Rook][(next(m)-1)^mirror] -  mg_pst[Rook][(next(m)+1)^mirror];
            egDelta -= eg_pst[Rook][(next(m)-1)^mirror] -  eg_pst[Rook][(next(m)+1)^mirror];
        }
        else if (next(m) == prev(m) - 2){
            mgDelta -= mg_pst[Rook][(next(m)+1)^mirror] -  mg_pst[Rook][(next(m)-2)^mirror];
            egDelta -= eg_pst[Rook][(next(m)+1)^mirror] -  eg_pst[Rook][(next(m)-2)^mirror];
        }
    }

    pos->phase += deltaPhase;
    if(pos->whiteToMove){ mgDelta = -mgDelta ; egDelta = -egDelta;}
    pos->mgEval += mgDelta;
    pos->egEval += egDelta;
}

int staticEval(Position * pos){
    int phase = pos->phase;
    if(phase > 24) phase = 24;
    int res = (phase * pos->mgEval + (24-phase) * pos->egEval)/24;
    if(!pos->whiteToMove) res = -res;
    return res;
}

static inline bool isIsolated(Position* pos, int sq, int color) {
    int file = sq % 8;
    return (pos->pieces[color][Pawn] & adjacentFilesMask[file]) == 0;
}

static inline bool isDoubled(Position* pos, int sq, int color) {
    int file = sq % 8;
    // More than one pawn on the file
    return __builtin_popcountll(pos->pieces[color][Pawn] & fileMask[file]) > 1;
}

static void evaluatePawnStructure(Position* pos, int* mgScore, int* egScore) {
    int mgW = 0, egW = 0;
    int mgB = 0, egB = 0;

    // White pawns.
    uint64_t wPawns = pos->pieces[WHITE_INDEX][Pawn];
    while (wPawns) {
        int sq = __builtin_ctzll(wPawns);
        wPawns &= wPawns - 1;
        int rank = sq / 8;

        // Passed pawn
        if (!(pos->pieces[BLACK_INDEX][Pawn] & passedPawnMask[WHITE_INDEX][sq])) {
            mgW += mg_passed_bonus[rank];
            egW += eg_passed_bonus[rank];
        }
        // Isolated pawn
        if (isIsolated(pos, sq, WHITE_INDEX)) {
            mgW -= MG_ISOLATED_PENALTY;
            egW -= EG_ISOLATED_PENALTY;
        }
        // Doubled pawn
        if (isDoubled(pos, sq, WHITE_INDEX)) {
            mgW -= MG_DOUBLED_PENALTY;
            egW -= EG_DOUBLED_PENALTY;
        }
    }

    // --- Black pawns ---
    uint64_t bPawns = pos->pieces[BLACK_INDEX][Pawn];
    while (bPawns) {
        int sq = __builtin_ctzll(bPawns);
        bPawns &= bPawns - 1;
        int rank = 7 - (sq / 8);

        // Passed pawn
        if (!(pos->pieces[WHITE_INDEX][Pawn] & passedPawnMask[BLACK_INDEX][sq])) {
            mgB += mg_passed_bonus[rank];
            egB += eg_passed_bonus[rank];
        }
        // Isolated pawn
        if (isIsolated(pos, sq, BLACK_INDEX)) {
            mgB -= MG_ISOLATED_PENALTY;
            egB -= EG_ISOLATED_PENALTY;
        }
        // Doubled pawn
        if (isDoubled(pos, sq, BLACK_INDEX)) {
            mgB -= MG_DOUBLED_PENALTY;
            egB -= EG_DOUBLED_PENALTY;
        }
    }

    *mgScore = mgW - mgB;
    *egScore = egW - egB;
}

int evalQS(Position *pos){
   int phase = pos->phase;
    if (phase > 24) phase = 24;
    int mg = pos->mgEval;
    int eg = pos->egEval;

    // --- PAWN HASH TABLE LOOKUP ---
    int idx = pos->pawnKey & (PAWN_TT_SIZE - 1);
    PawnEntry* entry = &pawnTable[idx];

    int pawnMg = 0;
    int pawnEg = 0;
    if (entry->key == pos->pawnKey) {
        // Reuse the cached pawn evaluation
        pawnMg = entry->mgScore;
        pawnEg = entry->egScore;
    }
    mg += pawnMg;
    eg += pawnEg;

    int res = (phase * mg + (24 - phase) * eg) / 24;
    return pos->whiteToMove ? res : -res;
}

int eval(Position* pos) {
    int phase = pos->phase;
    if (phase > 24) phase = 24;
    int mg = pos->mgEval;
    int eg = pos->egEval;

    // --- PAWN HASH TABLE LOOKUP ---
    int idx = pos->pawnKey & (PAWN_TT_SIZE - 1);
    PawnEntry* entry = &pawnTable[idx];

    int pawnMg, pawnEg;
    if (entry->key == pos->pawnKey) {
        // Reuse the cached pawn evaluation
        pawnMg = entry->mgScore;
        pawnEg = entry->egScore;
    } else {
        // Cache miss: recompute from bitboards
        evaluatePawnStructure(pos, &pawnMg, &pawnEg);
        entry->key = pos->pawnKey;
        entry->mgScore = (int16_t)pawnMg;
        entry->egScore = (int16_t)pawnEg;
    }

    mg += pawnMg;
    eg += pawnEg;

    if (__builtin_popcountll(pos->pieces[WHITE_INDEX][Bishop]) >= 2) {
        mg += 30; eg += 45;
    }
    if (__builtin_popcountll(pos->pieces[BLACK_INDEX][Bishop]) >= 2) {
        mg -= 30; eg -= 45;
    }

    // --- Tapered blend ---
    int res = (phase * mg + (24 - phase) * eg) / 24;
    return pos->whiteToMove ? res : -res;
}

void evaluateMobility(Position* pos, int* mgScore, int* egScore) {
    int mgW = 0, egW = 0;
    int mgB = 0, egB = 0;
    uint64_t occ = pos->occupied;

    // --- White ---
    for (int p = Knight; p <= Queen; p++) {
        uint64_t bb = pos->pieces[WHITE_INDEX][p];
        while (bb) {
            int sq = __builtin_ctzll(bb);
            bb &= bb - 1;

            uint64_t attacks = 0;
            if (p == Knight)      attacks = knightAttacks[sq];
            else if (p == Bishop) attacks = bishopAttacksFast(sq, occ);
            else if (p == Rook)   attacks = rookAttacksFast(sq, occ);
            else if (p == Queen)  attacks = bishopAttacksFast(sq, occ) | rookAttacksFast(sq, occ);

            int mob = __builtin_popcountll(attacks & ~pos->byColor[WHITE_INDEX]);
            int adj = mob - MOBILITY_BASE[p];
            if (adj < 0) adj = 0;
            mgW += MOBILITY_MG[p] * adj;
            egW += MOBILITY_EG[p] * adj;
        }
    }

    // --- Black ---
    for (int p = Knight; p <= Queen; p++) {
        uint64_t bb = pos->pieces[BLACK_INDEX][p];
        while (bb) {
            int sq = __builtin_ctzll(bb);
            bb &= bb - 1;

            uint64_t attacks = 0;
            if (p == Knight)      attacks = knightAttacks[sq];
            else if (p == Bishop) attacks = bishopAttacksFast(sq, occ);
            else if (p == Rook)   attacks = rookAttacksFast(sq, occ);
            else if (p == Queen)  attacks = bishopAttacksFast(sq, occ) | rookAttacksFast(sq, occ);

            int mob = __builtin_popcountll(attacks & ~pos->byColor[BLACK_INDEX]);
            int adj = mob - MOBILITY_BASE[p];
            if (adj < 0) adj = 0;
            mgB += MOBILITY_MG[p] * adj;
            egB += MOBILITY_EG[p] * adj;
        }
    }

    *mgScore = mgW - mgB;
    *egScore = egW - egB;
}

static int evaluateKingSafety(Position* pos, int side) {
    int enemy = side ^ 1;

    // Skip attacking pressure when the opponent has no queen
    if (pos->pieces[enemy][Queen] == 0) return 0;

    int kSq = (side == WHITE_INDEX) ? pos->KSquare : pos->kSquare;
    uint64_t kingZone = kingAttacks[kSq] | (1ULL << kSq);

    // Extend one rank forward; 64-bit shifts discard off-board ranks
    if (side == WHITE_INDEX) kingZone |= (kingZone << 8);
    else                     kingZone |= (kingZone >> 8);

    int attackUnits = 0;
    int attackersCount = 0;
    uint64_t occ = pos->occupied;

    // 1. Count enemy attackers in the king zone
    for (int p = Knight; p <= Queen; p++) {
        uint64_t bb = pos->pieces[enemy][p];
        while (bb) {
            int sq = __builtin_ctzll(bb);
            bb &= bb - 1;

            uint64_t attacks = 0;
            if (p == Knight)      attacks = knightAttacks[sq];
            else if (p == Bishop) attacks = bishopAttacksFast(sq, occ);
            else if (p == Rook)   attacks = rookAttacksFast(sq, occ);
            else if (p == Queen)  attacks = bishopAttacksFast(sq, occ) | rookAttacksFast(sq, occ);

            uint64_t hits = attacks & kingZone;
            if (hits) {
                attackersCount++;
                attackUnits += ATTACK_WEIGHT[p] * __builtin_popcountll(hits);
            }
        }
    }

    // Apply attacking pressure only with at least two distinct attackers
    int danger = 0;
    if (attackersCount >= 2) {
        if (attackUnits > SAFETY_TABLE_MAX) attackUnits = SAFETY_TABLE_MAX;
        danger = safetyTable[attackUnits];
    }

    // 2. Pawn shield for a king on a flank and its back ranks
    int shieldPenalty = 0;
    int kRank = kSq / 8;
    int kFile = kSq % 8;

    bool whiteShieldActive = (side == WHITE_INDEX && kRank <= 1 && (kFile <= 2 || kFile >= 5));
    bool blackShieldActive = (side == BLACK_INDEX && kRank >= 6 && (kFile <= 2 || kFile >= 5));

    if (whiteShieldActive || blackShieldActive) {
        int startFile = (kFile <= 2) ? 0 : 5;
        int endFile   = (kFile <= 2) ? 2 : 7;

        for (int f = startFile; f <= endFile; f++) {
            uint64_t filePawns = pos->pieces[side][Pawn] & fileMask[f];
            if (!filePawns) {
                shieldPenalty += 25; // Open file in front of the king
            } else {
                if (side == WHITE_INDEX) {
                    int pawnSq = __builtin_ctzll(filePawns);
                    int rank = pawnSq / 8;
                    if (rank == 2)      shieldPenalty += 10; // White pawn on the third rank
                    else if (rank >= 3) shieldPenalty += 20; // White pawn on the fourth rank or beyond
                } else {
                    int pawnSq = 63 - __builtin_clzll(filePawns);
                    int rank = pawnSq / 8;
                    if (rank == 5)      shieldPenalty += 10; // Black pawn on the sixth rank
                    else if (rank <= 4) shieldPenalty += 20; // Black pawn on the fifth rank or beyond
                }
            }
        }
    }

    return -(danger + shieldPenalty);
}

static void evaluateRooks(Position* pos, int* mgScore, int* egScore) {
    int mgW = 0, egW = 0;
    int mgB = 0, egB = 0;

    // --- White rooks ---
    uint64_t wRooks = pos->pieces[WHITE_INDEX][Rook];
    while (wRooks) {
        int sq = __builtin_ctzll(wRooks);
        wRooks &= wRooks - 1;

        int file = sq % 8;
        int rank = sq / 8;
        uint64_t fMask = fileMask[file];

        bool hasFriendlyPawn = (pos->pieces[WHITE_INDEX][Pawn] & fMask) != 0;
        bool hasEnemyPawn    = (pos->pieces[BLACK_INDEX][Pawn] & fMask) != 0;

        // Open / semi-open file
        if (!hasFriendlyPawn) {
            if (!hasEnemyPawn) {
                mgW += 20; egW += 15;
            } else {
                mgW += 10; egW += 8;
            }
        }

        // Seventh rank (index 6)
        if (rank == 6) {
            int oppKingRank = pos->kSquare / 8;
            bool targetsPawns = (pos->pieces[BLACK_INDEX][Pawn] & (0xFFULL << 48)) != 0;

            if (oppKingRank == 7 || targetsPawns) {
                mgW += 35; egW += 50;
            } else {
                mgW += 20; egW += 30;
            }
        }
    }

    // --- Black rooks ---
    uint64_t bRooks = pos->pieces[BLACK_INDEX][Rook];
    while (bRooks) {
        int sq = __builtin_ctzll(bRooks);
        bRooks &= bRooks - 1;

        int file = sq % 8;
        int rank = sq / 8;
        uint64_t fMask = fileMask[file];

        bool hasFriendlyPawn = (pos->pieces[BLACK_INDEX][Pawn] & fMask) != 0;
        bool hasEnemyPawn    = (pos->pieces[WHITE_INDEX][Pawn] & fMask) != 0;

        // Open / semi-open file
        if (!hasFriendlyPawn) {
            if (!hasEnemyPawn) {
                mgB += 20; egB += 15;
            } else {
                mgB += 10; egB += 8;
            }
        }

        // Black seventh rank (index 1)
        if (rank == 1) {
            int oppKingRank = pos->KSquare / 8;
            bool targetsPawns = (pos->pieces[WHITE_INDEX][Pawn] & (0xFFULL << 8)) != 0;

            if (oppKingRank == 0 || targetsPawns) {
                mgB += 35; egB += 50;
            } else {
                mgB += 20; egB += 30;
            }
        }
    }

    *mgScore = mgW - mgB;
    *egScore = egW - egB;
}

int evalNew(Position* pos) {
    int phase = pos->phase;
    if (phase > 24) phase = 24;
    int mg = pos->mgEval;
    int eg = pos->egEval;

    // 1. Pawn structure with a pawn cache
    int idx = pos->pawnKey & (PAWN_TT_SIZE - 1);
    PawnEntry* entry = &pawnTable[idx];
    int pawnMg, pawnEg;
    if (entry->key == pos->pawnKey) {
        pawnMg = entry->mgScore;
        pawnEg = entry->egScore;
    } else {
        evaluatePawnStructure(pos, &pawnMg, &pawnEg);
        entry->key = pos->pawnKey;
        entry->mgScore = (int16_t)pawnMg;
        entry->egScore = (int16_t)pawnEg;
    }
    mg += pawnMg;
    eg += pawnEg;

    // 2. Rook activity
    int rookMg, rookEg;
    evaluateRooks(pos, &rookMg, &rookEg);
    mg += rookMg;
    eg += rookEg;

    // 3. Bishop pair
    if (__builtin_popcountll(pos->pieces[WHITE_INDEX][Bishop]) >= 2) {
        mg += 30; eg += 45;
    }
    if (__builtin_popcountll(pos->pieces[BLACK_INDEX][Bishop]) >= 2) {
        mg -= 30; eg -= 45;
    }

    // 4. King safety (middlegame only)
    int whiteSafety = evaluateKingSafety(pos, WHITE_INDEX);
    int blackSafety = evaluateKingSafety(pos, BLACK_INDEX);
    mg += (whiteSafety - blackSafety);

    // 5. Blend by game phase
    int res = (phase * mg + (24 - phase) * eg) / 24;
    return pos->whiteToMove ? res : -res;
}

int evalNewWhite(Position* pos){
    if(pos->whiteToMove) return evalNew(pos);
    return -evalNew(pos);
}
