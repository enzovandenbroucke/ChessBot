#include "../Bot.h"
#include <assert.h>

#define NULL_MOVE_CONST16d 3
#define HISTORY_MAX16d 16384
#define HIST_BONUS_MAX16d 96
#define ASPIRATION_WINDOW16d 50
#define GOOD_CAPTURE_BONUS16d (128ULL << 41)
#define COUNTERMOVE_BONUS16d ((uint64_t) 98 << 41) // just under Killer 2 (100)

__thread int cpt16d = 0;
__thread uint64_t thread_pvTable16d[MAX_DEPTH*(MAX_DEPTH+1)];
__thread int thread_pvLength16d[MAX_DEPTH+1];
__thread uint16_t thread_killers16d[MAX_DEPTH*2];
__thread bool inBook16d = true;
__thread int historyTable16d[2][64][64]; // [color][from][to]
__thread uint16_t thread_counterMoves16d[64][64]; // [opponentFrom][opponentTo]
extern __thread TTEntry* tt16d;
Bot v16dt;

static inline void updateHistoryKillersAndCounter16d(Position* pos, uint64_t bestMove,
    int depth,uint64_t* quietsSearched, int numQuiets,uint64_t prevMove) {
    int bonus = depth * depth;
    if (bonus > 400) bonus = 400;

    // Reward the quiet cutoff move.
    int c = pos->whiteToMove ? WHITE_INDEX : BLACK_INDEX;
    int cur = historyTable16d[c][prev(bestMove)][next(bestMove)];
    historyTable16d[c][prev(bestMove)][next(bestMove)] += bonus - (cur * bonus) / HISTORY_MAX16d;

    // Penalize earlier quiet moves that failed to cause a cutoff.
    for (int i = 0; i < numQuiets; i++) {
        uint64_t qm = quietsSearched[i];
        int qc = colorIndex(pos->board[prev(qm)]);
        int qcur = historyTable16d[qc][prev(qm)][next(qm)];
        historyTable16d[qc][prev(qm)][next(qm)] -= bonus + (qcur * bonus) / HISTORY_MAX16d;
    }

    uint16_t mv = toSmallMove(bestMove);
    int ply = pos->currentDepth;
    if (thread_killers16d[2 * ply] != mv) {
        thread_killers16d[2 * ply + 1] = thread_killers16d[2 * ply];
        thread_killers16d[2 * ply] = mv;
    }

    // Associate the countermove with the preceding opponent move.
    if (prevMove != 0) {
        thread_counterMoves16d[prev(prevMove)][next(prevMove)] = mv;
    }
}

void orderMoves16d(Position* pos, int startIndex, int moveCount, uint64_t pvMove, uint16_t ttMove, uint64_t prevMove) {
    if (pvMove != 0 && promoteSmallMove(pos, toSmallMove(pvMove), startIndex + 1, startIndex + moveCount)) {
        startIndex++; moveCount--;
    }
    if (ttMove != 0 && ttMove != toSmallMove(pvMove) && promoteSmallMove(pos, ttMove, startIndex + 1, startIndex + moveCount)) {
        startIndex++; moveCount--;
    }

    int ply = pos->currentDepth;
    uint16_t killer1 = thread_killers16d[2 * ply];
    uint16_t killer2 = thread_killers16d[2 * ply + 1];
    uint16_t cmMove = (prevMove != 0) ? thread_counterMoves16d[prev(prevMove)][next(prevMove)] : 0;

    for (int i = 1; i <= moveCount; i++) {
        uint64_t m = pos->moves[startIndex + i] & WITHOUT_MVSCORE_CACHE;
        uint16_t sm = toSmallMove(m);

        if (!isVacant(captured(m)) || !isVacant(promotion(m))) {
            int s = see(pos, m);
            if (s >= 0) {
                m |= GOOD_CAPTURE_BONUS16d;
                m += (((uint64_t)(s / 100)) << 41);
            }
        } else {
            if (killer1 != 0 && sm == killer1) {
                m |= KILLERMOVE1_BONUS;
            } else if (killer2 != 0 && sm == killer2) {
                m |= KILLERMOVE2_BONUS;
            } else if (cmMove != 0 && sm == cmMove) {
                m |= COUNTERMOVE_BONUS16d;
            } else {
                int c = colorIndex(pos->board[prev(m)]);
                int rawScore = historyTable16d[c][prev(m)][next(m)];
                if (rawScore < 0) rawScore = 0;
                uint64_t histBonus = (uint64_t)((rawScore * HIST_BONUS_MAX16d) / HISTORY_MAX16d);
                m |= (histBonus << 41);
            }
        }
        pos->moves[startIndex + i] = m;
    }
    insertionSort(pos, startIndex, moveCount);
}

int quiescenceSearch16d(Position* pos, int alpha, int beta) {
    int startIndex = 256 * pos->currentDepth;

    if(checkTimeOnly(pos)) return 0;

    uint64_t h = pos->currentHash;
    TTEntry e = getEntry(tt16d,TT_SIZE_THREADED,h);
    int originalAlpha = alpha;
    if (e.key == h) {
        int stored = scoreFromTT(e.eval,pos->currentDepth);
        if (e.flag == FLAG_EXACT) {
            thread_pvLength16d[pos->currentDepth] = 0;
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
        return eval(pos);
    }

    int res = -INF;
    int stand_pat = eval(pos);

    if(isInCheck(pos)){
        generatePLMoves(pos);
    } else {
        // Stand-pat score.

        if (stand_pat >= beta) {
            addEntry(tt16d, TT_SIZE_THREADED, h, scoreToTT(stand_pat, pos->currentDepth),
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

    uint64_t pvMove = (pos->currentDepth == 0 && thread_pvLength16d[0] > 0) ? thread_pvTable16d[0] : 0;
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

        int score = -quiescenceSearch16d(pos, -beta, -alpha);

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
            addEntry(tt16d, TT_SIZE_THREADED, pos->currentHash, scoreToTT(score, pos->currentDepth),
                     toSmallMove(m), 0, FLAG_BETA);

            thread_pvTable16d[pos->currentDepth * MAX_DEPTH] = m;
            thread_pvLength16d[pos->currentDepth] = 1;
            return res;
        }
    }

    if (!anyLegal && isInCheck(pos)) {
        return -INF + pos->currentDepth;
    }

    uint8_t flag = (res <= originalAlpha) ? FLAG_ALPHA : FLAG_EXACT;
    addEntry(tt16d, TT_SIZE_THREADED, h, scoreToTT(res, pos->currentDepth), 0, 0, flag);
    return res;
}

int search16d(Position* pos, int depth, int alpha, int beta, bool allowNull, uint64_t prevMove, uint16_t excludedMove){
    int startIndex = 256 * pos->currentDepth;

    if (pos->currentDepth >= MAX_DEPTH - 1) {
        return eval(pos);
    }

    if (checkStopConditions(pos)) return 0;

    uint64_t h = pos->currentHash;
    TTEntry e = getEntry(tt16d, TT_SIZE_THREADED, h);

    int originalAlpha = alpha;
    if (excludedMove == 0 && e.key == h && e.depth >= depth) {
        int storedScore = scoreFromTT(e.eval,pos->currentDepth);
        if (e.flag == FLAG_EXACT) {
            cpt16d++;
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
        int res = quiescenceSearch16d(pos,alpha, beta);
        return res;
    }
    bool inCheck = isInCheck(pos);

    if (depth >= 4 && !inCheck && excludedMove == 0 && (e.key != h || e.bestResponse == 0)) {
        depth--;
    }

    if (depth == 1 && !inCheck && abs(beta) < MATE_THRESHOLD) {
        int se = eval(pos);
        if (se + 300 <= alpha) {
            int score = quiescenceSearch16d(pos, alpha, beta);
            if (score <= alpha) return score; // Prune only after the reduced search confirms fail-low.
        }
    }

    int extension = 0;
    if(inCheck) extension = 1; // Check extension.

    bool futilityPrune = false;
    if (depth <= 3 && !inCheck && abs(beta) < MATE_THRESHOLD && hasNonPawnMaterial(pos)) {
        int margin = 120 * depth; // 120 cp per remaining ply.
        int futilityMargin = 100 * depth; // 100 cp per remaining ply.
        int se = eval(pos);
        if (se - margin >= beta) {
            return se - margin; // Reverse futility cutoff.
        }
        if (se + futilityMargin <= alpha) {
           futilityPrune = true;  // will skip quiet moves in the loop
        }
    }

    if (allowNull && depth - 1 - NULL_MOVE_CONST16d >= 0 && !inCheck && hasNonPawnMaterial(pos)) {
        NullMoveState nms;
        makeNullMove(pos, &nms);
        int nmScore = -search16d(pos, depth - 1 - NULL_MOVE_CONST16d, -beta, -beta + 1, false,0,0);
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
        int sScore = search16d(pos, singularDepth, singularBeta - 1, singularBeta, false, prevMove, ttMove);

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
    uint64_t pvMove = thread_pvLength16d[ply] > 0 ? thread_pvTable16d[ply * MAX_DEPTH] : 0;
    thread_pvLength16d[pos->currentDepth] = 0;

    orderMoves16d(pos,startIndex,moveCount,pvMove,ttMove,prevMove);

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
        bool isKiller = (toSmallMove(m) == thread_killers16d[2 * ply]) || (toSmallMove(m) == thread_killers16d[2 * ply + 1]);

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

        if (isQuiet) {
            quietsSearched[numQuiets++] = m;
        }

        int score;
        if(isFirstMove){
            int ext = extension;
            if(toSmallMove(m) == ttMove){
                ext += singularExtension;
            }
            score = -search16d(pos, depth - 1 + ext, -beta, -alpha, true, m, 0);
            isFirstMove = false;
        } else {
            int reduction = 0;
            // Reduce late moves before a full-depth re-search.
            bool isBadCapture = !isVacant(captured(m)) && !(m & GOOD_CAPTURE_BONUS16d);

            if (!inCheck && depth >= 3 && i >= 3 && (isQuiet || isBadCapture) &&
                toSmallMove(m) != thread_killers16d[2 * ply] &&
                toSmallMove(m) != thread_killers16d[2 * ply + 1]) {

                int k = (i > 255) ? 255 : i;
                reduction = lmrDepth[k];

                if (isQuiet) {
                    // Adjust quiet-move reductions using history.
                   int c = colorIndex(pos->board[prev(m)]);
                    int hist = historyTable16d[c][prev(m)][next(m)];
                    if (hist > (HISTORY_MAX16d / 2)) reduction--;
                    else if (hist < (HISTORY_MAX16d / 8)) reduction++;
                } else {
                    // Bad captures do not use quiet-move history.

                    if (reduction > 2) reduction = 2;
                }

                if (reduction < 1) reduction = 1;
                if (reduction > depth - 2) reduction = depth - 2;
            }

            // 2. Reduced depth, null-window search
            score = -search16d(pos, depth - 1 - reduction + extension, -alpha - 1, -alpha, true,m,0);

            // 3. If it failed high AND we reduced it, re-search at full depth (null-window)
            if (score > alpha && reduction > 0) {
                score = -search16d(pos, depth - 1 + extension, -alpha - 1, -alpha, true,m,0);
            }

            // 4. PVS Re-search (exact window) if it improved alpha
            if (score > alpha && score < beta) {
                score = -search16d(pos, depth - 1 + extension, -beta, -alpha, true,m,0);
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
                if(excludedMove == 0) updatePV(thread_pvTable16d, thread_pvLength16d, pos->currentDepth, m);
            }
        }
        if (score >= beta){
            if (isQuiet) {
                numQuiets--; // Exclude the cutoff move from the history penalties.
                updateHistoryKillersAndCounter16d(pos, m, depth, quietsSearched, numQuiets, prevMove);
            }
            if(excludedMove == 0){
                addEntry(tt16d, TT_SIZE_THREADED, pos->currentHash, scoreToTT(score, pos->currentDepth), toSmallMove(m), (uint8_t) depth, FLAG_BETA);
                thread_pvTable16d[pos->currentDepth * MAX_DEPTH] = m;
                thread_pvLength16d[pos->currentDepth] = 1;
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
        addEntry(tt16d, TT_SIZE_THREADED, h, scoreToTT(res, pos->currentDepth), moveOrKeep, (uint8_t)depth, FLAG_ALPHA);
    } else if (excludedMove == 0) {
        addEntry(tt16d, TT_SIZE_THREADED, h, scoreToTT(res, pos->currentDepth), toSmallMove(bestResponse), (uint8_t)depth, FLAG_EXACT);
    }
    return res;
}

uint64_t getBestMoveTime16d(Position* pos,time_t limit){

    if(inBook16d){
        uint64_t bookMove = makeFromOpeningMove(pos);
        if(bookMove == 0){
            inBook16d = false;
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
    cpt16d = 0;
    thread_pvLength16d[0] = 0;

    memset(historyTable16d, 0, sizeof(historyTable16d));
    memset(thread_killers16d, 0, sizeof(thread_killers16d));
    memset(thread_counterMoves16d, 0, sizeof(thread_counterMoves16d));

    generateMoves(pos);
    int moveCount = (int) pos->moves[0] ;
    if(moveCount == 0) return 0 ;

    int score = eval(pos);
    uint64_t bestMove = pos->moves[1];

    for (int depth = 1; depth <= 64; depth++) {
        int delta = ASPIRATION_WINDOW16d;
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
            TTEntry e0 = getEntry(tt16d, TT_SIZE_THREADED, h0);
            uint64_t pvMove = (thread_pvLength16d[0] > 0) ? thread_pvTable16d[0] : 0;
            uint16_t ttMove = (e0.key == h0) ? e0.bestResponse : 0;
            orderMoves16d(pos, 0, moveCount, pvMove, ttMove,0);

            int currentScore = -INF;
            bool isFirstMove = true;

            for (int i = 1; i <= moveCount; i++) {
                uint64_t m = pos->moves[i];
                uint64_t h = pos->currentHash;
                move(pos, m);
                pos->currentDepth++;

                int s;
                if (isFirstMove) {
                    s = -search16d(pos, depth - 1, -beta, -alpha, true,m,0);
                    isFirstMove = false;
                } else {
                    s = -search16d(pos, depth - 1, -alpha - 1, -alpha, true,m,0);
                    if (s > alpha && s < beta) {
                        s = -search16d(pos, depth - 1, -beta, -alpha, true,m,0);
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
                    updatePV(thread_pvTable16d, thread_pvLength16d, 0, m);
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

