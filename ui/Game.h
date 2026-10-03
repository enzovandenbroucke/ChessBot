#ifndef CHESSBOT_UI_GAME_H
#define CHESSBOT_UI_GAME_H

#include "../Board.h"

/* Leave space in the engine's 2048-entry history for recursive search. */
#define UI_MAX_HISTORY 1024

typedef struct {
    Position* position;
    uint64_t history[UI_MAX_HISTORY];
    uint64_t legalMoves[MAX_MOVES];
    int historyCount;
    int historyCursor;
    int legalCount;
    int whiteScore;
    const char* status;
} Game;

bool loadGame(Game* game, const char* fen);
void refreshGame(Game* game);
uint64_t findGameMove(const Game* game, int from, int to, int promotionType);
bool playGameMove(Game* game, uint64_t candidate);
bool undoGameMove(Game* game);
bool redoGameMove(Game* game);

#endif
