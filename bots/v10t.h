#include "../Bot.h"
#include <assert.h>

#define NULL_MOVE_CONST10 3

__thread int ttCutoffCount10 = 0;
__thread uint64_t thread_pvTable10[MAX_DEPTH*(MAX_DEPTH+1)];
__thread int thread_pvLength10[MAX_DEPTH+1];
__thread uint16_t thread_killers10[MAX_DEPTH*2];
__thread bool inBook10 = true;
extern __thread TTEntry* tt10;
Bot v10t;

static inline void storeKiller10(int ply, uint64_t m){
    uint16_t mv = toSmallMove(m);
    if(isVacant(captured(m)) && thread_killers10[2*ply] != mv){
        thread_killers10[2*ply+1] = thread_killers10[2*ply];
        thread_killers10[2*ply] = mv;
    }
}

void orderMoves10(Position* pos, int startIndex, int moveCount, uint64_t pvMove, uint16_t ttMove) {
    // 1. Promote PV and TT
    if (pvMove != 0 && promoteFullMove(pos, pvMove, startIndex + 1, startIndex + moveCount)) {
        startIndex++; moveCount--;
    }
    if (ttMove != 0 && ttMove != toSmallMove(pvMove) && promoteSmallMove(pos, ttMove, startIndex + 1, startIndex + moveCount)) {
        startIndex++; moveCount--;
    }

    int ply = pos->currentDepth;
    uint16_t killerMove1 = thread_killers10[2 * ply];
    uint16_t killerMove2 = thread_killers10[2 * ply + 1];
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
                pos->moves[startIndex + i] = m | KILLERMOVE1_BONUS;
            } else if (newKiller2 && killerMove2 == toSmallMove(m)) {
                pos->moves[startIndex + i] = m | KILLERMOVE2_BONUS;
            }
        }
    }

    // Sort by the packed move score.

    insertionSort(pos, startIndex, moveCount);
}

int quiescenceSearch10(Position* pos, int startIndex, int alpha, int beta) {

    if (pos->stopSearch || (pos->historyPly > 0 && (pos->halfMove >= 100 || isRepetition(pos)))) return 0;

    if ((pos->nodeCount++ & 2047) == 0) {
        if (get_real_time_ms() - pos->startTime > pos->timeLimit) {
            pos->stopSearch = true;
            return 0;
        }
    }

    uint64_t h = pos->currentHash;
    TTEntry e = getEntry(tt10,TT_SIZE_THREADED,h);
    int originalAlpha = alpha;
    if (e.key == h) {
        int stored = scoreFromTT(e.eval,pos->currentDepth);
        if (e.flag == FLAG_EXACT) {
            thread_pvLength10[pos->currentDepth] = 0;
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

    if(isInCheck(pos)){
        generatePLMoves(pos);
    } else {
        // Stand-pat score.
        int stand_pat = staticEval(pos);

        if (stand_pat >= beta) {
            addEntry(tt10, TT_SIZE_THREADED, h, scoreToTT(stand_pat, pos->currentDepth),
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
    uint64_t pvMove = (thread_pvLength10[ply] > 0) ? thread_pvTable10[ply * MAX_DEPTH] : 0;
    thread_pvLength10[pos->currentDepth] = 0;
    uint16_t ttMove = (e.key == h) ? e.bestResponse : 0;

    moveCount = orderCapturesQS(pos, startIndex, moveCount, pvMove, ttMove);

    for (int i = 1; i <= moveCount; i++) {
        uint64_t m = pos->moves[startIndex + i];
        uint64_t h = pos->currentHash;

        move(pos, m);
        pos->currentDepth++;

        if (canCaptureKing(pos)) {
            unmove(pos, m);
            pos->currentDepth--;
            pos->currentHash = h;
            continue;
        }

        anyLegal = true;

        int score = -quiescenceSearch10(pos, startIndex + 256, -beta, -alpha);

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
            addEntry(tt10, TT_SIZE_THREADED, pos->currentHash, scoreToTT(score, pos->currentDepth),
                     toSmallMove(m), 0, FLAG_BETA);

            thread_pvTable10[pos->currentDepth * MAX_DEPTH] = m;
            thread_pvLength10[pos->currentDepth] = 1;
            return res;
        }
    }

    if (!anyLegal && isInCheck(pos)) {
        return -INF + pos->currentDepth;
    }

    uint8_t flag = (res <= originalAlpha) ? FLAG_ALPHA : FLAG_EXACT;
    addEntry(tt10, TT_SIZE_THREADED, h, scoreToTT(res, pos->currentDepth), 0, 0, flag);
    return res;
}

int search10(Position* pos, int startIndex, int depth,int alpha, int beta, bool allowNull){

    if(pos->stopSearch || (pos->historyPly > 0 && (pos->halfMove >= 100 || isRepetition(pos)))) return 0;
    if((pos->nodeCount++ & 2047) == 0){
        if(get_real_time_ms() - pos->startTime > pos->timeLimit){
            pos->stopSearch = true;
            return 0;
        }
    }

    uint64_t h = pos->currentHash;
    TTEntry e = getEntry(tt10, TT_SIZE_THREADED, h);

    int originalAlpha = alpha;
    if (e.key == h && e.depth >= depth) {
        int storedScore = scoreFromTT(e.eval,pos->currentDepth);
        if (e.flag == FLAG_EXACT) {
            ttCutoffCount10++;
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

    if(depth == 0) {

        int res = quiescenceSearch10(pos,startIndex,alpha, beta);

        return res;
    }

    if(allowNull && depth-1-NULL_MOVE_CONST10>=0 && !isInCheck(pos) && hasNonPawnMaterial(pos)){

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

        int nmScore = -search10(pos,startIndex+256,depth-1-NULL_MOVE_CONST10,-beta,-beta+1,false);

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
    uint64_t pvMove = thread_pvLength10[ply] > 0 ? thread_pvTable10[ply * MAX_DEPTH] : 0;
    thread_pvLength10[pos->currentDepth] = 0;

    uint16_t ttMove = e.key == h ? e.bestResponse : 0;
    orderMoves10(pos,startIndex,moveCount,pvMove,ttMove);

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
            score = -search10(pos,startIndex+256,depth-1,-beta,-alpha,true);
            isFirstMove = false;
        } else {
            int reduction = 0;
            // Reduce late moves before a full-depth re-search.
            if (depth >= 3 && i > 3 && (m >> 41) <= 10) {
                reduction = 1;

            }

            // 2. Reduced depth, null-window search
            score = -search10(pos, startIndex + 256, depth - 1 - reduction, -alpha - 1, -alpha, true);

            // 3. If it failed high AND we reduced it, re-search at full depth (null-window)
            if (score > alpha && reduction > 0) {
                score = -search10(pos, startIndex + 256, depth - 1, -alpha - 1, -alpha, true);
            }

            // 4. PVS Re-search (exact window) if it improved alpha
            if (score > alpha && score < beta) {
                score = -search10(pos, startIndex + 256, depth - 1, -beta, -alpha, true);
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
                thread_pvTable10[pos->currentDepth*MAX_DEPTH + 0] = m;
                memcpy(&thread_pvTable10[pos->currentDepth*MAX_DEPTH+1], &thread_pvTable10[(pos->currentDepth+1)*MAX_DEPTH], thread_pvLength10[pos->currentDepth+1] * sizeof(uint64_t));
                thread_pvLength10[pos->currentDepth] = thread_pvLength10[pos->currentDepth+1] + 1;
            }
        }
        if (score >= beta){
            addEntry(tt10, TT_SIZE_THREADED, pos->currentHash, scoreToTT(score, pos->currentDepth), toSmallMove(m), (uint8_t) depth, FLAG_BETA);
            storeKiller10(pos->currentDepth,m);
            thread_pvTable10[pos->currentDepth * MAX_DEPTH] = m;
            thread_pvLength10[pos->currentDepth] = 1;
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
        addEntry(tt10, TT_SIZE_THREADED, pos->currentHash, scoreToTT(res, pos->currentDepth), 0, (uint8_t) depth, flag);
    } else {
        addEntry(tt10, TT_SIZE_THREADED, pos->currentHash, scoreToTT(res, pos->currentDepth), toSmallMove(bestResponse), (uint8_t) depth, flag);
    }
    return res;
}

uint64_t getBestMoveTime10(Position* pos,time_t limit){

    if(inBook10){
        uint64_t bookMove = makeFromOpeningMove(pos);
        if(bookMove == 0){
            inBook10 = false;
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
    ttCutoffCount10 = 0;
    thread_pvLength10[0] = 0;
    for(int i = 0; i < 2*MAX_DEPTH; i++){
        thread_killers10[i]=0;
    }

    generateMoves(pos);
    int moveCount = (int) pos->moves[0] ;
    if(moveCount == 0) return 0 ;
    uint64_t bestMove = pos->moves[1];

    for(int depth = 1; depth <= 64; depth++){
        int alpha = -INF;
        int beta = INF;
        uint64_t candidate = 0;

        uint64_t h0 = pos->currentHash;
        TTEntry e0 = getEntry(tt10, TT_SIZE_THREADED, h0);
        int offset = 0;
        uint64_t pvMove = thread_pvTable10[0];
        uint16_t ttMove = (e0.key == h0) ? e0.bestResponse : 0;
        uint16_t killerMove1 = thread_killers10[0];
        uint16_t killerMove2 = thread_killers10[1];
        if( thread_pvLength10[0]>0 && promoteFullMove(pos,pvMove,1+offset,moveCount))  offset++;
        if( ttMove != 0 && e0.key == h0 && (thread_pvLength10[0]==0 || ttMove != toSmallMove(pvMove))
        && promoteSmallMove(pos,ttMove,1+offset,moveCount))        offset++;
        if( killerMove1 != 0 && killerMove1 != toSmallMove(pvMove) && killerMove1 != ttMove
        && promoteSmallMove(pos,killerMove1,1+offset,moveCount))   offset++;
        if( killerMove2 != 0 && killerMove2 != toSmallMove(pvMove) && killerMove2 != ttMove
        && promoteSmallMove(pos,killerMove2,1+offset,moveCount))   offset++;
        insertionSort(pos,offset,moveCount-offset);

        bool isFirstMove = true;

        for(int i = 1 ; i <= moveCount ; i++){
            uint64_t h = pos->currentHash;
            uint64_t m = pos->moves[i];
            move(pos,m);
            pos->currentDepth++;

            int score;
            if(isFirstMove){
                score = -search10(pos,256*pos->currentDepth, depth-1, -beta,-alpha,true);
                isFirstMove = false;
            } else {
                score = -search10(pos,256*pos->currentDepth, depth-1, -alpha-1,-alpha,true);
                if(score > alpha && score < beta) score = -search10(pos,256*pos->currentDepth, depth-1, -beta,-alpha,true);
            }

            unmove(pos,m);
            pos->currentHash = h;
            pos->currentDepth--;

            if(pos->stopSearch) break;

            if(score > alpha){
                alpha = score;
                candidate = m;
                thread_pvTable10[0] = m;
                memcpy(&thread_pvTable10[1], &thread_pvTable10[MAX_DEPTH], thread_pvLength10[1] * sizeof(uint64_t));
                thread_pvLength10[0] = thread_pvLength10[1] + 1;
            }
        }
        if(pos->stopSearch) break;
        if(candidate != 0) bestMove = candidate;
        CHESSBOT_SEARCH_ITERATION(depth);
    }

    return bestMove;
}

