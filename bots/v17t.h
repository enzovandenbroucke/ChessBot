#include "../Bot.h"
#include <assert.h>

#define NULL_MOVE_CONST17 3
#define HISTORY_MAX17 16384
#define HIST_BONUS_MAX17 96
#define ASPIRATION_WINDOW17 50
#define GOOD_CAPTURE_BONUS17 (128ULL << 41)
#define COUNTERMOVE_BONUS17 ((uint64_t) 98 << 41) // just under Killer 2 (100)

__thread int cpt17 = 0;
__thread uint64_t thread_pvTable17[MAX_DEPTH*(MAX_DEPTH+1)];
__thread int thread_pvLength17[MAX_DEPTH+1];
__thread uint16_t thread_killers17[MAX_DEPTH*2];
__thread bool inBook17 = true;
__thread int historyTable17[2][64][64]; // [color][from][to]
__thread uint16_t thread_counterMoves17[64][64]; // [opponentFrom][opponentTo]
extern __thread TTEntry* tt17;
Bot v17t;

#ifdef CHESSBOT_WEB
/* Optional telemetry; native builds retain the existing search interface. */
__thread int webSearchScore17;
__thread int webSearchDepth17;
#endif

static inline void updateHistoryKillersAndCounter17(Position* pos, uint64_t bestMove,
    int depth,uint64_t* quietsSearched, int numQuiets,uint64_t prevMove) {
    int bonus = depth * depth;
    if (bonus > 400) bonus = 400;

    // Reward the quiet cutoff move.
    int c = pos->whiteToMove ? WHITE_INDEX : BLACK_INDEX;
    int cur = historyTable17[c][prev(bestMove)][next(bestMove)];
    historyTable17[c][prev(bestMove)][next(bestMove)] += bonus - (cur * bonus) / HISTORY_MAX17;

    // Penalize earlier quiet moves that failed to cause a cutoff.
    for (int i = 0; i < numQuiets; i++) {
        uint64_t qm = quietsSearched[i];
        int qc = colorIndex(pos->board[prev(qm)]);
        int qcur = historyTable17[qc][prev(qm)][next(qm)];
        historyTable17[qc][prev(qm)][next(qm)] -= bonus + (qcur * bonus) / HISTORY_MAX17;
    }

    uint16_t mv = toSmallMove(bestMove);
    int ply = pos->currentDepth;
    if (thread_killers17[2 * ply] != mv) {
        thread_killers17[2 * ply + 1] = thread_killers17[2 * ply];
        thread_killers17[2 * ply] = mv;
    }

    // Associate the countermove with the preceding opponent move.
    if (prevMove != 0) {
        thread_counterMoves17[prev(prevMove)][next(prevMove)] = mv;
    }
}

void orderMoves17(Position* pos, int startIndex, int moveCount, uint64_t pvMove, uint16_t ttMove, uint64_t prevMove) {
    if (pvMove != 0 && promoteSmallMove(pos, toSmallMove(pvMove), startIndex + 1, startIndex + moveCount)) {
        startIndex++; moveCount--;
    }
    if (ttMove != 0 && ttMove != toSmallMove(pvMove) && promoteSmallMove(pos, ttMove, startIndex + 1, startIndex + moveCount)) {
        startIndex++; moveCount--;
    }

    int ply = pos->currentDepth;
    uint16_t killer1 = thread_killers17[2 * ply];
    uint16_t killer2 = thread_killers17[2 * ply + 1];
    uint16_t cmMove = (prevMove != 0) ? thread_counterMoves17[prev(prevMove)][next(prevMove)] : 0;

    for (int i = 1; i <= moveCount; i++) {
        uint64_t m = pos->moves[startIndex + i] & WITHOUT_MVSCORE_CACHE;
        uint16_t sm = toSmallMove(m);

        if (!isVacant(captured(m)) || !isVacant(promotion(m))) {
            int s = see(pos, m);
            if (s >= 0) {
                m |= GOOD_CAPTURE_BONUS17;
                m += (((uint64_t)(s / 100)) << 41);
            }
        } else {
            if (killer1 != 0 && sm == killer1) {
                m |= KILLERMOVE1_BONUS;
            } else if (killer2 != 0 && sm == killer2) {
                m |= KILLERMOVE2_BONUS;
            } else if (cmMove != 0 && sm == cmMove) {
                m |= COUNTERMOVE_BONUS17;
            } else {
                int c = colorIndex(pos->board[prev(m)]);
                int rawScore = historyTable17[c][prev(m)][next(m)];
                if (rawScore < 0) rawScore = 0;
                uint64_t histBonus = (uint64_t)((rawScore * HIST_BONUS_MAX17) / HISTORY_MAX17);
                m |= (histBonus << 41);
            }
        }
        pos->moves[startIndex + i] = m;
    }
    insertionSort(pos, startIndex, moveCount);
}

int quiescenceSearch17(Position* pos, int alpha, int beta) {
    int startIndex = 256 * pos->currentDepth;

    if(checkTimeOnly(pos)) return 0;

    uint64_t h = pos->currentHash;
    TTEntry e = getEntry(tt17,TT_SIZE_THREADED,h);
    int originalAlpha = alpha;
    if (e.key == h) {
        int stored = scoreFromTT(e.eval,pos->currentDepth);
        if (e.flag == FLAG_EXACT) {
            thread_pvLength17[pos->currentDepth] = 0;
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
        return evalNew(pos);
    }

    int res = -INF;
    int stand_pat = evalNew(pos);

    if(isInCheck(pos)){
        generatePLMoves(pos);
    } else {
        // Stand-pat score.

        if (stand_pat >= beta) {
            addEntry(tt17, TT_SIZE_THREADED, h, scoreToTT(stand_pat, pos->currentDepth),
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

    uint64_t pvMove = (pos->currentDepth == 0 && thread_pvLength17[0] > 0) ? thread_pvTable17[0] : 0;
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

        int score = -quiescenceSearch17(pos, -beta, -alpha);

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
            addEntry(tt17, TT_SIZE_THREADED, pos->currentHash, scoreToTT(score, pos->currentDepth),
                     toSmallMove(m), 0, FLAG_BETA);

            thread_pvTable17[pos->currentDepth * MAX_DEPTH] = m;
            thread_pvLength17[pos->currentDepth] = 1;
            return res;
        }
    }

    if (!anyLegal && isInCheck(pos)) {
        return -INF + pos->currentDepth;
    }

    uint8_t flag = (res <= originalAlpha) ? FLAG_ALPHA : FLAG_EXACT;
    addEntry(tt17, TT_SIZE_THREADED, h, scoreToTT(res, pos->currentDepth), 0, 0, flag);
    return res;
}

int search17(Position* pos, int depth, int alpha, int beta, bool allowNull, uint64_t prevMove, uint16_t excludedMove){
    int startIndex = 256 * pos->currentDepth;

    if (pos->currentDepth >= MAX_DEPTH - 1) {
        return evalNew(pos);
    }

    if (checkStopConditions(pos)) return 0;

    uint64_t h = pos->currentHash;
    TTEntry e = getEntry(tt17, TT_SIZE_THREADED, h);

    int originalAlpha = alpha;
    if (excludedMove == 0 && e.key == h && e.depth >= depth) {
        int storedScore = scoreFromTT(e.eval,pos->currentDepth);
        if (e.flag == FLAG_EXACT) {
            cpt17++;
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
        int res = quiescenceSearch17(pos,alpha, beta);
        return res;
    }
    bool inCheck = isInCheck(pos);

    if (depth >= 4 && !inCheck && excludedMove == 0 && (e.key != h || e.bestResponse == 0)) {
        depth--;
    }

    if (depth == 1 && !inCheck && abs(beta) < MATE_THRESHOLD) {
        int se = evalNew(pos);
        if (se + 300 <= alpha) {
            int score = quiescenceSearch17(pos, alpha, beta);
            if (score <= alpha) return score; // Prune only after the reduced search confirms fail-low.
        }
    }

    int extension = 0;
    if(inCheck) extension = 1; // Check extension.

    bool futilityPrune = false;
    if (depth <= 3 && !inCheck && abs(beta) < MATE_THRESHOLD && hasNonPawnMaterial(pos)) {
        int margin = 120 * depth; // 120 cp per remaining ply.
        int futilityMargin = 100 * depth; // 100 cp per remaining ply.
        int se = evalNew(pos);
        if (se - margin >= beta) {
            return se - margin; // Reverse futility cutoff.
        }
        if (se + futilityMargin <= alpha) {
           futilityPrune = true;  // will skip quiet moves in the loop
        }
    }

    if (allowNull && depth - 1 - NULL_MOVE_CONST17 >= 0 && !inCheck && hasNonPawnMaterial(pos)) {
        NullMoveState nms;
        makeNullMove(pos, &nms);
        int nmScore = -search17(pos, depth - 1 - NULL_MOVE_CONST17, -beta, -beta + 1, false,0,0);
        unmakeNullMove(pos, &nms);
        if (nmScore >= beta) return beta;
    }
    uint16_t ttMove = e.key == h ? e.bestResponse : 0;
    int singularExtension = 0;
    // Test whether the TT move is singular.
    if (depth >= 7 && !inCheck && excludedMove == 0 && ttMove != 0 &&
        e.depth >= depth - 3 && e.flag != FLAG_ALPHA && abs(e.eval) < MATE_THRESHOLD){
        int singularBeta = e.eval - 2 * depth;
        int singularDepth = (depth - 1) / 2;

        // Search at reduced depth with the TT move excluded.
        int sScore = search17(pos, singularDepth, singularBeta - 1, singularBeta, false, prevMove, ttMove);

        // Extend the TT move when alternatives fail below singularBeta.
        if (sScore < singularBeta) {
            singularExtension = 1;
        }
    }

    int res = -INF;
    generatePLMoves(pos);
    int moveCount = (int) pos->moves[startIndex] ;

    if(moveCount == 0){
        if(inCheck) return -INF + pos->currentDepth;
        return 0;
    }

    int ply = pos->currentDepth;
    uint64_t pvMove = thread_pvLength17[ply] > 0 ? thread_pvTable17[ply * MAX_DEPTH] : 0;
    thread_pvLength17[pos->currentDepth] = 0;

    orderMoves17(pos,startIndex,moveCount,pvMove,ttMove,prevMove);

    uint64_t bestResponse = pos->moves[startIndex+1];
    uint64_t hash = pos->currentHash;

    bool isFirstMove = true;

    int d = (depth > 64) ? 64 : depth;
    const int* lmrDepth = &lmrTable[d * 256];
    int lateMovesCap = 3 + 3 * depth * depth;
    int quietMovesCount = 0;

    uint64_t quietsSearched[MAX_MOVES];
    int numQuiets = 0;

    for(int i = 1 ; i <= moveCount ; i++){
        uint64_t m = pos->moves[(startIndex) + i];
        if (excludedMove != 0 && toSmallMove(m) == excludedMove) {
            continue;
        }

        bool isQuiet = isVacant(captured(m)) && isVacant(promotion(m));
        bool isKiller = (toSmallMove(m) == thread_killers17[2 * ply]) || (toSmallMove(m) == thread_killers17[2 * ply + 1]);

        move(pos,m);
        pos->currentDepth++;

        if(canCaptureKing(pos)){
            unmove(pos,m);
            pos->currentDepth--;
            pos->currentHash = h;
            continue;
        }
        if (isQuiet) {
            quietMovesCount++;

            // Late move pruning.
            if (!inCheck && depth <= 4 && quietMovesCount > lateMovesCap && !isKiller && !isInCheck(pos)) {
                unmove(pos, m);
                pos->currentDepth--;
                pos->currentHash = h;
                continue;
            }
        }

        if (futilityPrune && isQuiet && !isKiller && i > 1 && !isInCheck(pos)) {
            unmove(pos, m);
            pos->currentDepth--;
            pos->currentHash = h;
            continue;
        }

        if (isQuiet && numQuiets < MAX_MOVES) {
            quietsSearched[numQuiets++] = m;
        }

        int score;
        if(isFirstMove){
            int ext = extension;
            if(toSmallMove(m) == ttMove){
                ext += singularExtension;
            }
            score = -search17(pos, depth - 1 + ext, -beta, -alpha, true, m, 0);
            isFirstMove = false;
        } else {
            int reduction = 0;
            // Reduce late moves before a full-depth re-search.
            bool isBadCapture = !isVacant(captured(m)) && !(m & GOOD_CAPTURE_BONUS17);

            if (!inCheck && depth >= 3 && i >= 3 && (isQuiet || isBadCapture) &&
                toSmallMove(m) != thread_killers17[2 * ply] &&
                toSmallMove(m) != thread_killers17[2 * ply + 1]) {

                int k = (i > 255) ? 255 : i;
                reduction = lmrDepth[k];

                if (isQuiet) {
                    // Adjust quiet-move reductions using history.
                   int c = colorIndex(pos->board[prev(m)]);
                    int hist = historyTable17[c][prev(m)][next(m)];
                    if (hist > (HISTORY_MAX17 / 2)) reduction--;
                    else if (hist < (HISTORY_MAX17 / 8)) reduction++;
                } else {
                    // Bad captures do not use quiet-move history.

                    if (reduction > 2) reduction = 2;
                }

                if (reduction < 1) reduction = 1;
                if (reduction > depth - 2) reduction = depth - 2;
            }

            // 2. Reduced depth, null-window search
            score = -search17(pos, depth - 1 - reduction + extension, -alpha - 1, -alpha, true,m,0);

            // 3. If it failed high AND we reduced it, re-search at full depth (null-window)
            if (score > alpha && reduction > 0) {
                score = -search17(pos, depth - 1 + extension, -alpha - 1, -alpha, true,m,0);
            }

            // 4. PVS Re-search (exact window) if it improved alpha
            if (score > alpha && score < beta) {
                score = -search17(pos, depth - 1 + extension, -beta, -alpha, true,m,0);
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
                if(excludedMove == 0) updatePV(thread_pvTable17, thread_pvLength17, pos->currentDepth, m);
            }
        }
        if (score >= beta){
            if (isQuiet) {
                numQuiets--; // Exclude the cutoff move from the history penalties.
                updateHistoryKillersAndCounter17(pos, m, depth, quietsSearched, numQuiets, prevMove);
            }
            if(excludedMove == 0){
                addEntry(tt17, TT_SIZE_THREADED, pos->currentHash, scoreToTT(score, pos->currentDepth), toSmallMove(m), (uint8_t) depth, FLAG_BETA);
                thread_pvTable17[pos->currentDepth * MAX_DEPTH] = m;
                thread_pvLength17[pos->currentDepth] = 1;
            }
            return res;
        }
    }
    if(isFirstMove){
        if(isInCheck(pos)) return -INF + pos->currentDepth;
        return 0;
    }
    if (res <= originalAlpha && excludedMove == 0) {
        uint16_t moveOrKeep = (e.key == h) ? e.bestResponse : 0;
        addEntry(tt17, TT_SIZE_THREADED, h, scoreToTT(res, pos->currentDepth), moveOrKeep, (uint8_t)depth, FLAG_ALPHA);
    } else if (excludedMove == 0) {
        addEntry(tt17, TT_SIZE_THREADED, h, scoreToTT(res, pos->currentDepth), toSmallMove(bestResponse), (uint8_t)depth, FLAG_EXACT);
    }
    return res;
}

uint64_t getBestMoveDynamic17(Position* pos,uint64_t timeLeft, uint64_t increment){

#ifdef CHESSBOT_WEB
    webSearchScore17 = 0;
    webSearchDepth17 = 0;
#endif

    if(inBook17){
        uint64_t bookMove = makeFromOpeningMove(pos);
        if(bookMove == 0){
            inBook17 = false;
        } else {
            printf("Opening-book move.\n\n\n");
            return bookMove;
        }
    }

    generateMoves(pos);
    int moveCount = (int) pos->moves[0] ;
    if(moveCount == 0) return 0 ;
    if(moveCount == 1) return pos->moves[1];

    uint64_t targetTime = timeLeft / 25 + increment / 2;
    uint64_t maxTime = targetTime * 3;
    if (maxTime > (uint64_t)(timeLeft * 0.75)) {
        maxTime = (uint64_t)(timeLeft * 0.75);
    }
#ifdef CHESSBOT_BENCHMARK
    if (benchmarkFixedMs) targetTime = maxTime = benchmarkFixedMs;
#endif

    pos->timeLimit = maxTime;
    pos->startTime = get_real_time_ms();
    pos->stopSearch = false;
    pos->nodeCount = 0;
    pos->currentDepth = 0;
    thread_pvLength17[0] = 0;

    memset(historyTable17, 0, sizeof(historyTable17));
    memset(thread_killers17, 0, sizeof(thread_killers17));
    memset(thread_counterMoves17, 0, sizeof(thread_counterMoves17));

    int score = evalNew(pos);
    int prevScore = score;
    uint64_t bestMove = pos->moves[1];
    uint64_t prevBestMove = bestMove;

    for (int depth = 1; depth <= 64; depth++) {
        int delta = ASPIRATION_WINDOW17;
        int alpha = -INF;
        int beta = INF;

        if (depth >= 5 && abs(score) < MATE_THRESHOLD) {
            alpha = score - delta;
            beta = score + delta;
        }

        uint64_t candidate = 0;

        while (true) {
            int origAlpha = alpha;
            int origBeta = beta;

            uint64_t h0 = pos->currentHash;
            TTEntry e0 = getEntry(tt17, TT_SIZE_THREADED, h0);
            uint64_t pvMove = (thread_pvLength17[0] > 0) ? thread_pvTable17[0] : 0;
            uint16_t ttMove = (e0.key == h0) ? e0.bestResponse : 0;
            orderMoves17(pos, 0, moveCount, pvMove, ttMove,0);

            int currentScore = -INF;
            bool isFirstMove = true;

            for (int i = 1; i <= moveCount; i++) {
                uint64_t m = pos->moves[i];
                uint64_t h = pos->currentHash;
                move(pos, m);
                pos->currentDepth++;

                int s;
                if (isFirstMove) {
                    s = -search17(pos, depth - 1, -beta, -alpha, true,m,0);
                    isFirstMove = false;
                } else {
                    s = -search17(pos, depth - 1, -alpha - 1, -alpha, true,m,0);
                    if (s > alpha && s < beta) {
                        s = -search17(pos, depth - 1, -beta, -alpha, true,m,0);
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
                    updatePV(thread_pvTable17, thread_pvLength17, 0, m);
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

#ifdef CHESSBOT_WEB
        webSearchScore17 = score;
        webSearchDepth17 = depth;
#endif

        // Adjust the time budget for root instability and score drops.
        double timeMultiplier = 1.0;

        // The best root move changed.
        if (depth >= 5 && toSmallMove(bestMove) != toSmallMove(prevBestMove)) {
            timeMultiplier *= 1.5;
        }

        // The root score dropped sharply.
        if (depth >= 5 && score < prevScore - 40) {
            timeMultiplier *= 1.6;
        }

        prevBestMove = bestMove;
        prevScore = score;

        uint64_t dynamicTarget = (uint64_t)(targetTime * timeMultiplier);
        if (dynamicTarget > maxTime) dynamicTarget = maxTime;

        uint64_t elapsed = get_real_time_ms() - pos->startTime;

        // Stop after 55% of the adjusted budget rather than starting another iteration.
        if (
#ifdef CHESSBOT_BENCHMARK
            !benchmarkFixedMs &&
#endif
            elapsed >= (dynamicTarget * 55) / 100) {
            break;
        }
    }

    return bestMove;
}

