#include "../Bot.h"
#include <assert.h>

#define NULL_MOVE_CONST13 3
#define HISTORY_MAX13 16384
#define HIST_BONUS_MAX13 96
#define ASPIRATION_WINDOW13 50
#define DELTA_PRUNING13 200
#define GOOD_CAPTURE_BONUS13 (128ULL << 41)

__thread int ttCutoffCount13 = 0;
__thread uint64_t thread_pvTable13[MAX_DEPTH*(MAX_DEPTH+1)];
__thread int thread_pvLength13[MAX_DEPTH+1];
__thread uint16_t thread_killers13[MAX_DEPTH*2];
__thread bool inBook13 = true;
__thread int historyTable13[2][64][64]; // [color][from][to]
extern __thread TTEntry* tt13;
Bot v13t;

static inline void updateHistoryAndKillers13(Position* pos, uint64_t m, int depth, int history[2][64][64], uint16_t* killers) {
    if (isVacant(captured(m))) {
        int bonus = depth * depth;
        if (bonus > 400) bonus = 400;

        int c = colorIndex(pos->board[prev(m)]);
        int current = history[c][prev(m)][next(m)];
        history[c][prev(m)][next(m)] += bonus - (current * bonus) / HISTORY_MAX13;

        uint16_t mv = toSmallMove(m);
        int ply = pos->currentDepth;
        if (killers[2 * ply] != mv) {
            killers[2 * ply + 1] = killers[2 * ply];
            killers[2 * ply] = mv;
        }
    }
}

void orderMoves13(Position* pos, int startIndex, int moveCount, uint64_t pvMove, uint16_t ttMove) {
    // 1. Promote PV and TT
    if (pvMove != 0 && promoteFullMove(pos, pvMove, startIndex + 1, startIndex + moveCount)) {
        startIndex++; moveCount--;
    }
    if (ttMove != 0 && ttMove != toSmallMove(pvMove) && promoteSmallMove(pos, ttMove, startIndex + 1, startIndex + moveCount)) {
        startIndex++; moveCount--;
    }

    int ply = pos->currentDepth;
    uint16_t killerMove1 = thread_killers13[2 * ply];
    uint16_t killerMove2 = thread_killers13[2 * ply + 1];
    bool newKiller1 = killerMove1 != 0 && killerMove1 != toSmallMove(pvMove) && killerMove1 != ttMove;
    bool newKiller2 = killerMove2 != 0 && killerMove2 != toSmallMove(pvMove) && killerMove2 != ttMove;

    // 2. Single pass to assign Killer bonuses to quiet moves
    for (int i = 1; i <= moveCount; i++) {
        uint64_t m = pos->moves[startIndex + i];
        m = m & WITHOUT_MVSCORE_CACHE; // Strip any leftover score
        if(!isVacant(captured(m)) || !isVacant(promotion(m))){
            int s = see(pos,m);
            if(s >= 0){
                m |= GOOD_CAPTURE_BONUS13;
                m += (((uint64_t)(s/100)) << 41);
            }
        } else {
            if (newKiller1 && killerMove1 == toSmallMove(m)) {
                m = m | KILLERMOVE1_BONUS;
            } else if (newKiller2 && killerMove2 == toSmallMove(m)) {
                m = m | KILLERMOVE2_BONUS;
            } else {
                // Normalize history scores below the killer bonuses.
                int c = colorIndex(pos->board[prev(m)]);
                int rawScore = historyTable13[c][prev(m)][next(m)];
                if (rawScore < 0) rawScore = 0;

                uint64_t histBonus = (uint64_t)((rawScore * HIST_BONUS_MAX13) / HISTORY_MAX13);
                m |= (histBonus << 41);
            }
        }
        pos->moves[startIndex + i] = m;
    }
    // Sort by the packed move score.

    insertionSort(pos, startIndex, moveCount);
}

int quiescenceSearch13(Position* pos, int alpha, int beta) {
    int startIndex = 256 * pos->currentDepth;

    if(checkStopConditions(pos)) return 0;

    uint64_t h = pos->currentHash;
    TTEntry e = getEntry(tt13,TT_SIZE_THREADED,h);
    int originalAlpha = alpha;
    if (e.key == h) {
        int stored = scoreFromTT(e.eval,pos->currentDepth);
        if (e.flag == FLAG_EXACT) {
            thread_pvLength13[pos->currentDepth] = 0;
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
            addEntry(tt13, TT_SIZE_THREADED, h, scoreToTT(stand_pat, pos->currentDepth),
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
    uint64_t pvMove = (thread_pvLength13[ply] > 0) ? thread_pvTable13[ply * MAX_DEPTH] : 0;
    thread_pvLength13[pos->currentDepth] = 0;
    uint16_t ttMove = (e.key == h) ? e.bestResponse : 0;

    moveCount = orderCapturesQS(pos, startIndex, moveCount, pvMove, ttMove);

    for (int i = 1; i <= moveCount; i++) {
        uint64_t m = pos->moves[startIndex + i];
        uint64_t h = pos->currentHash;

        if (!isInCheck(pos) && see(pos, m) < 0) continue;

        move(pos, m);
        pos->currentDepth++;

        if (canCaptureKing(pos)) {
            unmove(pos, m);
            pos->currentDepth--;
            pos->currentHash = h;
            continue;
        }

        anyLegal = true;

        int score = -quiescenceSearch13(pos, -beta, -alpha);

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
            addEntry(tt13, TT_SIZE_THREADED, pos->currentHash, scoreToTT(score, pos->currentDepth),
                     toSmallMove(m), 0, FLAG_BETA);

            thread_pvTable13[pos->currentDepth * MAX_DEPTH] = m;
            thread_pvLength13[pos->currentDepth] = 1;
            return res;
        }
    }

    if (!anyLegal && isInCheck(pos)) {
        return -INF + pos->currentDepth;
    }

    uint8_t flag = (res <= originalAlpha) ? FLAG_ALPHA : FLAG_EXACT;
    addEntry(tt13, TT_SIZE_THREADED, h, scoreToTT(res, pos->currentDepth), 0, 0, flag);
    return res;
}

int search13(Position* pos, int depth,int alpha, int beta, bool allowNull){
    int startIndex = 256 * pos->currentDepth;

    if (pos->currentDepth >= MAX_DEPTH - 1) {
        return staticEval(pos);
    }

    if (checkStopConditions(pos)) return 0;

    uint64_t h = pos->currentHash;
    TTEntry e = getEntry(tt13, TT_SIZE_THREADED, h);

    int originalAlpha = alpha;
    if (e.key == h && e.depth >= depth) {
        int storedScore = scoreFromTT(e.eval,pos->currentDepth);
        if (e.flag == FLAG_EXACT) {
            ttCutoffCount13++;
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
        int res = quiescenceSearch13(pos,alpha, beta);
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

    if (allowNull && depth - 1 - NULL_MOVE_CONST13 >= 0 && !inCheck && hasNonPawnMaterial(pos)) {
        NullMoveState nms;
        makeNullMove(pos, &nms);
        int nmScore = -search13(pos, depth - 1 - NULL_MOVE_CONST13, -beta, -beta + 1, false);
        unmakeNullMove(pos, &nms);
        if (nmScore >= beta) return beta;
    }

    int res = -INF;
    generatePLMoves(pos);
    int moveCount = (int) pos->moves[startIndex] ;

    if(moveCount == 0){
        if(isInCheck(pos)) return -INF + pos->currentDepth;
        return 0;
    }

    int ply = pos->currentDepth;
    uint64_t pvMove = thread_pvLength13[ply] > 0 ? thread_pvTable13[ply * MAX_DEPTH] : 0;
    thread_pvLength13[pos->currentDepth] = 0;

    uint16_t ttMove = e.key == h ? e.bestResponse : 0;
    orderMoves13(pos,startIndex,moveCount,pvMove,ttMove);

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
            score = -search13(pos,depth-1+extension,-beta,-alpha,true);
            isFirstMove = false;
        } else {
            int reduction = 0;
            // Reduce late moves before a full-depth re-search.
            bool isQuiet = isVacant(captured(m)) && isVacant(promotion(m));
            bool isBadCapture = !isVacant(captured(m)) && !(m & GOOD_CAPTURE_BONUS13);

            if (!inCheck && depth >= 3 && i >= 3 && (isQuiet || isBadCapture) &&
                toSmallMove(m) != thread_killers13[2 * ply] &&
                toSmallMove(m) != thread_killers13[2 * ply + 1]) {

                reduction = lmrTable[depth | (i*64)];

                if (isQuiet) {
                    // Adjust quiet-move reductions using history.
                   int c = colorIndex(pos->board[prev(m)]);
                    int hist = historyTable13[c][prev(m)][next(m)];
                    if (hist > (HISTORY_MAX13 / 2)) reduction--;
                    else if (hist < (HISTORY_MAX13 / 8)) reduction++;
                } else {
                    // Bad captures do not use quiet-move history.

                    if (reduction > 2) reduction = 2;
                }

                if (reduction < 1) reduction = 1;
                if (reduction > depth - 2) reduction = depth - 2;
            }

            // 2. Reduced depth, null-window search
            score = -search13(pos, depth - 1 - reduction + extension, -alpha - 1, -alpha, true);

            // 3. If it failed high AND we reduced it, re-search at full depth (null-window)
            if (score > alpha && reduction > 0) {
                score = -search13(pos, depth - 1 + extension, -alpha - 1, -alpha, true);
            }

            // 4. PVS Re-search (exact window) if it improved alpha
            if (score > alpha && score < beta) {
                score = -search13(pos, depth - 1 + extension, -beta, -alpha, true);
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
                updatePV(thread_pvTable13, thread_pvLength13, pos->currentDepth, m);
            }
        }
        if (score >= beta){

            updateHistoryAndKillers13(pos, m, depth, historyTable13, thread_killers13);
            addEntry(tt13, TT_SIZE_THREADED, pos->currentHash, scoreToTT(score, pos->currentDepth), toSmallMove(m), (uint8_t) depth, FLAG_BETA);
            thread_pvTable13[pos->currentDepth * MAX_DEPTH] = m;
            thread_pvLength13[pos->currentDepth] = 1;
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
        addEntry(tt13, TT_SIZE_THREADED, pos->currentHash, scoreToTT(res, pos->currentDepth), 0, (uint8_t) depth, flag);
    } else {
        addEntry(tt13, TT_SIZE_THREADED, pos->currentHash, scoreToTT(res, pos->currentDepth), toSmallMove(bestResponse), (uint8_t) depth, flag);
    }
    return res;
}

uint64_t getBestMoveTime13(Position* pos,time_t limit){

    if(inBook13){
        uint64_t bookMove = makeFromOpeningMove(pos);
        if(bookMove == 0){
            inBook13 = false;
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
    ttCutoffCount13 = 0;
    thread_pvLength13[0] = 0;

    memset(historyTable13, 0, sizeof(historyTable13));
    memset(thread_killers13, 0, sizeof(thread_killers13));

    generateMoves(pos);
    int moveCount = (int) pos->moves[0] ;
    if(moveCount == 0) return 0 ;

    int score = staticEval(pos);
    uint64_t bestMove = pos->moves[1];

    for (int depth = 1; depth <= 64; depth++) {
        int delta = ASPIRATION_WINDOW13;
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
            TTEntry e0 = getEntry(tt13, TT_SIZE_THREADED, h0);
            uint64_t pvMove = (thread_pvLength13[0] > 0) ? thread_pvTable13[0] : 0;
            uint16_t ttMove = (e0.key == h0) ? e0.bestResponse : 0;
            orderMoves13(pos, 0, moveCount, pvMove, ttMove);

            int currentScore = -INF;
            bool isFirstMove = true;

            for (int i = 1; i <= moveCount; i++) {
                uint64_t m = pos->moves[i];
                uint64_t h = pos->currentHash;
                move(pos, m);
                pos->currentDepth++;

                int s;
                if (isFirstMove) {
                    s = -search13(pos, depth - 1, -beta, -alpha, true);
                    isFirstMove = false;
                } else {
                    s = -search13(pos, depth - 1, -alpha - 1, -alpha, true);
                    if (s > alpha && s < beta) {
                        s = -search13(pos, depth - 1, -beta, -alpha, true);
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
                    updatePV(thread_pvTable13, thread_pvLength13, 0, m);
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

