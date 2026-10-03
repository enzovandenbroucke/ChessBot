/* Private measurement protocol, not a public UCI implementation. */
#include <inttypes.h>
static int benchmarkDepth;
static uint64_t benchmarkFixedMs;
#define CHESSBOT_SEARCH_ITERATION(depth) (benchmarkDepth = (depth))
#define CHESSBOT_BENCHMARK

#if BENCH_VERSION == 10
#include "../bots/v10t.h"
#define SUFFIX 10
#elif BENCH_VERSION == 11
#include "../bots/v11t.h"
#define SUFFIX 11
#elif BENCH_VERSION == 12
#include "../bots/v12t.h"
#define SUFFIX 12
#elif BENCH_VERSION == 13
#include "../bots/v13t.h"
#define SUFFIX 13
#elif BENCH_VERSION == 14
#include "../bots/v14t.h"
#define SUFFIX 14
#elif BENCH_VERSION == 15
#include "../bots/v15t.h"
#define SUFFIX 15
#elif BENCH_VERSION == 16
#include "../bots/v16t.h"
#define SUFFIX 16
#elif BENCH_VERSION == 17
#include "../bots/v17t.h"
#define SUFFIX 17
#else
#error Unsupported benchmark version
#endif
#define JOIN_(a,b) a##b
#define JOIN(a,b) JOIN_(a,b)
__thread TTEntry* JOIN(tt,SUFFIX);
extern __thread PawnEntry pawnTable[16384];
static bool benchmarkBookEnabled;

static void resetSearch(void) {
    memset(JOIN(tt,SUFFIX), 0, TT_SIZE_THREADED * sizeof(TTEntry));
    memset(JOIN(thread_pvTable,SUFFIX), 0, sizeof(JOIN(thread_pvTable,SUFFIX)));
    memset(JOIN(thread_pvLength,SUFFIX), 0, sizeof(JOIN(thread_pvLength,SUFFIX)));
    memset(JOIN(thread_killers,SUFFIX), 0, sizeof(JOIN(thread_killers,SUFFIX)));
    memset(pawnTable, 0, 16384 * sizeof(PawnEntry));
    JOIN(inBook,SUFFIX) = benchmarkBookEnabled;
}

static void uciMove(uint64_t m, char out[6]) {
    if (!m) { strcpy(out, "0000"); return; }
    squareToString(out, prev(m));
    squareToString(out + 2, next(m));
    int p = uncolor(promotion(m));
    if (p) { out[4] = "  nbrqk"[p]; out[5] = 0; }
}

static bool pushUci(Position* pos, const char* text) {
    pos->currentDepth = 0;
    generateMoves(pos);
    for (int i = 1; i <= (int)pos->moves[0]; i++) {
        char candidate[6];
        uciMove(pos->moves[i], candidate);
        if (!strcmp(candidate, text)) { move(pos, pos->moves[i]); return true; }
    }
    return false;
}

static double wallMs(void) {
#ifdef _WIN32
    LARGE_INTEGER counter, frequency;
    QueryPerformanceCounter(&counter); QueryPerformanceFrequency(&frequency);
    return (double)counter.QuadPart * 1000.0 / (double)frequency.QuadPart;
#else
    struct timespec value;
    clock_gettime(CLOCK_MONOTONIC, &value);
    return value.tv_sec * 1000.0 + value.tv_nsec / 1000000.0;
#endif
}

int main(void) {
    /* Windows CRT does not guarantee line buffering for redirected pipes. */
    setvbuf(stdout, NULL, _IONBF, 0);
    srand(1);
    const char* bookSetting = getenv("CHESSBOT_BENCH_BOOK");
    benchmarkBookEnabled = bookSetting && !strcmp(bookSetting, "1");
    Position* pos = allocateMemory();
    if (benchmarkBookEnabled && (!bookEntries || bookSize <= 0)) {
        fprintf(stderr, "Requested benchmark opening book could not be loaded.\n");
        freePosition(pos); freeGlobalData(); return 2;
    }
    JOIN(tt,SUFFIX) = calloc(TT_SIZE_THREADED, sizeof(TTEntry));
    if (!pos || !JOIN(tt,SUFFIX)) return 2;
    resetSearch();
    char line[16384];
    while (fgets(line, sizeof(line), stdin)) {
        line[strcspn(line, "\r\n")] = 0;
        if (!strcmp(line, "quit")) break;
        if (!strcmp(line, "info")) {
            printf("{\"book_enabled\":%s,\"book_entries\":%ld}\n",
                   benchmarkBookEnabled ? "true" : "false", bookSize);
        }
        else if (!strcmp(line, "reset") || !strncmp(line, "reset ", 6)) {
            unsigned int seed = 1;
            if (line[5] == ' ') sscanf(line + 6, "%u", &seed);
            srand(seed); resetSearch(); puts("{\"ok\":true}");
        }
        else if (!strncmp(line, "position ", 9)) {
            char* moves = strchr(line + 9, '|');
            if (moves) *moves++ = 0;
            memset(pos, 0, sizeof(*pos));
            initBoard(pos, line + 9); /* Runner validates FEN before sending it. */
            bool ok = true;
            for (char* token = moves ? strtok(moves, " ") : NULL; token; token = strtok(NULL, " ")) {
                if (pos->historyPly >= 1024 || !pushUci(pos, token)) { ok = false; break; }
            }
            printf("{\"ok\":%s}\n", ok ? "true" : "false");
        } else if (!strncmp(line, "search ", 7)) {
            char mode[16]; uint64_t budget, increment = 0;
            if (sscanf(line + 7, "%15s %" SCNu64 " %" SCNu64, mode, &budget, &increment) < 2 ||
                !budget || budget > 3600000 || (strcmp(mode,"fixed") && strcmp(mode,"clock"))) {
                puts("{\"error\":\"invalid search budget\"}"); continue;
            }
            benchmarkDepth = 0; pos->nodeCount = 0;
            benchmarkFixedMs = !strcmp(mode, "fixed") ? budget : 0;
            bool previousBookState = JOIN(inBook,SUFFIX);
            if (benchmarkFixedMs) JOIN(inBook,SUFFIX) = false;
            double start = wallMs();
#if BENCH_VERSION == 17
            uint64_t m = getBestMoveDynamic17(pos, budget, increment);
#else
            uint64_t limit = benchmarkFixedMs ? budget : budget / 25 + increment / 2;
            if (!benchmarkFixedMs && limit > budget * 3 / 4) limit = budget * 3 / 4;
            if (!limit) limit = 1;
            uint64_t m = JOIN(getBestMoveTime,SUFFIX)(pos, (time_t)limit);
#endif
            double elapsed = wallMs() - start;
            bool usedBook = !benchmarkFixedMs && m && JOIN(inBook,SUFFIX);
            if (benchmarkFixedMs) JOIN(inBook,SUFFIX) = previousBookState;
            char text[6]; uciMove(m, text);
            printf("{\"move\":\"%s\",\"nodes\":%d,\"depth\":%d,\"elapsed_ms\":%.6f,\"book\":%s}\n",
                   text, pos->nodeCount, benchmarkDepth, elapsed, usedBook ? "true" : "false");
        } else puts("{\"error\":\"unknown command\"}");
    }
    free(JOIN(tt,SUFFIX)); freePosition(pos); freeGlobalData();
    return 0;
}
