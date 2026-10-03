#include "Position.h"
#include "Paths.h"

bool precomputed = false;
bool flagOpening = true;

TTEntry* tTable;

uint64_t* pvTable;
int* pvLength;

PolyglotEntry* bookEntries = NULL;
long bookSize = 0; // Number of opening-book entries

uint64_t rand64() {
  uint64_t r = 0;
  for (int i=0; i<64; i += 15) {
    r = r*((uint64_t)RAND_MAX + 1) + rand();
  }
  return r;
}

void printBitboard(uint64_t bb){
    for(int i = 56; i >= 0; i++){
        if(bb & (1ULL << i)){
            printf("X ");
        } else {
            printf(". ");
        }
        if(i % 8 == 7){
            printf("\n");
            i = i - 16;
        }
    }
    printf("\n");
}

static void recomputeBitboards(Position* pos){
    for(int p = 1; p < 7; p++){
        pos->pieces[WHITE_INDEX][p] = 0;
        pos->pieces[BLACK_INDEX][p] = 0;
        for(int sq = 0; sq < 64; sq++){
            if(pos->board[sq] == (p | White)){
                pos->pieces[WHITE_INDEX][p] |= (1ULL << sq);
            }
            if(pos->board[sq] == (p | Black)){
                pos->pieces[BLACK_INDEX][p] |= (1ULL << sq);
            }
        }
    }
    updateAggregateBitboards(pos);
}

void printBoard(Position* pos){
    for(int i = 56; i >= 0; i--){
        int p = pos->board[i];
        if(p == Vacant){
            printf(". ");
        } else {
            if(isWhite(p)){
                if(isPawn(p)) printf("P ");
                else if(isKnight(p)) printf("N ");
                else if(isBishop(p)) printf("B ");
                else if(isRook(p)) printf("R ");
                else if(isQueen(p)) printf("Q ");
                else if(isKing(p)) printf("K ");
            } else {
                if(isPawn(p)) printf("p ");
                else if(isKnight(p)) printf("n ");
                else if(isBishop(p)) printf("b ");
                else if(isRook(p)) printf("r ");
                else if(isQueen(p)) printf("q ");
                else if(isKing(p)) printf("k ");
            }
        }
        if(i % 8 == 7){
            printf("\n");
            i = i - 16;
        }
    }
}

void initOpeningBook(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        printf("No opening book found (%s). Using normal search.\n", filename);
        return;
    }

    // Determine the file size
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    bookSize = fileSize / sizeof(PolyglotEntry);
    bookEntries = malloc(fileSize);

    if (bookEntries) {
        fread(bookEntries, sizeof(PolyglotEntry), bookSize, f);
        for (long i = 0; i < bookSize; i++) {
            bookEntries[i].key = __builtin_bswap64(bookEntries[i].key);
            bookEntries[i].move = __builtin_bswap16(bookEntries[i].move);
            bookEntries[i].weight = __builtin_bswap16(bookEntries[i].weight);
        }
    }
    fclose(f);
    printf("Opening book loaded: %ld entries.\n", bookSize);
}

Position* allocateMemory(){
    Position* pos = calloc(1,sizeof(Position));

    if (pos == NULL) {
        printf("Memory allocation failed\n");
        exit(1);
    }

    if(!precomputed){
        tTable = calloc(TT_SIZE,sizeof(TTEntry));
        pvLength = calloc(MAX_DEPTH+1,sizeof(int));
        pvTable = calloc(MAX_DEPTH*(MAX_DEPTH+1),sizeof(uint64_t));
        initOpeningBook(openingBookPath());
        precomputed = true;
    }
    return pos;
}

static void fenToBoard(Position* pos, char* s){

    char fenCopy[128];
    strncpy(fenCopy, s, sizeof(fenCopy) - 1);
    fenCopy[sizeof(fenCopy) - 1] = '\0';
    char** words = malloc(6 * sizeof(char*));
    assert(words != NULL);
    for (int i = 0; i < 6; i++){
        char* w = strtok(i == 0 ? fenCopy : NULL, " ");
        if (w != NULL) {
            words[i] = malloc((strlen(w) + 1) * sizeof(char));
            strcpy(words[i], w);
        } else if(i == 3) {
            words[i] = malloc(2*sizeof(char));
            strcpy(words[i],"-");
        } else {
            words[i] = malloc(2 * sizeof(char));
            strcpy(words[i], i == 4 ? "0" : "1");
        }
    }

    int square = 56;
    int i = 0;

    int len0 = (int)strlen(words[0]);
    while (i < len0){

        char c = words[0][i];
        char u = toupper(c);
        bool isBlack = c != u;
        int offset = White;
        int pieceCode = 0;
        if (u == 'R') pieceCode = Rook;
        else if (u == 'N') pieceCode = Knight;
        else if (u == 'B') pieceCode = Bishop;
        else if (u == 'Q') pieceCode = Queen;
        else if (u == 'K'){
            pieceCode = King;
            if (c == 'K') pos->KSquare = square;
            else pos->kSquare = square;
        }
        else if (u == 'P') pieceCode |= Pawn;

        if (isBlack) offset = Black;
        pieceCode |= offset;

        if (isalpha(c)) pos->board[square] = pieceCode;

        else if (c == '/') square -= 17;

        else if (isdigit(c)){
            int k = c - '0';
            for (int m = 0; m < k; m++)
            {
                pos->board[square] = (Vacant);
                square++;
            }
            square--;
        }
        square++;
        i++;
    }

    recomputeBitboards(pos);

    pos->whiteToMove = words[1][0] == 'w';

    pos->KCastle = false;
    pos->kCastle = false;
    pos->QCastle = false;
    pos->qCastle = false;
    i = 0;

    recomputeEvaluation(pos);

    if (words[2][0] == 'K'){
        i++;
        pos->KCastle = true;
    }
    if (i < (int)strlen(words[2]) && words[2][i] == 'Q'){
        i++;
        pos->QCastle = true;
    }
    if (i < (int)strlen(words[2]) && words[2][i] == 'k'){
        i++;
        pos->kCastle = true;
    }
    if (i < (int)strlen(words[2]) && words[2][i] == 'q'){
        i++;
        pos->qCastle = true;
    }

    if (words[3][0] == '-'){
        pos->enPassant = -1;
    } else {
        pos->enPassant = ((int)words[3][0] - 97) + ((int)words[3][1] - 49)*8;
        i++;
    }

    pos->halfMove = atoi(words[4]);
    pos->fullMove = atoi(words[5]);

    for (int j = 0; j < 6; j++){
        free(words[j]);
    }
    free(words);
}

void initBoard(Position* pos,char* fen){

    pos->historyPly = 0;
    pos->currentDepth = 0;
    pos->inBook = true;
    pos->nodeCount = 0;
    pos->stopSearch = false;
    pos->timeLimit = TIME_TO_THINK;
    pos->startTime = 0;

    for (int sq = 0; sq < 64; sq++){
        pos->board[sq] = Vacant;
    }

    fenToBoard(pos,fen);

    pos->currentHash = computeHash(pos);
    pos->pawnKey = computePawnHash(pos);

    for (int i = 0; i < MAX_DEPTH; i++){
        pos->moves[256 * i] = 0;
    }

}

void freePosition(Position *pos){
    if(pos != NULL) free(pos);
}

void freeGlobalData(){
    free(bookEntries);
    bookEntries = NULL;
    free(tTable);
    free(pvLength);
    free(pvTable);
}

void squareToString(char* res,int square){
    int row = square / 8;
    int col = square % 8;
    char c = (char)(row + '1');
    char d = (char)(col + 'a');
    sprintf(res, "%c%c", d, c);
}

void printMove(uint64_t m){
    char prev_sq [3];
    squareToString(prev_sq,prev(m));
    char next_sq[3];
    squareToString(next_sq,next(m));
    printf("%s to %s\n", prev_sq, next_sq);
}

void boardToFen(Position* pos,char* w){
    int position = 0;

    for (int row = 0; row < 8; row++){
        int emptySquareCount = 0;
        for (int col = 0; col < 8; col++){
            int p = pos->board[(7 - row) * 8 + col];
            if (isVacant(p)) emptySquareCount++;
            else {
                if (emptySquareCount > 0){
                    w[position++] = (char)('0' + emptySquareCount);
                    emptySquareCount = 0;
                }
                if (isWhite(p) && isRook(p))        w[position++] = 'R';
                else if (isWhite(p) && isPawn(p))   w[position++] = 'P';
                else if (isWhite(p) && isKnight(p)) w[position++] = 'N';
                else if (isWhite(p) && isBishop(p)) w[position++] = 'B';
                else if (isWhite(p) && isQueen(p))  w[position++] = 'Q';
                else if (isWhite(p) && isKing(p))   w[position++] = 'K';
                else if (isRook(p))                 w[position++] = 'r';
                else if (isPawn(p))                 w[position++] = 'p';
                else if (isKnight(p))               w[position++] = 'n';
                else if (isBishop(p))               w[position++] = 'b';
                else if (isQueen(p))                w[position++] = 'q';
                else if (isKing(p))                 w[position++] = 'k';
            }
        }

        if (emptySquareCount > 0) w[position++] = (char)('0' + emptySquareCount);
        if (row < 7) w[position++] = '/';
    }

    w[position++] = ' ';
    if (pos->whiteToMove) w[position++] = 'w';
    else w[position++] = 'b';

    w[position++] = ' ';
    if (pos->KCastle) w[position++] = 'K';
    if (pos->QCastle) w[position++] = 'Q';
    if (pos->kCastle) w[position++] = 'k';
    if (pos->qCastle) w[position++] = 'q';

    if (!(pos->KCastle || pos->QCastle || pos->kCastle || pos->qCastle)) w[position++] = '-';

    w[position++] = ' ';
    if (pos->enPassant == -1) w[position++] = '-';
    else{
        char sq[3];
        squareToString(sq,pos->enPassant);
        int len = strlen(sq);
        memcpy(&w[position], sq, len);
        position += len;
    }

    position += sprintf(&w[position], " %d %d", pos->halfMove, pos->fullMove);
    w[position] = '\0';
}

bool isRepetition(Position *pos){
    // We can stop at last irreversible move (capture/pawn push)
    int limit = pos->historyPly - pos->halfMove;
    if (limit < 0) limit = 0;

    // We navigate 2 by 2 because of the turns
    // historyPly - 4 is the first time a repetition could occur
    for (int i = pos->historyPly - 4; i >= limit; i -= 2) {
        if (pos->hashHistory[i] == pos->currentHash) {
            return true;
        }
    }
    return false;
}

