#ifndef BOT_H
#define BOT_H

#include <time.h>
#include "Board.h"

/* Optional completed-iteration observation; ordinary builds have no observer. */
#ifndef CHESSBOT_SEARCH_ITERATION
#define CHESSBOT_SEARCH_ITERATION(depth) ((void)0)
#endif

/* Historical changes and measurements are recorded in docs/VERSIONS.md. */

typedef uint64_t (*Move_t)(Position*, uint64_t timeLeft, uint64_t increment);

typedef struct Bot {
    char* name;
    TTEntry* table;
    Move_t play;
    uint64_t* pvTable;
    int* pvLength;
    uint16_t* killers;
} Bot;

static inline void swap(Position *pos, int i, int j){
    uint64_t temp = pos->moves[j];
    pos->moves[j] = pos->moves[i];
    pos->moves[i] = temp;
}

static inline bool checkStopConditions(Position* pos) {
    if (pos->stopSearch || (pos->historyPly > 0 && (pos->halfMove >= 100 || isRepetition(pos)))) return true;
    if ((pos->nodeCount++ & 2047) == 0) {
        if (get_real_time_ms() - pos->startTime > pos->timeLimit) {
            pos->stopSearch = true;
            return true;
        }
    }
    return false;
}

static inline bool checkTimeOnly(Position* pos) {
    if (pos->stopSearch) return true;
    if ((pos->nodeCount++ & 2047) == 0) {
        if (get_real_time_ms() - pos->startTime > pos->timeLimit) {
            pos->stopSearch = true;
            return true;
        }
    }
    return false;
}

static inline void updatePV(uint64_t* pvTable, int* pvLength, int ply, uint64_t m) {
    pvTable[ply * MAX_DEPTH] = m;
    memcpy(&pvTable[ply * MAX_DEPTH + 1], &pvTable[(ply + 1) * MAX_DEPTH], pvLength[ply + 1] * sizeof(uint64_t));
    pvLength[ply] = pvLength[ply + 1] + 1;
}

static inline int findOpeningIndex(uint64_t currHash){
    int a = 0;
    int b = bookSize - 1;
    int res = -1;
    while(a <= b){
        int m = (a+b)/2;
        if(bookEntries[m].key == currHash){
            res = m;
            b = m-1;
        } else if (bookEntries[m].key < currHash){
            a = m + 1;
        } else {
            b = m-1;
        }
    }
    return res;
}

static inline uint64_t makeFromOpeningMove(Position *pos){
    if(!bookEntries || bookSize == 0) return 0;

    uint64_t polyHash = polyglotHash(pos);
    int i = findOpeningIndex(polyHash);
    if(i < 0 || i >= bookSize) return 0;

    long j = i;
    long count = 0;
    PolyglotEntry options[16];
    uint32_t totalWeight = 0;

    while (j < bookSize && bookEntries[j].key == polyHash && count < 16) {
        options[count] = bookEntries[j];
        totalWeight += bookEntries[j].weight;
        count++;
        j++;
    }

    if (count == 0 || totalWeight == 0) return 0;

    uint32_t randomPoint = (uint32_t)rand() % totalWeight;
    PolyglotEntry selectedEntry;
    uint32_t currentSum = 0;

    for (long i = 0; i < count; i++) {
        currentSum += options[i].weight;
        if (randomPoint < currentSum) {
            selectedEntry = options[i]; //putting randomisation to ensure variety
            break;
        }
    }
    assert(randomPoint < currentSum);
    uint16_t smove = selectedEntry.move;

    int bookTo   = smove & 0x3F;
    int bookFrom = (smove >> 6) & 0x3F;
    int promPiece = (smove >> 12) & PieceCache;
    if (promPiece != 0) {
        promPiece++; //standard encoding : Knight = 1, Bishop = 2, Rook = 3, Queen = 4 -> add one to get our pieceCodes
    }

    generateMoves(pos);
    int totalMoves = pos->moves[256 * pos->currentDepth];

    for (int i = 1; i <= totalMoves; i++) {
        uint64_t m = pos->moves[i + pos->currentDepth * 256];
        if (prev(m) == bookFrom && next(m) == bookTo && (promotion(m) & PieceCache) == promPiece) {
            return m; // We found a book move !
        }
    }
    return 0;

}

static inline bool promoteSmallMove(Position* pos, uint16_t small_move,int destinationIndex,int last){
    for(int i = destinationIndex; i <= last; i++){
        uint64_t m = pos->moves[i];
        if(toSmallMove(m) == small_move){
            swap(pos,destinationIndex,i);
            return true;
        }
    }
    return false;
}

static inline bool promoteFullMove(Position* pos, uint64_t mv,int destinationIndex,int last){
    return promoteSmallMove(pos,toSmallMove(mv),destinationIndex,last);
}

static inline void insertionSort(Position *pos,int startIndex, int moveCount){ // Move slots begin at startIndex + 1; startIndex stores their count.
    for(int i = 2; i <= moveCount; i++){
        uint64_t m = pos->moves[startIndex + i];
        int j = i-1;
        while(j > 0 && m > pos->moves[startIndex+j]){
            pos->moves[startIndex + j + 1] = pos->moves[startIndex + j];
            j--;
        }
        pos->moves[startIndex + j + 1] = m;
    }
}

static inline void sortCaptures(Position *pos,int startIndex, int moveCount, int seeScores[256]){ // Move slots begin at startIndex + 1; startIndex stores their count.
    for(int i = 2; i <= moveCount; i++){
        uint64_t m = pos->moves[startIndex + i];
        int s = seeScores[i];
        int j = i-1;
    while(j > 0 && (s > seeScores[j] || (s == seeScores[j] && m > pos->moves[startIndex+j]))) {
            pos->moves[startIndex + j + 1] = pos->moves[startIndex + j];
            seeScores[j+1] = seeScores[j];
            j--;
        }
        pos->moves[startIndex + j + 1] = m;
        seeScores[j+1] = s;
    }
}

static inline int orderCapturesQS(Position *pos, int startIndex, int moveCount, uint64_t pvMove, uint16_t ttMove) {
    int offset = 0;
    if (pvMove != 0 && promoteSmallMove(pos, toSmallMove(pvMove), startIndex + 1, startIndex + moveCount)) offset++;
    if (ttMove != 0 && ttMove != toSmallMove(pvMove) && promoteSmallMove(pos, ttMove, startIndex + 1 + offset, startIndex + moveCount)) offset++;

    // Sort ALL captures by MVV-LVA.
    // Good captures (score 121+) naturally sort above bad captures (score 17-80) thanks to the +100 bonus in the precomputed data.

    insertionSort(pos, startIndex + offset, moveCount - offset);

    return moveCount;
}

static inline uint64_t getAllAttackers(Position* pos, int sq, uint64_t occupied, uint64_t bishops, uint64_t rooks) {
    return (pawnAttacks[BLACK_INDEX][sq] & pos->pieces[WHITE_INDEX][Pawn]) | //reverse lookup to see if white pawns can attacks sq
           (pawnAttacks[WHITE_INDEX][sq] & pos->pieces[BLACK_INDEX][Pawn]) | //same with opposite colors
           (knightAttacks[sq] & (pos->pieces[WHITE_INDEX][Knight] | pos->pieces[BLACK_INDEX][Knight])) |
           (kingAttacks[sq]   & (pos->pieces[WHITE_INDEX][King]   | pos->pieces[BLACK_INDEX][King]))   |
           (bishopAttacksFast(sq, occupied) & bishops) |
           (rookAttacksFast(sq, occupied)   & rooks);
}

static inline int getLeastValuableAttacker(Position* pos, uint64_t myAttackers, int side, int* attackerSq) {
    uint64_t bb = myAttackers & pos->pieces[side][Pawn];
    if (bb) {
        *attackerSq = __builtin_ctzll(bb);
        return Pawn;
    }

    bb = myAttackers & pos->pieces[side][Knight];
    if (bb) {
        *attackerSq = __builtin_ctzll(bb);
        return Knight;
    }

    bb = myAttackers & pos->pieces[side][Bishop];
    if (bb) {
        *attackerSq = __builtin_ctzll(bb);
        return Bishop;
    }

    bb = myAttackers & pos->pieces[side][Rook];
    if (bb) {
        *attackerSq = __builtin_ctzll(bb);
        return Rook;
    }

    bb = myAttackers & pos->pieces[side][Queen];
    if (bb) {
        *attackerSq = __builtin_ctzll(bb);
        return Queen;
    }

    bb = myAttackers & pos->pieces[side][King];
    if (bb) {
        *attackerSq = __builtin_ctzll(bb);
        return King;
    }

    return Vacant;
}

static inline int see(Position* pos, uint64_t m) {
    int from = prev(m);
    int to = next(m);
    int cap = captured(m);
    int prom = promotion(m);
    bool flagEP = isEnPassant(m);

    int gain[32];
    int d = 0;

    if (prom != Vacant) {
        gain[0] = pieceValue(cap) + pieceValue(prom) - PAWN_VALUE;
    } else {
        gain[0] = pieceValue(cap);
    }

    int victim = (prom != Vacant) ? uncolor(prom) : uncolor(pos->board[from]);
    int side = oppositeColorIndex(pos->board[from]);

    // Remove the initial attacker from the occupied squares.
    uint64_t occupied = pos->occupied ^ (1ULL << from);
    if (flagEP) {
        int delta = to % 8 - from % 8;
        occupied ^= (1ULL << (from + delta));
    }

    // Include queens in both sliding-attacker masks for x-rays.
    uint64_t bishops = pos->pieces[WHITE_INDEX][Bishop] | pos->pieces[BLACK_INDEX][Bishop] |
                       pos->pieces[WHITE_INDEX][Queen]  | pos->pieces[BLACK_INDEX][Queen];
    uint64_t rooks   = pos->pieces[WHITE_INDEX][Rook]   | pos->pieces[BLACK_INDEX][Rook]   |
                       pos->pieces[WHITE_INDEX][Queen]  | pos->pieces[BLACK_INDEX][Queen];

    uint64_t attackers = getAllAttackers(pos, to, occupied, bishops, rooks) & occupied;

    while (d<31) {
        d++;
        gain[d] = pieceValue(victim) - gain[d - 1];
        if (gain[d] < 0) break;

        uint64_t myAttackers = attackers & pos->byColor[side];
        if (!myAttackers) break;

        int nextSq = -1;
        int nextPiece = getLeastValuableAttacker(pos, myAttackers, side, &nextSq);
        if (nextPiece == Vacant || nextSq < 0) break;

        // A king cannot recapture onto a defended square.
        if (nextPiece == King && (attackers & pos->byColor[side ^ 1])) {
            break;
        }

        victim = nextPiece;
        occupied ^= (1ULL << nextSq);

        // Reveal sliding attackers after removing the previous attacker.
        attackers |= (bishopAttacksFast(to, occupied) & bishops) | (rookAttacksFast(to, occupied) & rooks);
        attackers &= occupied;

        side ^= 1;
    }

    // Propagate exchange gains backward with negamax.
    while (--d > 0) {
        int opp = -gain[d];
        if (opp < gain[d - 1]) {
            gain[d - 1] = opp;
        }
    }

    return gain[0];
}

#endif
