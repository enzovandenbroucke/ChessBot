#include <stdio.h>
#include <math.h>
#include <pthread.h>
#include "Paths.h"
#include <stdlib.h>
#include <stdbool.h>

#include "bots/v15t.h"
#include "bots/v17at.h"
#include "bots/stockfish.h"

// Shared tournament state.
typedef struct {
    int totalGames;
    int gamesPlayed;
    int winsA;
    int winsB;
    int draws;
    Bot* botA;
    Bot* botB;
    uint64_t baseTimeMs;
    uint64_t incMs;
    char** fens;
    int numFens;
    pthread_mutex_t lock;
} TournamentContext;

// Shared calibration state.

typedef struct {
    Bot bot;
    int targetElo;
    uint64_t baseTimeMs;
    uint64_t incMs;
    int totalGames;
    int gamesPlayed;
    int victories;
    int draws;
    int losses;
    double sumSq;
    char** fens;
    int numFens;
    pthread_mutex_t lock;
    pthread_mutex_t sfLock;
} CalibrationContext;

__thread TTEntry* tt17a = NULL;
__thread TTEntry* tt15 = NULL;

int botAgainstBot(Position* pos, Bot w, Bot b, uint64_t baseTimeMs, uint64_t incMs) {
    uint64_t wTime = baseTimeMs;
    uint64_t bTime = baseTimeMs;

    while (true) {
        if (pos->halfMove >= 100 || isRepetition(pos)) return 0;

        uint64_t m = 0;
        uint64_t t0 = get_real_time_ms();

        if (pos->whiteToMove) {
            m = w.play(pos, wTime, incMs);
            uint64_t elapsed = get_real_time_ms() - t0;

            if (elapsed > wTime) return -1; // White loses on time.
            wTime = wTime - elapsed + incMs;
        } else {
            m = b.play(pos, bTime, incMs);
            uint64_t elapsed = get_real_time_ms() - t0;

            if (elapsed > bTime) return 1;  // Black loses on time.
            bTime = bTime - elapsed + incMs;
        }

        if (m == 0) {
            if (pos->whiteToMove && isAttacked(pos, pos->KSquare, true)) return -1;
            if (!pos->whiteToMove && isAttacked(pos, pos->kSquare, false)) return 1;
            return 0;
        }

        move(pos, m);
    }
}

int matchAgainstStaticStockfish(Position* pos, Bot bot, int targetElo, bool botIsWhite,
                                               uint64_t baseTimeMs, uint64_t incMs) {
    uint64_t wTime = baseTimeMs;
    uint64_t bTime = baseTimeMs;

    while (true) {
        if (pos->halfMove >= 100 || isRepetition(pos)) return 0;

        uint64_t m = 0;
        uint64_t t0 = get_real_time_ms();
        bool isBotTurn = (pos->whiteToMove == botIsWhite);
        uint64_t* activeClock = pos->whiteToMove ? &wTime : &bTime;

        if (isBotTurn) {
            m = bot.play(pos, *activeClock, incMs);
        } else {
            m = getStockfishMove(pos, targetElo);
        }

        uint64_t elapsed = get_real_time_ms() - t0;

        if (elapsed > *activeClock) {
            if (pos->whiteToMove) return botIsWhite ? -1 : 1;
            else                  return botIsWhite ? 1 : -1;
        }

        *activeClock = *activeClock - elapsed + incMs;

        if (m == 0) {
            if (pos->whiteToMove && isAttacked(pos, pos->KSquare, true)) return botIsWhite ? -1 : 1;
            if (!pos->whiteToMove && isAttacked(pos, pos->kSquare, false)) return botIsWhite ? 1 : -1;
            return 0;
        }

        move(pos, m);
    }
}

int matchAgainstStockfish(Position* pos, Bot bot, int targetElo, bool botIsWhite,
                          uint64_t baseTimeMs, uint64_t incMs) {
    uint64_t wTime = baseTimeMs;
    uint64_t bTime = baseTimeMs;

    while (true) {
        if (pos->halfMove >= 100 || isRepetition(pos)) return 0;

        uint64_t m = 0;
        uint64_t t0 = get_real_time_ms();
        bool isBotTurn = (pos->whiteToMove == botIsWhite);
        uint64_t* activeClock = pos->whiteToMove ? &wTime : &bTime;

        if (isBotTurn) {
            m = bot.play(pos, *activeClock, incMs);
        } else {
            m = getStockfishMoveDynamic(pos, targetElo, wTime, bTime, incMs);
        }

        uint64_t elapsed = get_real_time_ms() - t0;

        if (elapsed > *activeClock) {
            if (pos->whiteToMove) return botIsWhite ? -1 : 1;
            else                  return botIsWhite ? 1 : -1;
        }

        *activeClock = *activeClock - elapsed + incMs;

        if (m == 0) {
            if (pos->whiteToMove && isAttacked(pos, pos->KSquare, true)) return botIsWhite ? -1 : 1;
            if (!pos->whiteToMove && isAttacked(pos, pos->kSquare, false)) return botIsWhite ? 1 : -1;
            return 0;
        }

        move(pos, m);
    }
}

void calibrateBot(Bot bot, int targetElo,uint64_t baseTimeMs, uint64_t incMs) {
    Position* pos = allocateMemory();
    int victories = 0;
    int draws = 0;
    int losses = 0;
    double sumSq = 0.0; // Sum of squares for the variance

    FILE* f = fopen("test_pos_dataset_normal.txt", "r");
    if (f == NULL) {
        printf("Failed to open output.txt\n");
        freePosition(pos);
        return;
    }
    char fen[128];
    int gameCount = 0;

    printf("=== Calibrating %s against Stockfish (%d Elo) ===\n", bot.name, targetElo);

    for (int i = 0; i < 500; i++) { // Paired games with colors reversed for each starting position.
        if (fgets(fen, 128, f) == NULL) {
            break;
        }

        // First game: bot plays White.
        initBoard(pos, fen);
        clearTableSize(bot.table, TT_SIZE_THREADED);
        int resultWhite = matchAgainstStockfish(pos, bot, targetElo, true,baseTimeMs,incMs);
        if (resultWhite == 1) { victories++; sumSq += 1.0; }
        else if (resultWhite == 0) { draws++; sumSq += 0.25; }
        else losses++;
        gameCount++;

        printf("Game %d (Bot=White): %s\n", gameCount, resultWhite == 1 ? "Win" : (resultWhite == 0 ? "Draw" : "Loss"));

        // Return game: bot plays Black.
        initBoard(pos, fen);
        clearTableSize(bot.table, TT_SIZE_THREADED);
        int resultBlack = matchAgainstStockfish(pos, bot, targetElo, false,baseTimeMs,incMs);
        if (resultBlack == 1) { victories++; sumSq += 1.0; }
        else if (resultBlack == 0) { draws++; sumSq += 0.25; }
        else losses++;
        gameCount++;

        printf("Game %d (Bot=Black): %s\n", gameCount, resultBlack == 1 ? "Win" : (resultBlack == 0 ? "Draw" : "Loss"));

        printf("Current score: %d W, %d D, %d L\n\n", victories, draws, losses);
    }

    fclose(f);
    freePosition(pos);
    // Estimate Elo relative to the configured opponent.
    printf("=== Final results for %s ===\n", bot.name);
    printf("Wins : %d | Draws : %d | Losss : %d\n", victories, draws, losses);

    if (gameCount == 0) {
        printf("No games played.\n");
        return;
    }

    double score = victories + (draws / 2.0);
    double S = score / gameCount;

    printf("Score (S): %.2f%%\n", S * 100);

    // The Elo formula is undefined for scores of 0 or 1.
    if (S <= 0.0 || S >= 1.0) {
        printf("Extreme score (100%% or 0%%). Elo uncertainty unavailable.\n");
        return;
    }

    double eloDiff = 400.0 * log10(S / (1.0 - S));
    int finalElo = targetElo + (int)round(eloDiff);

    // Sample variance of game scores.

    double varianceScore = (sumSq - gameCount * S * S) / (gameCount - 1);
    double sigmaScore = sqrt(varianceScore);

    double standardErrorMean = sigmaScore / sqrt(gameCount);

    // Delta-method estimate of Elo uncertainty.

    double derivative = 400.0 / (log(10.0) * S * (1.0 - S));
    double sigmaElo = derivative * standardErrorMean;

    // Use 1.96 standard errors for the reported 95% interval.
    int marginOfError = (int)round(1.96 * sigmaElo);

    printf("----------------------------------------\n");
    printf(" ESTIMATED ELO: %d \n", finalElo);
    printf(" ERROR MARGIN: +/- %d Elo (95%%)\n", marginOfError);
    printf(" INTERVAL: [%d - %d]\n", finalElo - marginOfError, finalElo + marginOfError);
    printf("----------------------------------------\n");
}

void fight(Bot w,Bot b,uint64_t baseTimeMs,uint64_t incMs){
    Position* pos = allocateMemory();
    int victories = 0;
    int draws = 0;
    int losses = 0;

    FILE* f = fopen("test_pos_dataset_normal.txt", "r");

    if (f == NULL) {
        printf("Failed to open output.txt\n");
        freePosition(pos);
        return;
    }
    char fen[128];
    int nb = 1000;
    for(int i = 0; i < nb/2 ; i++){
        if (fgets(fen, 128, f) == NULL) {
            printf("End of file reached at game %d\n", i);
            break;
        }
        printf("\nGame %d, FEN: %s", i+1,fen);
        initBoard(pos,fen);
        clearTableSize(w.table, TT_SIZE_THREADED);
        clearTableSize(b.table, TT_SIZE_THREADED);
        inBook15 = true;
        inBook17a = true;

        int result = botAgainstBot(pos,w,b,baseTimeMs,incMs);
        if(result == 1) {
            victories++;
            printf("%s won against %s, ",w.name,b.name);
        }
        else if (result == 0) {
            draws++;
            printf("Draw, ");
        }
        else {
            losses++;
            printf("%s lost against %s, ",w.name,b.name);
        }
        double p = (victories + 0.5 * draws)/(2*i+1);
        printf("Current score for %s: %d wins, %d draws, %d losses, p = %f\n",w.name,victories,draws,losses,p);

        printf("\nReturn game %d, FEN: %s", i+1,fen);
        initBoard(pos,fen);
        inBook17a = true;
        inBook15 = true;
        clearTableSize(w.table, TT_SIZE_THREADED);
        clearTableSize(b.table, TT_SIZE_THREADED);
        result = botAgainstBot(pos,b,w,baseTimeMs,incMs);
        if(result == -1) {
            victories++;
            printf("%s won against %s, ",w.name,b.name);
        }
        else if (result == 0) {
            draws++;
            printf("Draw, ");
        }
        else {
            losses++;
            printf("%s lost against %s, ",w.name,b.name);
        }
        p = (victories + 0.5 * draws)/(2*i+2);
        printf("Current score for %s: %d wins, %d draws, %d losses, p = %f\n",w.name,victories,draws,losses,p);
    }

    printf("\n\nMatch result for the first bot: %d wins, %d draws, %d losses\n",victories,draws,losses);
    double p = (victories + 0.5 * draws)/nb;
    int diffElo = (int) -400 * log10(1/p-1);
    printf("Bot %s vs bot %s: estimated Elo difference %d\n",w.name,b.name,diffElo);
    fclose(f);
    freePosition(pos);
}

char** loadFens(int* numFens) {
    const char* filename = "test_pos_dataset_normal.txt";
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Failed to open the starting-position file (%s)\n", filename);
        exit(1);
    }

    // Reserve up to 1000 starting positions.
    int maxFens = 1000;
    char** fens = malloc(maxFens * sizeof(char*));
    int count = 0;
    char buffer[256];

    while (fgets(buffer, sizeof(buffer), file) && count < maxFens) {

        buffer[strcspn(buffer, "\r\n")] = 0;

        if (strlen(buffer) > 0) {
            fens[count] = malloc(strlen(buffer) + 1);
            strcpy(fens[count], buffer);
            count++;
        }
    }

    fclose(file);
    *numFens = count;
    printf("Loaded %d starting positions.\n", count);
    return fens;
}

void freeFens(char** fens, int numFens) {
    for (int i = 0; i < numFens; i++) {
        free(fens[i]);
    }
    free(fens);
}

void* tournamentWorker(void* arg) {
    TournamentContext* ctx = (TournamentContext*)arg;

    tt17a = calloc(TT_SIZE_THREADED, sizeof(TTEntry));
    if (!tt17a) return NULL;
    tt15 = calloc(TT_SIZE_THREADED, sizeof(TTEntry));
    if (!tt15) return NULL;

    Position* pos = allocateMemory();

    while (true) {
        int gameIndex;

        pthread_mutex_lock(&ctx->lock);
        if (ctx->gamesPlayed >= ctx->totalGames) {
            pthread_mutex_unlock(&ctx->lock);
            break;
        }
        gameIndex = ctx->gamesPlayed;
        ctx->gamesPlayed++;
        pthread_mutex_unlock(&ctx->lock);

        // Consecutive pairs share a starting FEN.
        int fenIndex = (gameIndex / 2) % ctx->numFens;
        char* startingFen = ctx->fens[fenIndex];

        // Bot A plays White in even games and Black in odd games.
        bool botAIsWhite = (gameIndex % 2 == 0);
        Bot* whiteBot = botAIsWhite ? ctx->botA : ctx->botB;
        Bot* blackBot = botAIsWhite ? ctx->botB : ctx->botA;

        initBoard(pos, startingFen);
        clearTableSize(tt17a,TT_SIZE_THREADED);
        clearTableSize(tt15,TT_SIZE_THREADED);
        inBook17a = true;
        inBook15 = true;
        int result = 0;

        while (true) {
            generateMoves(pos);

            if (pos->moves[pos->currentDepth * 256] == 0) {
                if (isInCheck(pos)) result = pos->whiteToMove ? -1 : 1;
                else result = 0;
                break;
            }

            if (pos->halfMove >= 100 || isRepetition(pos)) {
                result = 0;
                break;
            }

            uint64_t m = 0;
            if (pos->whiteToMove) {
                m = whiteBot->play(pos, ctx->baseTimeMs,ctx->incMs);
            } else {
                m = blackBot->play(pos, ctx->baseTimeMs,ctx->incMs);
            }

            // Handle a missing engine move.
            if (m == 0) {
                result = pos->whiteToMove ? -1 : 1;
                break;
            }

            move(pos, m);
        }

        // Update shared results under the mutex.
        pthread_mutex_lock(&ctx->lock);
        if (result == 0) ctx->draws++;
        else if ((result == 1 && botAIsWhite) || (result == -1 && !botAIsWhite)) ctx->winsA++;
        else ctx->winsB++;

        int completedGames = ctx->winsA + ctx->winsB + ctx->draws;
        if (completedGames >= 2) {
            double S = (ctx->winsA + 0.5 * ctx->draws) / completedGames;

            if (S > 0.0 && S < 1.0) {
                double eloDiff = 400.0 * log10(S / (1.0 - S));

                // Squared scores: 1 for a win, 0.25 for a draw.
                double sumSq = ctx->winsA + 0.25 * ctx->draws;
                double varianceScore = (sumSq - completedGames * S * S) / (completedGames - 1);
                if (varianceScore < 0.0) varianceScore = 0.0;

                double sigmaScore = sqrt(varianceScore);
                double standardErrorMean = sigmaScore / sqrt(completedGames);
                double derivative = 400.0 / (log(10.0) * S * (1.0 - S));
                double sigmaElo = derivative * standardErrorMean;
                int marginOfError = (int)round(1.96 * sigmaElo);

                printf("\r[Tournament] %d/%d | %s=%d %s=%d D=%d | Diff: %+d (+/- %d)   ",
                       completedGames, ctx->totalGames,
                       ctx->botA->name, ctx->winsA,
                       ctx->botB->name, ctx->winsB,
                       ctx->draws,
                       (int)round(eloDiff), marginOfError);
            } else {
                printf("\r[Tournament] %d/%d | %s=%d %s=%d D=%d | Diff: N/A (extreme score)   ",
                       completedGames, ctx->totalGames,
                       ctx->botA->name, ctx->winsA,
                       ctx->botB->name, ctx->winsB,
                       ctx->draws);
            }
        } else {
            printf("\r[Tournament] %d/%d | %s=%d %s=%d D=%d | Diff: pending...   ",
                   completedGames, ctx->totalGames,
                   ctx->botA->name, ctx->winsA,
                   ctx->botB->name, ctx->winsB,
                   ctx->draws);
        }

        fflush(stdout);
        pthread_mutex_unlock(&ctx->lock);
    }

    if (pos != NULL) free(pos);
    free(tt17a);
    free(tt15);
    tt17a = NULL;
    tt15 = NULL;
    return NULL;
}

void runTournamentMultiThread(Bot* botA, Bot* botB, int numGames, int numThreads, uint64_t baseTimeMs,uint64_t incMs){
    TournamentContext ctx;
    pthread_t threads[numThreads];

    ctx.fens = loadFens(&ctx.numFens);

    ctx.totalGames = numGames;
    ctx.gamesPlayed = 0;
    ctx.winsA = 0;
    ctx.winsB = 0;
    ctx.draws = 0;
    ctx.botA = botA;
    ctx.botB = botB;
    ctx.baseTimeMs = baseTimeMs;
    ctx.incMs = incMs;

    pthread_mutex_init(&ctx.lock, NULL);

    printf("Starting tournament: %d games on %d threads...\n\n", numGames, numThreads);

    for (int i = 0; i < numThreads; i++) {
        if (pthread_create(&threads[i], NULL, tournamentWorker, &ctx) != 0) {
            printf("Failed to create thread %d\n", i);
            exit(1);
        }
    }

    for (int i = 0; i < numThreads; i++) {
        pthread_join(threads[i], NULL);
    }

    pthread_mutex_destroy(&ctx.lock);

    int gameCount = ctx.winsA + ctx.draws + ctx.winsB;
    printf("\n\n=== Final results for %s ===\n", ctx.botA->name);
    printf("Wins : %d | Draws : %d | Losss : %d\n",
           ctx.winsA, ctx.draws, ctx.winsB);

    if (gameCount == 0) {
        printf("No games played.\n");
        freeFens(ctx.fens, ctx.numFens);
        return;
    }

    double score = ctx.winsA + (ctx.draws / 2.0);
    double S = score / gameCount;
    printf("Score (S): %.2f%%\n", S * 100);

    if (S <= 0.0 || S >= 1.0) {
        printf("Extreme score. Elo uncertainty unavailable.\n");
        freeFens(ctx.fens, ctx.numFens);
        return;
    }

    double eloDiff = 400.0 * log10(S / (1.0 - S));

    int sumSq = ctx.winsA + 0.25 * ctx.draws;
    double varianceScore = (sumSq - gameCount * S * S) / (gameCount - 1);
    double sigmaScore = sqrt(varianceScore);
    double standardErrorMean = sigmaScore / sqrt(gameCount);
    double derivative = 400.0 / (log(10.0) * S * (1.0 - S));
    double sigmaElo = derivative * standardErrorMean;
    int marginOfError = (int)round(1.96 * sigmaElo);

    printf("----------------------------------------\n");
    printf(" ESTIMATED ELO DIFFERENCE: %d\n", (int)round(eloDiff));
    printf(" ERROR MARGIN: +/- %d Elo (95%%)\n", marginOfError);
    printf(" INTERVAL: [%d - %d]\n",
           (int)round(eloDiff) - marginOfError, (int)round(eloDiff) + marginOfError);
    printf("----------------------------------------\n");

    freeFens(ctx.fens, ctx.numFens);
}

void* calibrationWorker(void* arg) {
    CalibrationContext* ctx = (CalibrationContext*)arg;

    tt17a = calloc(TT_SIZE_THREADED, sizeof(TTEntry));
    if (!tt17a) {
        fprintf(stderr, "Failed to allocate tt15\n");
        return NULL;
    }

    initStockfishThread();

    Position* pos = allocateMemory(); // One position allocation per thread

    while (true) {
        int gameIndex;
        pthread_mutex_lock(&ctx->lock);
        if (ctx->gamesPlayed >= ctx->totalGames) {
            pthread_mutex_unlock(&ctx->lock);
            break;
        }
        gameIndex = ctx->gamesPlayed;
        ctx->gamesPlayed++;
        pthread_mutex_unlock(&ctx->lock);

        // Game pairs: even = bot is White, odd = bot is Black
        int fenIndex = (gameIndex / 2) % ctx->numFens;
        char* startingFen = ctx->fens[fenIndex];
        bool botIsWhite = (gameIndex % 2 == 0);

        initBoard(pos, startingFen);
        clearTableSize(tt17a, TT_SIZE_THREADED);
        inBook17a = true;

        uint64_t wTime = ctx->baseTimeMs;
        uint64_t bTime = ctx->baseTimeMs;

        int result = 0;
        while (true) {
            if (pos->halfMove >= 100 || isRepetition(pos)) {
                result = 0;
                break;
            }

            // Check for mate/stalemate
            generateMoves(pos);
            if (pos->moves[pos->currentDepth * 256] == 0) {
                if (isInCheck(pos)) result = pos->whiteToMove ? -1 : 1;
                else result = 0;
                // Flip result to bot's perspective
                if (!botIsWhite) result = -result;
                break;
            }

            bool isBotTurn = pos->whiteToMove == botIsWhite;
            uint64_t* activeClock = pos->whiteToMove ? &wTime : &bTime;
            uint64_t m = 0;
            uint64_t elapsed = 0;

            if (isBotTurn) {
                uint64_t t0 = get_real_time_ms();
                m = ctx->bot.play(pos, *activeClock, ctx->incMs);
                elapsed = get_real_time_ms() - t0;
            } else {
                pthread_mutex_lock(&ctx->sfLock);
                uint64_t t0 = get_real_time_ms(); // Start timing after acquiring the Stockfish mutex.
                m = getStockfishMoveDynamic(pos, ctx->targetElo, wTime, bTime, ctx->incMs);
                elapsed = get_real_time_ms() - t0;
                pthread_mutex_unlock(&ctx->sfLock);
            }

            if (elapsed > *activeClock) {
                result = isBotTurn ? -1 : 1;
                break;
            }
            *activeClock = *activeClock - elapsed + ctx->incMs;

            if (m == 0) {
                if (pos->whiteToMove == botIsWhite) {
                    printf("BOT returned 0 at ply %d\n", pos->historyPly);
                } else {
                    printf("STOCKFISH returned 0 at ply %d\n", pos->historyPly);
              }

                if (pos->whiteToMove && isAttacked(pos, pos->KSquare, true))
                    result = botIsWhite ? -1 : 1;
                else if (!pos->whiteToMove && isAttacked(pos, pos->kSquare, false))
                    result = botIsWhite ? 1 : -1;
                else result = 0;

                break;
            }
            move(pos, m);
        }

        // Update shared counters
        pthread_mutex_lock(&ctx->lock);
        if (result == 1)       { ctx->victories++; ctx->sumSq += 1.0; }
        else if (result == 0)  { ctx->draws++;     ctx->sumSq += 0.25; }
        else                   { ctx->losses++;     }

        int completedGames = ctx->victories + ctx->draws + ctx->losses;

        if (completedGames >= 2) {
            double S = (ctx->victories + 0.5 * ctx->draws) / completedGames;

            // The Elo formula requires a score strictly between 0 and 1.
            if (S > 0.0 && S < 1.0) {
                double eloDiff = 400.0 * log10(S / (1.0 - S));
                int estElo = ctx->targetElo + (int)round(eloDiff);

                double varianceScore = (ctx->sumSq - completedGames * S * S) / (completedGames - 1);
                if (varianceScore < 0.0) varianceScore = 0.0; // Clamp floating-point roundoff.

                double sigmaScore = sqrt(varianceScore);
                double standardErrorMean = sigmaScore / sqrt(completedGames);
                double derivative = 400.0 / (log(10.0) * S * (1.0 - S));
                double sigmaElo = derivative * standardErrorMean;
                int marginOfError = (int)round(1.96 * sigmaElo);

                printf("\r[Calibration] %d/%d | W=%d D=%d L=%d | Elo: %d (+/- %d)   ",
                       completedGames, ctx->totalGames,
                       ctx->victories, ctx->draws, ctx->losses,
                       estElo, marginOfError);
            } else {
                printf("\r[Calibration] %d/%d | W=%d D=%d L=%d | Elo: N/A (extreme score)   ",
                       completedGames, ctx->totalGames,
                       ctx->victories, ctx->draws, ctx->losses);
            }
        } else {
            printf("\r[Calibration] %d/%d | W=%d D=%d L=%d | Elo: pending...   ",
                   completedGames, ctx->totalGames,
                   ctx->victories, ctx->draws, ctx->losses);
        }

        fflush(stdout);
        pthread_mutex_unlock(&ctx->lock);
    }
    if (pos != NULL) freePosition(pos);
    free(tt17a);
    tt17a = NULL;

    closeStockfishThread();
    return NULL;
}

void calibrateBotMultiThread(Bot bot, int targetElo, int numGames, int numThreads, uint64_t baseTimeMs, uint64_t incMs) {
    CalibrationContext ctx;
    pthread_t threads[numThreads];

    ctx.fens = loadFens(&ctx.numFens);
    ctx.bot = bot;
    ctx.targetElo = targetElo;
    ctx.baseTimeMs = baseTimeMs;
    ctx.incMs = incMs;
    ctx.totalGames = numGames;
    ctx.gamesPlayed = 0;
    ctx.victories = 0;
    ctx.draws = 0;
    ctx.losses = 0;
    ctx.sumSq = 0.0;
    pthread_mutex_init(&ctx.lock, NULL);
    pthread_mutex_init(&ctx.sfLock, NULL);

    printf("=== Calibrating %s vs Stockfish (%d Elo) ===\n", bot.name, targetElo);
    printf("    %d games on %d threads\n", numGames, numThreads);

    for (int i = 0; i < numThreads; i++) {
        if (pthread_create(&threads[i], NULL, calibrationWorker, &ctx) != 0) {
            printf("Failed to create thread %d\n", i);
            exit(1);
        }
    }
    for (int i = 0; i < numThreads; i++) {
        pthread_join(threads[i], NULL);
    }

    pthread_mutex_destroy(&ctx.lock);
    pthread_mutex_destroy(&ctx.sfLock);

    // --- Elo Calculation (identical to calibrateBot) ---
    int gameCount = ctx.victories + ctx.draws + ctx.losses;
    printf("\n\n=== Final results for %s ===\n", bot.name);
    printf("Wins : %d | Draws : %d | Losss : %d\n",
           ctx.victories, ctx.draws, ctx.losses);

    if (gameCount == 0) {
        printf("No games played.\n");
        freeFens(ctx.fens, ctx.numFens);
        return;
    }

    double score = ctx.victories + (ctx.draws / 2.0);
    double S = score / gameCount;
    printf("Score (S): %.2f%%\n", S * 100);

    if (S <= 0.0 || S >= 1.0) {
        printf("Extreme score. Elo uncertainty unavailable.\n");
        freeFens(ctx.fens, ctx.numFens);
        return;
    }

    double eloDiff = 400.0 * log10(S / (1.0 - S));
    int finalElo = targetElo + (int)round(eloDiff);

    double varianceScore = (ctx.sumSq - gameCount * S * S) / (gameCount - 1);
    double sigmaScore = sqrt(varianceScore);
    double standardErrorMean = sigmaScore / sqrt(gameCount);
    double derivative = 400.0 / (log(10.0) * S * (1.0 - S));
    double sigmaElo = derivative * standardErrorMean;
    int marginOfError = (int)round(1.96 * sigmaElo);

    printf("----------------------------------------\n");
    printf(" ESTIMATED ELO: %d\n", finalElo);
    printf(" ERROR MARGIN: +/- %d Elo (95%%)\n", marginOfError);
    printf(" INTERVAL: [%d - %d]\n",
           finalElo - marginOfError, finalElo + marginOfError);
    printf("----------------------------------------\n");

    freeFens(ctx.fens, ctx.numFens);
}

void calibrateRegularBullet(Bot bot, int targetElo){
    calibrateBotMultiThread(bot,targetElo,500,4, 1000,20);
}

void calibrateRegularBlitz(Bot bot, int targetElo){
    calibrateBotMultiThread(bot,targetElo,500,4,3000,50);
}

int main(){
    srand((unsigned int)time(NULL));
    initOpeningBook(openingBookPath());
    precomputed = true;

    v17at.name = "Version 17a";
    v17at.play = getBestMoveDynamic17a;
    v17at.table = calloc(TT_SIZE_THREADED, sizeof(TTEntry));

    calibrateRegularBlitz(v17at,2770);

    free(v17at.table);

    freeGlobalData();

    return 0;
}
