#include "../ui/Game.h"
#include "../bots/v17at.h"
#include <emscripten/emscripten.h>

__thread TTEntry* tt17a = NULL;
static Game game;
static char output[24576];

EMSCRIPTEN_KEEPALIVE int cb_init(void) {
    if (!game.position) game.position = calloc(1, sizeof(Position));
    return game.position && loadGame(&game, START_FEN);
}

EMSCRIPTEN_KEEPALIVE int cb_load(const char* fen) {
    if (!game.position && !cb_init()) return 0;
    if (!loadGame(&game, fen)) return 0;
    if (tt17a) clearTableSize(tt17a, TT_SIZE_THREADED);
    return 1;
}

EMSCRIPTEN_KEEPALIVE int cb_move(int from, int to, int promote) {
    if (!game.position || from < 0 || from >= 64 || to < 0 || to >= 64) return 0;
    return playGameMove(&game, findGameMove(&game, from, to, promote));
}

EMSCRIPTEN_KEEPALIVE int cb_undo(void) { return game.position && undoGameMove(&game); }
EMSCRIPTEN_KEEPALIVE int cb_redo(void) { return game.position && redoGameMove(&game); }

EMSCRIPTEN_KEEPALIVE const char* cb_state(void) {
    if (!game.position) return "{}";
    Position* pos = game.position;
    char fen[128]; boardToFen(pos, fen);
    int result = !game.legalCount ? (isInCheck(pos) ? 1 : 2) :
        pos->halfMove >= 100 ? 3 : isRepetition(pos) ? 4 :
        game.historyCursor >= UI_MAX_HISTORY ? 5 : 0;
    int used = snprintf(output, sizeof(output),
        "{\"fen\":\"%s\",\"whiteToMove\":%s,\"staticScore\":%d,\"result\":%d,"
        "\"check\":%s,\"cursor\":%d,\"historyCount\":%d,\"legal\":[",
        fen, pos->whiteToMove ? "true" : "false", game.whiteScore, result,
        isInCheck(pos) ? "true" : "false", game.historyCursor, game.historyCount);
    for (int i = 0; i < game.legalCount; i++) {
        uint64_t m = game.legalMoves[i];
        used += snprintf(output + used, sizeof(output) - used,
            "%s{\"from\":%d,\"to\":%d,\"promotion\":%d}", i ? "," : "",
            prev(m), next(m), isVacant(promotion(m)) ? 0 : uncolor(promotion(m)));
    }
    snprintf(output + used, sizeof(output) - used, "]}");
    return output;
}

EMSCRIPTEN_KEEPALIVE const char* cb_search(int timeLeftMs, int incrementMs) {
    if (!game.position || !game.legalCount || game.position->halfMove >= 100 ||
        isRepetition(game.position) || timeLeftMs <= 0) return "{\"move\":null}";
    if (!tt17a) tt17a = calloc(TT_SIZE_THREADED, sizeof(TTEntry));
    Position* snapshot = malloc(sizeof(Position));
    if (!tt17a || !snapshot) { free(snapshot); return "{\"error\":\"Search allocation failed\"}"; }
    memcpy(snapshot, game.position, sizeof(Position));
    bool white = snapshot->whiteToMove;
    inBook17a = false; /* The standalone browser build has no opening book. */
    uint64_t start = get_real_time_ms();
    uint64_t best = getBestMoveDynamic17a(snapshot, (uint64_t)timeLeftMs,
                                        (uint64_t)(incrementMs > 0 ? incrementMs : 0));
    int used = snprintf(output, sizeof(output),
        "{\"move\":{\"from\":%d,\"to\":%d,\"promotion\":%d},"
        "\"score\":%d,\"depth\":%d,\"nodes\":%d,\"elapsed\":%llu,\"pv\":[",
        prev(best), next(best), !best || isVacant(promotion(best)) ? 0 : uncolor(promotion(best)),
        white ? webSearchScore17a : -webSearchScore17a, webSearchDepth17a,
        snapshot->nodeCount, (unsigned long long)(get_real_time_ms() - start));
    for (int i = 0; i < thread_pvLength17a[0] && i < MAX_DEPTH && webSearchDepth17a > 0; i++) {
        uint64_t m = thread_pvTable17a[i];
        char from[3], to[3], suffix[2] = {0};
        squareToString(from, prev(m)); squareToString(to, next(m));
        if (!isVacant(promotion(m))) suffix[0] = "  nbrqk"[uncolor(promotion(m))];
        used += snprintf(output + used, sizeof(output) - used, "%s\"%s%s%s\"",
                         i ? "," : "", from, to, suffix);
    }
    snprintf(output + used, sizeof(output) - used, "]}");
    free(snapshot);
    return output;
}
