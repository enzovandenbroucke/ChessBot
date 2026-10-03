#include "Game.h"
#include <limits.h>

/* Validate external input before passing it to the engine's trusted FEN parser.
 * This is a structural guard, not a proof that a position is reachable.
 */
static bool validFen(const char* fen) {
    char board[72], side[2], rights[5], ep[3], half[12], full[12], extra;
    if (!fen || strlen(fen) >= 128 ||
        sscanf(fen, "%71s %1s %4s %2s %11s %11s %c",
               board, side, rights, ep, half, full, &extra) != 6) return false;
    if (strcmp(side, "w") && strcmp(side, "b")) return false;
    int rank = 7, file = 0, whiteKings = 0, blackKings = 0;
    int pieces = 0, whiteKing = -1, blackKing = -1;
    char squares[64] = {0};
    for (const char* p = board; *p; p++) {
        if (*p == '/') {
            if (file != 8 || rank == 0) return false;
            rank--; file = 0;
        } else if (*p >= '1' && *p <= '8') {
            file += *p - '0';
            if (file > 8) return false;
        } else {
            if (!strchr("PNBRQKpnbrqk", *p) || file >= 8) return false;
            if ((*p == 'P' || *p == 'p') && (rank == 0 || rank == 7)) return false;
            int square = rank * 8 + file++;
            squares[square] = *p;
            if (*p == 'K') { whiteKings++; whiteKing = square; }
            if (*p == 'k') { blackKings++; blackKing = square; }
            pieces++;
        }
    }
    if (rank || file != 8 || whiteKings != 1 || blackKings != 1 || pieces > 32)
        return false;
    if (abs(whiteKing / 8 - blackKing / 8) <= 1 &&
        abs(whiteKing % 8 - blackKing % 8) <= 1) return false;
    if (strcmp(rights, "-")) {
        const char* order = "KQkq";
        int previous = -1;
        for (const char* p = rights; *p; p++) {
            const char* found = strchr(order, *p);
            if (!found || found - order <= previous) return false;
            previous = (int)(found - order);
            int kingSquare = isupper((unsigned char)*p) ? 4 : 60;
            int rookSquare = *p == 'K' ? 7 : *p == 'Q' ? 0 : *p == 'k' ? 63 : 56;
            if (squares[kingSquare] != (kingSquare == 4 ? 'K' : 'k') ||
                squares[rookSquare] != (kingSquare == 4 ? 'R' : 'r')) return false;
        }
    }
    if (strcmp(ep, "-")) {
        if (strlen(ep) != 2 || ep[0] < 'a' || ep[0] > 'h' ||
            ep[1] != (side[0] == 'w' ? '6' : '3')) return false;
        int target = (ep[1] - '1') * 8 + ep[0] - 'a';
        int pawnSquare = target + (side[0] == 'w' ? -8 : 8);
        if (squares[target] || squares[pawnSquare] != (side[0] == 'w' ? 'p' : 'P'))
            return false;
    }
    char* end;
    for (const char* p = half; *p; p++) if (!isdigit((unsigned char)*p)) return false;
    for (const char* p = full; *p; p++) if (!isdigit((unsigned char)*p)) return false;
    long halfCount = strtol(half, &end, 10);
    if (*end || halfCount < 0 || halfCount > 100) return false;
    long fullCount = strtol(full, &end, 10);
    return !*end && fullCount > 0 && fullCount < INT_MAX - UI_MAX_HISTORY;
}

void refreshGame(Game* game) {
    Position* pos = game->position;
    pos->currentDepth = 0;
    generateMoves(pos);
    game->legalCount = (int)pos->moves[0];
    if (game->legalCount > MAX_MOVES) {
        game->legalCount = 0;
        game->status = "Unsupported position: move count";
        return;
    }
    memcpy(game->legalMoves, pos->moves + 1, game->legalCount * sizeof(uint64_t));
    game->whiteScore = evalNewWhite(pos);
    if (!game->legalCount) game->status = isInCheck(pos) ? "Checkmate" : "Stalemate";
    else if (pos->halfMove >= 100) game->status = "Draw: fifty-move rule";
    else if (isRepetition(pos)) game->status = "Draw: engine repetition rule";
    else if (game->historyCursor >= UI_MAX_HISTORY) game->status = "History limit reached";
    else game->status = isInCheck(pos) ? "Check" : "Playing";
}

bool loadGame(Game* game, const char* fen) {
    if (!validFen(fen)) return false;
    Position* candidate = calloc(1, sizeof(Position));
    if (!candidate) return false;
    char copy[128];
    snprintf(copy, sizeof(copy), "%s", fen);
    initBoard(candidate, copy);
    /* The side to move may be in check; the other king cannot be capturable. */
    if (canCaptureKing(candidate)) { free(candidate); return false; }
    generateMoves(candidate);
    if (candidate->moves[0] > MAX_MOVES) { free(candidate); return false; }
    memcpy(game->position, candidate, sizeof(Position));
    free(candidate);
    game->historyCount = game->historyCursor = 0;
    refreshGame(game);
    return true;
}

uint64_t findGameMove(const Game* game, int from, int to, int promotionType) {
    for (int i = 0; i < game->legalCount; i++) {
        uint64_t m = game->legalMoves[i];
        if (prev(m) == from && next(m) == to &&
            (isVacant(promotion(m)) || uncolor(promotion(m)) == promotionType)) return m;
    }
    return 0;
}

bool playGameMove(Game* game, uint64_t candidate) {
    if (!candidate || game->historyCursor >= UI_MAX_HISTORY ||
        game->position->halfMove >= 100 || isRepetition(game->position)) return false;
    for (int i = 0; i < game->legalCount; i++) {
        uint64_t m = game->legalMoves[i];
        if (toSmallMove(m) != toSmallMove(candidate)) continue;
        /* Use freshly generated moves, including their reversible state. */
        move(game->position, m);
        game->history[game->historyCursor++] = m;
        game->historyCount = game->historyCursor;
        refreshGame(game);
        return true;
    }
    return false;
}

bool undoGameMove(Game* game) {
    if (!game->historyCursor) return false;
    unmove(game->position, game->history[--game->historyCursor]);
    refreshGame(game);
    return true;
}

bool redoGameMove(Game* game) {
    if (game->historyCursor >= game->historyCount) return false;
    move(game->position, game->history[game->historyCursor++]);
    refreshGame(game);
    return true;
}
