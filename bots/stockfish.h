#ifndef STOCKFISH_H
#define STOCKFISH_H
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "../Bot.h"

__thread HANDLE hSfStdInWr = NULL;
__thread HANDLE hSfStdOutRd = NULL;
__thread PROCESS_INFORMATION piSf;

void initStockfishThread() {
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    HANDLE hSfStdOutWr = NULL;
    HANDLE hSfStdInRd = NULL;

    CreatePipe(&hSfStdOutRd, &hSfStdOutWr, &saAttr, 0);
    SetHandleInformation(hSfStdOutRd, HANDLE_FLAG_INHERIT, 0);

    CreatePipe(&hSfStdInRd, &hSfStdInWr, &saAttr, 0);
    SetHandleInformation(hSfStdInWr, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA siStartInfo;
    ZeroMemory(&piSf, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFOA));
    siStartInfo.cb = sizeof(STARTUPINFOA);
    siStartInfo.hStdError = hSfStdOutWr;
    siStartInfo.hStdOutput = hSfStdOutWr;
    siStartInfo.hStdInput = hSfStdInRd;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    char enginePath[] = "stockfish.exe";
    if (!CreateProcessA(NULL, enginePath, NULL, NULL, TRUE, 0, NULL, NULL, &siStartInfo, &piSf)) {
        printf("Failed to launch stockfish.exe.\n");
        return;
    }

    CloseHandle(hSfStdOutWr);
    CloseHandle(hSfStdInRd);

    DWORD dwWritten;
    char initCmd[] = "uci\nisready\n";
    WriteFile(hSfStdInWr, initCmd, (DWORD)strlen(initCmd), &dwWritten, NULL);
    Sleep(50);
}

void closeStockfishThread() {
    if (hSfStdInWr) {
        DWORD dwWritten;
        WriteFile(hSfStdInWr, "quit\n", 5, &dwWritten, NULL);
        CloseHandle(hSfStdInWr);
        CloseHandle(hSfStdOutRd);
        WaitForSingleObject(piSf.hProcess, 1000);
        CloseHandle(piSf.hProcess);
        CloseHandle(piSf.hThread);
    }
}

int getStockfishEval(Position* pos) {

    if (hSfStdOutRd == NULL) {
        printf("Stockfish is not running.\n");
        return 0;
    }

    char buffer[1024];
    DWORD dwRead;
    DWORD bytesAvailable;

    // Drain pending UCI output before sending a new position.

    while (PeekNamedPipe(hSfStdOutRd, NULL, 0, NULL, &bytesAvailable, NULL) && bytesAvailable > 0) {
        ReadFile(hSfStdOutRd, buffer, sizeof(buffer) - 1, &dwRead, NULL);
    }

    char fen[128];
    boardToFen(pos, fen);

    char cmd[256];
    sprintf(cmd, "position fen %s\ngo movetime 100\n", fen);

    DWORD dwWritten;
    WriteFile(hSfStdInWr, cmd, strlen(cmd), &dwWritten, NULL);

    char totalOutput[32768] = "";
    int sfScore = 0;
    DWORD startTick = GetTickCount();

    while (true) {
        if (GetTickCount() - startTick > 1000) {
            printf("Stockfish response timeout (1 second).\n");
            break;
        }

        if (PeekNamedPipe(hSfStdOutRd, NULL, 0, NULL, &bytesAvailable, NULL) && bytesAvailable > 0) {
            if (ReadFile(hSfStdOutRd, buffer, sizeof(buffer) - 1, &dwRead, NULL) && dwRead > 0) {
                buffer[dwRead] = '\0';

                if (strlen(totalOutput) + dwRead < sizeof(totalOutput)) {
                    strcat(totalOutput, buffer);
                }

                if (strstr(totalOutput, "bestmove")) {
                    break;
                }
            }
        } else {
            Sleep(5);
        }
    }

    char* lastCp = NULL;
    char* lastMate = NULL;
    char* searchPtr = totalOutput;

    while ((searchPtr = strstr(searchPtr, "score cp ")) != NULL) {
        lastCp = searchPtr;
        searchPtr++;
    }

    searchPtr = totalOutput;
    while ((searchPtr = strstr(searchPtr, "score mate ")) != NULL) {
        lastMate = searchPtr;
        searchPtr++;
    }

    if (lastMate > lastCp) {
        int mateIn;
        sscanf(lastMate + 11, "%d", &mateIn);
        sfScore = (mateIn > 0) ? 30000 - mateIn : -30000 - mateIn;
    } else if (lastCp != NULL) {
        sscanf(lastCp + 9, "%d", &sfScore);
    } else {

        printf("No score found in the Stockfish response:\n%s\n", totalOutput);
    }

    // Return the score from White's perspective.
    return pos->whiteToMove ? sfScore : -sfScore;
}

uint64_t getStockfishMove(Position* pos, int targetElo) {
    if (hSfStdOutRd == NULL) return 0;

    // Drain pending UCI output.
    char buffer[1024];
    DWORD dwRead, bytesAvailable;
    while (PeekNamedPipe(hSfStdOutRd, NULL, 0, NULL, &bytesAvailable, NULL) && bytesAvailable > 0) {
        ReadFile(hSfStdOutRd, buffer, sizeof(buffer) - 1, &dwRead, NULL);
    }

    char fen[128];
    boardToFen(pos, fen);

    // Set UCI strength and time limits.
    char cmd[512];
    sprintf(cmd, "setoption name UCI_LimitStrength value true\n"
                 "setoption name UCI_Elo value %d\n"
                 "position fen %s\n"
                 "go movetime 100\n", targetElo, fen);

    DWORD dwWritten;
    WriteFile(hSfStdInWr, cmd, strlen(cmd), &dwWritten, NULL);

    char totalOutput[32768] = "";
    DWORD startTick = GetTickCount();

    while (true) {
        if (GetTickCount() - startTick > 2000) break; // Response timeout.

        if (PeekNamedPipe(hSfStdOutRd, NULL, 0, NULL, &bytesAvailable, NULL) && bytesAvailable > 0) {
            if (ReadFile(hSfStdOutRd, buffer, sizeof(buffer) - 1, &dwRead, NULL) && dwRead > 0) {
                buffer[dwRead] = '\0';
                if (strlen(totalOutput) + dwRead < sizeof(totalOutput)) {
                    strcat(totalOutput, buffer);
                }
                if (strstr(totalOutput, "bestmove")) break;
            }
        } else {
            Sleep(5);
        }
    }

    // Parse the UCI bestmove, including promotions.
    char* bestmovePtr = strstr(totalOutput, "bestmove ");
    if (!bestmovePtr) return 0;

    char moveStr[8];
    sscanf(bestmovePtr + 9, "%7s", moveStr);

    if (strcmp(moveStr, "(none)") == 0) return 0; // Checkmate or stalemate.

    // Convert algebraic squares to 0-63 indices.
    int pFrom = (moveStr[0] - 'a') + (moveStr[1] - '1') * 8;
    int pTo   = (moveStr[2] - 'a') + (moveStr[3] - '1') * 8;

    int myPromoPiece = Vacant;
    if (strlen(moveStr) >= 5) {
        char p = moveStr[4];
        if (p == 'q') myPromoPiece = Queen;
        else if (p == 'r') myPromoPiece = Rook;
        else if (p == 'b') myPromoPiece = Bishop;
        else if (p == 'n') myPromoPiece = Knight;
    }

    // Match the engine-generated move
    generateMoves(pos);
    int totalMoves = pos->moves[256 * pos->currentDepth];
    for (int i = 1; i <= totalMoves; i++) {
        uint64_t m = pos->moves[i + pos->currentDepth * 256];
        if (prev(m) == pFrom && next(m) == pTo) {
            if (myPromoPiece == Vacant || (promotion(m) & PieceCache) == myPromoPiece) {
                return m;
            }
        }
    }
    return 0;
}

uint64_t getStockfishMoveDynamic(Position* pos, int targetElo,uint64_t wTime,uint64_t bTime,uint64_t incMs) {
    if (hSfStdOutRd == NULL) return 0;

    // Drain pending UCI output.
    char buffer[1024];
    DWORD dwRead, bytesAvailable;
    while (PeekNamedPipe(hSfStdOutRd, NULL, 0, NULL, &bytesAvailable, NULL) && bytesAvailable > 0) {
        ReadFile(hSfStdOutRd, buffer, sizeof(buffer) - 1, &dwRead, NULL);
    }

    char fen[128];
    boardToFen(pos, fen);

    // Set UCI strength and time limits.
    char cmd[512];
    sprintf(cmd, "setoption name UCI_LimitStrength value true\n"
                 "setoption name UCI_Elo value %d\n"
                 "position fen %s\n"
                 "go wtime %llu btime %llu winc %llu binc %llu\n", targetElo, fen,wTime, bTime, incMs, incMs);

    DWORD dwWritten;
    WriteFile(hSfStdInWr, cmd, strlen(cmd), &dwWritten, NULL);

    char totalOutput[32768] = "";
    DWORD startTick = GetTickCount();

    while (true) {
        uint64_t currentClock = pos->whiteToMove ? wTime : bTime;
        if (GetTickCount() - startTick > currentClock + 1500) break;

        if (PeekNamedPipe(hSfStdOutRd, NULL, 0, NULL, &bytesAvailable, NULL) && bytesAvailable > 0) {
            if (ReadFile(hSfStdOutRd, buffer, sizeof(buffer) - 1, &dwRead, NULL) && dwRead > 0) {
                buffer[dwRead] = '\0';
                if (strlen(totalOutput) + dwRead < sizeof(totalOutput)) {
                    strcat(totalOutput, buffer);
                }
                if (strstr(totalOutput, "bestmove")) break;
            }
        } else {
            Sleep(5);
        }
    }

    // Parse the UCI bestmove, including promotions.
    char* bestmovePtr = strstr(totalOutput, "bestmove ");
    if (!bestmovePtr) return 0;

    char moveStr[8];
    sscanf(bestmovePtr + 9, "%7s", moveStr);

    if (strcmp(moveStr, "(none)") == 0) return 0; // Checkmate or stalemate.

    // Convert algebraic squares to 0-63 indices.
    int pFrom = (moveStr[0] - 'a') + (moveStr[1] - '1') * 8;
    int pTo   = (moveStr[2] - 'a') + (moveStr[3] - '1') * 8;

    int myPromoPiece = Vacant;
    if (strlen(moveStr) >= 5) {
        char p = moveStr[4];
        if (p == 'q') myPromoPiece = Queen;
        else if (p == 'r') myPromoPiece = Rook;
        else if (p == 'b') myPromoPiece = Bishop;
        else if (p == 'n') myPromoPiece = Knight;
    }

    // Match the engine-generated move
    generateMoves(pos);
    int totalMoves = pos->moves[256 * pos->currentDepth];
    for (int i = 1; i <= totalMoves; i++) {
        uint64_t m = pos->moves[i + pos->currentDepth * 256];
        if (prev(m) == pFrom && next(m) == pTo) {
            if (myPromoPiece == Vacant || (promotion(m) & PieceCache) == myPromoPiece) {
                return m;
            }
        }
    }
    return 0;
}

#endif
