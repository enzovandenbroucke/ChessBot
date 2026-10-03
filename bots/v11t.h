#include "../Bot.h"
#include <assert.h>

#define NULL_MOVE_CONST11 3
#define HISTORY_MAX11 16384
#define HIST_BONUS_MAX11 96
#define ASPIRATION_WINDOW11 50
#define DELTA_PRUNING11 200

__thread int ttCutoffCount11 = 0;
__thread uint64_t thread_pvTable11[MAX_DEPTH*(MAX_DEPTH+1)];
__thread int thread_pvLength11[MAX_DEPTH+1];
__thread uint16_t thread_killers11[MAX_DEPTH*2];
__thread bool inBook11 = true;
__thread int historyTable11[2][64][64]; // [color][from][to]
extern __thread TTEntry* tt11;
Bot v11t;

static inline void storeKiller11(int ply, uint64_t m){
    uint16_t mv = toSmallMove(m);
    if(isVacant(captured(m)) && thread_killers11[2*ply] != mv){
        thread_killers11[2*ply+1] = thread_killers11[2*ply];
        thread_killers11[2*ply] = mv;
    }
}

void orderMoves11(Position* pos, int startIndex, int moveCount, uint64_t pvMove, uint16_t ttMove) {
    // 1. Promote PV and TT
    if (pvMove != 0 && promoteFullMove(pos, pvMove, startIndex + 1, startIndex + moveCount)) {
        startIndex++; moveCount--;
    }
    if (ttMove != 0 && ttMove != toSmallMove(pvMove) && promoteSmallMove(pos, ttMove, startIndex + 1, startIndex + moveCount)) {
        startIndex++; moveCount--;
    }

    int ply = pos->currentDepth;
    uint16_t killerMove1 = thread_killers11[2 * ply];
    uint16_t killerMove2 = thread_killers11[2 * ply + 1];
    bool newKiller1 = killerMove1 != 0 && killerMove1 != toSmallMove(pvMove) && killerMove1 != ttMove;
    bool newKiller2 = killerMove2 != 0 && killerMove2 != toSmallMove(pvMove) && killerMove2 != ttMove;

    // 2. Single pass to assign Killer bonuses to quiet moves
    for (int i = 1; i <= moveCount; i++) {
        uint64_t m = pos->moves[startIndex + i];

        // If it's a capture, its mvScore is ALREADY in the upper bits!
        // Good captures have the +100 bonus, Bad captures don't. We leave them alone.

        // If it's a quiet move, we check for Killers.
        if (isVacant(captured(m))) {
            m = m & WITHOUT_MVSCORE_CACHE; // Strip any leftover score
            if (newKiller1 && killerMove1 == toSmallMove(m)) {
                m = m | KILLERMOVE1_BONUS;
            } else if (newKiller2 && killerMove2 == toSmallMove(m)) {
                m = m | KILLERMOVE2_BONUS;
            } else {
                // Normalize history scores below the killer bonuses.
                int c = colorIndex(pos->board[prev(m)]);
                int rawScore = historyTable11[c][prev(m)][next(m)];
                if (rawScore < 0) rawScore = 0;

                uint64_t histBonus = (uint64_t)((rawScore * HIST_BONUS_MAX11) / HISTORY_MAX11);
                m |= (histBonus << 41);
            }
            pos->moves[startIndex + i] = m;
        }
    }

    // Sort by the packed move score.

    insertionSort(pos, startIndex, moveCount);
}

int quiescenceSearch11(Position* pos, int startIndex, int alpha, int beta) {

    if (pos->stopSearch || (pos->historyPly > 0 && (pos->halfMove >= 100 || isRepetition(pos)))) return 0;

    if ((pos->nodeCount++ & 2047) == 0) {
        if (get_real_time_ms() - pos->startTime > pos->timeLimit) {
            pos->stopSearch = true;
            return 0;
        }
    }

    uint64_t h = pos->currentHash;
    TTEntry e = getEntry(tt11,TT_SIZE_THREADED,h);
    int originalAlpha = alpha;
    if (e.key == h) {
        int stored = scoreFromTT(e.eval,pos->currentDepth);
        if (e.flag == FLAG_EXACT) {
            thread_pvLength11[pos->currentDepth] = 0;
            return stored;
        } else if (e.flag == FLAG_BETA && alpha < stored) {
            alpha = stored;
        } else if (e.flag == FLAG_ALPHA && beta > stored) {
            beta =  stored;
        }
        if (alpha >= beta) {
            return stored;
        }
    }

    // Stop before exceeding the fixed search buffers.
    if (pos->currentDepth >= MAX_DEPTH - 1){
        return staticEval(pos);
    }

    int res = -INF;
    int stand_pat = staticEval(pos);

    if(isInCheck(pos)){
        generatePLMoves(pos);
    } else {
        // Stand-pat score.

        if (stand_pat >= beta) {
            addEntry(tt11, TT_SIZE_THREADED, h, scoreToTT(stand_pat, pos->currentDepth),
                     0, 0, FLAG_BETA);
            return beta;
        }
        if (alpha < stand_pat) alpha = stand_pat;

        res = stand_pat;

        generatePLCaptures(pos);
        int state = gameState(pos);
        if (pos->whiteToMove) genQuietQueenPromotionsWhite(pos, state);
        else genQuietQueenPromotionsBlack(pos, state);
    }

    int moveCount = (int) pos->moves[startIndex];
    bool anyLegal = false;

    int ply = pos->currentDepth;
    uint64_t pvMove = (thread_pvLength11[ply] > 0) ? thread_pvTable11[ply * MAX_DEPTH] : 0;
    thread_pvLength11[pos->currentDepth] = 0;
    uint16_t ttMove = (e.key == h) ? e.bestResponse : 0;

    moveCount = orderCapturesQS(pos, startIndex, moveCount, pvMove, ttMove);

    for (int i = 1; i <= moveCount; i++) {
        uint64_t m = pos->moves[startIndex + i];
        uint64_t h = pos->currentHash;

        int capturedVal = pieceValue(captured(m));
        // Delta pruning: captured material plus a 200 cp margin cannot reach alpha.
        if (!isInCheck(pos) && stand_pat + capturedVal + DELTA_PRUNING11 < alpha && isVacant(promotion(m))) {
            continue;
        }

        move(pos, m);
        pos->currentDepth++;

        if (canCaptureKing(pos)) {
            unmove(pos, m);
            pos->currentDepth--;
            pos->currentHash = h;
            continue;
        }

        anyLegal = true;

        int score = -quiescenceSearch11(pos, startIndex + 256, -beta, -alpha);

        unmove(pos, m);
        pos->currentHash = h;
        pos->currentDepth--;

        if (score > res) {
            res = score;
            if (score > alpha) {
                alpha = score;
            }
        }
        if (score >= beta) {
            addEntry(tt11, TT_SIZE_THREADED, pos->currentHash, scoreToTT(score, pos->currentDepth),
                     toSmallMove(m), 0, FLAG_BETA);

            thread_pvTable11[pos->currentDepth * MAX_DEPTH] = m;
            thread_pvLength11[pos->currentDepth] = 1;
            return res;
        }
    }

    if (!anyLegal && isInCheck(pos)) {
        return -INF + pos->currentDepth;
    }

    uint8_t flag = (res <= originalAlpha) ? FLAG_ALPHA : FLAG_EXACT;
    addEntry(tt11, TT_SIZE_THREADED, h, scoreToTT(res, pos->currentDepth), 0, 0, flag);
    return res;
}

int search11(Position* pos, int startIndex, int depth,int alpha, int beta, bool allowNull){

    if (pos->currentDepth >= MAX_DEPTH - 1) {
        return staticEval(pos);
    }

    if(pos->stopSearch || (pos->historyPly > 0 && (pos->halfMove >= 100 || isRepetition(pos)))) return 0;
    if((pos->nodeCount++ & 2047) == 0){
        if(get_real_time_ms() - pos->startTime > pos->timeLimit){
            pos->stopSearch = true;
            return 0;
        }
    }

    uint64_t h = pos->currentHash;
    TTEntry e = getEntry(tt11, TT_SIZE_THREADED, h);

    int originalAlpha = alpha;
    if (e.key == h && e.depth >= depth) {
        int storedScore = scoreFromTT(e.eval,pos->currentDepth);
        if (e.flag == FLAG_EXACT) {
            ttCutoffCount11++;
            return storedScore;
        } else if (e.flag == FLAG_BETA && alpha < storedScore) {
            alpha =storedScore;
        } else if (e.flag == FLAG_ALPHA && beta > storedScore) {
            beta =  storedScore;
        }
        if (alpha >= beta) {
            return storedScore;
        }
    }

    if(depth <= 0) {

        int res = quiescenceSearch11(pos,startIndex,alpha, beta);

        return res;
    }

    bool inCheck = isInCheck(pos);
    int extension = 0;
    if(inCheck) extension = 1; // Check extension.

    if (depth <= 3 && !inCheck && abs(beta) < MATE_THRESHOLD) {
       int margin = 120 * depth; // 120 cp per remaining ply.
        int e = staticEval(pos);
        if (e - margin >= beta) {
            return e - margin; // Reverse futility cutoff.
        }
    }

    if(allowNull && depth-1-NULL_MOVE_CONST11>=0 && !inCheck && hasNonPawnMaterial(pos)){

        pos->whiteToMove = !pos->whiteToMove;
        pos->currentDepth++;
        int prevEP = pos->enPassant;
        uint64_t prevHash = pos->currentHash;

        pos->enPassant = -1;
        pos->currentHash ^= zobristRandom[12*64];
        if(prevEP != -1) { // We remove the en Passant only if it was there before, that is, if it was possible before
            int epRank = prevEP / 8;

            int pawnSq = (epRank == 2) ? prevEP + 8 : prevEP - 8;
            int col2 = pawnSq % 8;

            int attackerColor = (epRank == 2) ? Black : White;
            int expectedPawn = Pawn | attackerColor;

            if( (col2 > 0 && pos->board[pawnSq - 1] == expectedPawn) ||
                (col2 < 7 && pos->board[pawnSq + 1] == expectedPawn) ) {
                pos->currentHash ^= zobristRandom[12*64 + 5 + col2];
            }
        }

        int nmScore = -search11(pos,startIndex+256,depth-1-NULL_MOVE_CONST11,-beta,-beta+1,false);

        pos->currentDepth--;
        pos->whiteToMove = !pos->whiteToMove;
        pos->enPassant = prevEP;
        pos->currentHash = prevHash;
        if(nmScore >= beta){
            return beta;
        }
    }

    int res = -INF;
    generatePLMoves(pos);
    int moveCount = (int) pos->moves[startIndex] ;

    if(moveCount == 0){
        if(isInCheck(pos)) return -INF + pos->currentDepth;
        return 0;
    }

    int ply = pos->currentDepth;
    uint64_t pvMove = thread_pvLength11[ply] > 0 ? thread_pvTable11[ply * MAX_DEPTH] : 0;
    thread_pvLength11[pos->currentDepth] = 0;

    uint16_t ttMove = e.key == h ? e.bestResponse : 0;
    orderMoves11(pos,startIndex,moveCount,pvMove,ttMove);

    uint64_t bestResponse = pos->moves[startIndex+1];
    uint64_t hash = pos->currentHash;

    bool isFirstMove = true;

    for(int i = 1 ; i <= moveCount ; i++){
        uint64_t m = pos->moves[(startIndex) + i];
        move(pos,m);
        pos->currentDepth++;
        if(canCaptureKing(pos)){
            unmove(pos,m);
            pos->currentDepth--;
            pos->currentHash = h;
            continue;
        }
        int score;
        if(isFirstMove){
            score = -search11(pos,startIndex+256,depth-1+extension,-beta,-alpha,true);
            isFirstMove = false;
        } else {
            int reduction = 0;
            // Reduce late moves before a full-depth re-search.
            if (!inCheck && depth >= 3 && i > 3 && isVacant(captured(m)) && isVacant(promotion(m)) && (m >> 41) <= 50) {
                reduction = 1;

            }

            // 2. Reduced depth, null-window search
            score = -search11(pos, startIndex + 256, depth - 1 - reduction + extension, -alpha - 1, -alpha, true);

            // 3. If it failed high AND we reduced it, re-search at full depth (null-window)
            if (score > alpha && reduction > 0) {
                score = -search11(pos, startIndex + 256, depth - 1 + extension, -alpha - 1, -alpha, true);
            }

            // 4. PVS Re-search (exact window) if it improved alpha
            if (score > alpha && score < beta) {
                score = -search11(pos, startIndex + 256, depth - 1 + extension, -beta, -alpha, true);
            }
        }

        unmove(pos,m);
        pos->currentDepth--;
        pos->currentHash = hash;
        if(pos->stopSearch) return 0;
        if(score > res){
            res = score;
            bestResponse = m;
            if(score > alpha){
                alpha = score;
                thread_pvTable11[pos->currentDepth*MAX_DEPTH + 0] = m;
                memcpy(&thread_pvTable11[pos->currentDepth*MAX_DEPTH+1], &thread_pvTable11[(pos->currentDepth+1)*MAX_DEPTH], thread_pvLength11[pos->currentDepth+1] * sizeof(uint64_t));
                thread_pvLength11[pos->currentDepth] = thread_pvLength11[pos->currentDepth+1] + 1;
            }
        }
        if (score >= beta){
            if(isVacant(captured(m))){
                int bonus = depth * depth;
                if (bonus > 400) bonus = 400;

                int c = colorIndex(pos->board[prev(m)]);
                int current = historyTable11[c][prev(m)][next(m)];
                historyTable11[c][prev(m)][next(m)] += bonus - (current * bonus) / HISTORY_MAX11;
                storeKiller11(pos->currentDepth,m);
            }

            addEntry(tt11, TT_SIZE_THREADED, pos->currentHash, scoreToTT(score, pos->currentDepth), toSmallMove(m), (uint8_t) depth, FLAG_BETA);
            thread_pvTable11[pos->currentDepth * MAX_DEPTH] = m;
            thread_pvLength11[pos->currentDepth] = 1;
            return res;
        }
    }
    if(isFirstMove){
        if(isInCheck(pos)) return -INF + pos->currentDepth;
        return 0;
    }
    uint8_t flag = FLAG_EXACT;
    if(res <= originalAlpha) {
        flag = FLAG_ALPHA;
        addEntry(tt11, TT_SIZE_THREADED, pos->currentHash, scoreToTT(res, pos->currentDepth), 0, (uint8_t) depth, flag);
    } else {
        addEntry(tt11, TT_SIZE_THREADED, pos->currentHash, scoreToTT(res, pos->currentDepth), toSmallMove(bestResponse), (uint8_t) depth, flag);
    }
    return res;
}

uint64_t getBestMoveTime11(Position* pos,time_t limit){

    if(inBook11){
        uint64_t bookMove = makeFromOpeningMove(pos);
        if(bookMove == 0){
            inBook11 = false;
        } else {
            printf("Opening-book move.\n\n\n");
            return bookMove;
        }
    }

    pos->timeLimit = limit;
    pos->startTime = get_real_time_ms();
    pos->stopSearch = false;
    pos->nodeCount = 0;
    pos->currentDepth = 0;
    ttCutoffCount11 = 0;
    thread_pvLength11[0] = 0;
    memset(historyTable11, 0, sizeof(historyTable11));
    for(int i = 0; i < 2*MAX_DEPTH; i++){
        thread_killers11[i]=0;
    }

    generateMoves(pos);
    int moveCount = (int) pos->moves[0] ;
    if(moveCount == 0) return 0 ;

    int score = staticEval(pos);
    uint64_t bestMove = pos->moves[1];

    for (int depth = 1; depth <= 64; depth++) {
        int delta = ASPIRATION_WINDOW11;
        int alpha = -INF;
        int beta = INF;

        // Use aspiration windows from depth 5, outside the mate-score range.
        if (depth >= 5 && abs(score) < MATE_THRESHOLD) {
            alpha = score - delta;
            beta = score + delta;
        }

        uint64_t candidate = 0;

        while (true) {
            int origAlpha = alpha;
            int origBeta = beta;

            uint64_t h0 = pos->currentHash;
            TTEntry e0 = getEntry(tt11, TT_SIZE_THREADED, h0);
            uint64_t pvMove = (thread_pvLength11[0] > 0) ? thread_pvTable11[0] : 0;
            uint16_t ttMove = (e0.key == h0) ? e0.bestResponse : 0;
            orderMoves11(pos, 0, moveCount, pvMove, ttMove);

            int currentScore = -INF;
            bool isFirstMove = true;

            for (int i = 1; i <= moveCount; i++) {
                uint64_t m = pos->moves[i];
                uint64_t h = pos->currentHash;
                move(pos, m);
                pos->currentDepth++;

                int s;
                if (isFirstMove) {
                    s = -search11(pos, 256 * pos->currentDepth, depth - 1, -beta, -alpha, true);
                    isFirstMove = false;
                } else {
                    s = -search11(pos, 256 * pos->currentDepth, depth - 1, -alpha - 1, -alpha, true);
                    if (s > alpha && s < beta) {
                        s = -search11(pos, 256 * pos->currentDepth, depth - 1, -beta, -alpha, true);
                    }
                }

                unmove(pos, m);
                pos->currentHash = h;
                pos->currentDepth--;

                if (pos->stopSearch) break;

                if (s > currentScore) currentScore = s;

                if (s > alpha) {
                    alpha = s;
                    candidate = m;
                    thread_pvTable11[0] = m;
                    memcpy(&thread_pvTable11[1], &thread_pvTable11[MAX_DEPTH], thread_pvLength11[1] * sizeof(uint64_t));
                    thread_pvLength11[0] = thread_pvLength11[1] + 1;
                }
            }

            if (pos->stopSearch) break;

            if (currentScore <= origAlpha) {
                alpha = -INF;
                beta = origBeta;
            } else if (currentScore >= origBeta) {
                alpha = origAlpha;
                beta = INF;
            } else {
                score = currentScore;
                break;
            }
        }
        if (pos->stopSearch) break;
        if (candidate != 0) bestMove = candidate;
        CHESSBOT_SEARCH_ITERATION(depth);
    }

    return bestMove;
}

