#include "Board.h"
#include <inttypes.h>

static int failures, checks;
static const char *context;
#define CHECK(expr) do { checks++; if (!(expr)) { failures++; \
    if (failures <= 30) fprintf(stderr, "FAIL [%s] line %d: %s\n", context, __LINE__, #expr); \
} } while (0)

typedef struct { const char *name, *fen; uint64_t nodes[5]; } Fixture;
/* Reference: https://chessprogramming.org/Perft_Results */
static const Fixture fixtures[] = {
    {"start", START_FEN, {1,20,400,8902,197281}},
    {"kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", {1,48,2039,97862,4085603}},
    {"rook endgame", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", {1,14,191,2812,43238}},
    {"promotions", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", {1,6,264,9467,422333}},
    {"tactical", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", {1,44,1486,62379,2103487}},
    {"middlegame", "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", {1,46,2079,89890,3894594}}
};

static void load(Position *p, const char *fen) { initBoard(p, (char*)fen); }

static void consistent(Position *p) {
    uint64_t pieces[2][7] = {{0}}, colors[2] = {0};
    for (int sq=0; sq<64; sq++) if (!isVacant(p->board[sq])) {
        int c=colorIndex(p->board[sq]), t=uncolor(p->board[sq]);
        pieces[c][t] |= 1ULL << sq;
        colors[c] |= 1ULL << sq;
    }
    CHECK(memcmp(pieces,p->pieces,sizeof pieces)==0);
    CHECK(memcmp(colors,p->byColor,sizeof colors)==0);
    CHECK(p->occupied==(colors[0]|colors[1]));
    CHECK(p->currentHash==computeHash(p));
    CHECK(p->pawnKey==computePawnHash(p));
    int mg=p->mgEval, eg=p->egEval, phase=p->phase;
    recomputeEvaluation(p);
    CHECK(mg==p->mgEval && eg==p->egEval && phase==p->phase);
    p->mgEval=mg; p->egEval=eg; p->phase=phase;
}

static void restored(Position *p, Position *before) {
    char a[128], b[128]; boardToFen(p,a); boardToFen(before,b);
    CHECK(strcmp(a,b)==0);
    CHECK(memcmp(p->board,before->board,sizeof p->board)==0);
    CHECK(memcmp(p->pieces,before->pieces,sizeof p->pieces)==0);
    CHECK(memcmp(p->byColor,before->byColor,sizeof p->byColor)==0);
    CHECK(p->occupied==before->occupied);
    CHECK(p->KSquare==before->KSquare && p->kSquare==before->kSquare);
    CHECK(p->currentHash==before->currentHash && p->pawnKey==before->pawnKey);
    CHECK(p->historyPly==before->historyPly && p->currentDepth==before->currentDepth);
    CHECK(p->mgEval==before->mgEval && p->egEval==before->egEval && p->phase==before->phase);
    /* Scratch move buffers and history beyond historyPly are intentionally excluded. */
}

static uint64_t perft(Position *p, int depth, bool late) {
    if (!depth) return 1;
    if (late) generatePLMoves(p); else generateMoves(p);
    int start=p->currentDepth*256, count=(int)p->moves[start];
    uint64_t total=0;
    for (int i=1;i<=count;i++) {
        uint64_t m=p->moves[start+i];
        move(p,m); p->currentDepth++;
        if (!late || !canCaptureKing(p)) total+=perft(p,depth-1,late);
        unmove(p,m); p->currentDepth--;
    }
    return total;
}

static void encodings(void) {
    context="move encoding";
    for (int from=0;from<64;from++) for (int to=0;to<64;to++)
        for (int prom=Knight;prom<=Queen;prom++) {
            int state=15 | (43<<4) | (99<<11);
            uint64_t m=make(from,to,prom|White,Rook|Black,state,1,100ULL<<41);
            CHECK(prev(m)==from && next(m)==to);
            CHECK(promotion(m)==(prom|White) && captured(m)==(Rook|Black));
            CHECK(castlingRights(m)==15 && enPassantValue(m)==43 && hCount(m)==99);
            CHECK(isEnPassant(m));
            CHECK(toSmallMove(m)==(uint16_t)(from | (to<<6) | (prom<<12)));
        }
    context="TT mate score";
    CHECK(scoreFromTT(scoreToTT(INF-10,4),7)==INF-13);
    CHECK(scoreFromTT(scoreToTT(-INF+10,4),7)==-INF+13);
    CHECK(scoreFromTT(scoreToTT(125,4),7)==125);
}

static void special(Position *p, Position *saved, const char *name, const char *fen,
                    int from, int to, int promotionType, bool expected) {
    context=name; load(p,fen); *saved=*p;
    generateMoves(p);
    uint64_t found=0;
    for (int i=1;i<=(int)p->moves[0];i++) {
        uint64_t m=p->moves[i];
        if (prev(m)==from && next(m)==to && uncolor(promotion(m))==promotionType) found=m;
    }
    CHECK((found!=0)==expected);
    if (found) {
        move(p,found); consistent(p);
        CHECK(p->board[to]==(promotionType ? (promotionType | (saved->whiteToMove?White:Black)) : saved->board[from]));
        if (isEnPassant(found)) CHECK(isVacant(p->board[to+(saved->whiteToMove?-8:8)]));
        unmove(p,found); restored(p,saved);
    }
}

int main(int argc, char **argv) {
    bool extended=argc==2 && strcmp(argv[1],"--extended")==0;
    if (argc>1 && !extended) { fprintf(stderr,"Usage: engine-tests [--extended]\n"); return 2; }
    Position *p=calloc(1,sizeof *p), *saved=calloc(1,sizeof *saved);
    if (!p || !saved) { free(p); free(saved); return 2; }
    encodings();
    for (size_t f=0;f<sizeof fixtures/sizeof fixtures[0];f++) {
        context=fixtures[f].name; load(p,fixtures[f].fen); *saved=*p;
        char fen[128]; boardToFen(p,fen); CHECK(strcmp(fen,fixtures[f].fen)==0);
        consistent(p); generateMoves(p); restored(p,saved);
        int count=(int)p->moves[0];
        for (int i=1;i<=count;i++) {
            uint64_t m=p->moves[i]; move(p,m); consistent(p);
            unmove(p,m); restored(p,saved);
        }
        for (int depth=0;depth<=(extended?4:3);depth++) for (int late=0;late<2;late++) {
            load(p,fixtures[f].fen); uint64_t n=perft(p,depth,late!=0);
            if(n!=fixtures[f].nodes[depth]) fprintf(stderr,"%s depth %d late=%d expected=%" PRIu64 " actual=%" PRIu64 "\n",context,depth,late,fixtures[f].nodes[depth],n);
            CHECK(n==fixtures[f].nodes[depth]); restored(p,saved);
        }
        printf("Checked %s\n",context);
    }
    special(p,saved,"white castle","r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",4,6,0,true);
    special(p,saved,"black castle","r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1",60,58,0,true);
    special(p,saved,"castle through check","k4r2/8/8/8/8/8/8/4K2R w K - 0 1",4,6,0,false);
    special(p,saved,"white en passant","4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 2",36,43,0,true);
    special(p,saved,"black en passant","4k3/8/8/8/3Pp3/8/8/4K3 b - d3 0 2",28,19,0,true);
    special(p,saved,"pinned en passant","k3r3/8/8/3pP3/8/8/8/4K3 w - d6 0 2",36,43,0,false);
    for (int t=Knight;t<=Queen;t++) {
        special(p,saved,"white promotion","4k3/P7/8/8/8/8/8/4K3 w - - 0 1",48,56,t,true);
        special(p,saved,"black capture promotion","4k3/8/8/8/8/8/p7/1R2K3 b - - 0 1",8,1,t,true);
    }
    context="checkmate";
    load(p,"7k/6Q1/5K2/8/8/8/8/8 b - - 0 1");
    generateMoves(p); CHECK(p->moves[0]==0 && isInCheck(p));
    context="stalemate";
    load(p,"7k/5Q2/5K2/8/8/8/8/8 b - - 0 1");
    generateMoves(p); CHECK(p->moves[0]==0 && !isInCheck(p));
    context="deterministic game rollback";
    load(p,START_FEN); *saved=*p;
    uint64_t played[96]; int length=0;
    uint32_t seed=12345;
    for (;length<96;length++) {
        generateMoves(p); int n=(int)p->moves[0];
        if (!n) break;
        seed=1664525u*seed+1013904223u;
        played[length]=p->moves[1+seed%(uint32_t)n];
        move(p,played[length]); consistent(p);
    }
    while (length) { unmove(p,played[--length]); consistent(p); }
    restored(p,saved);
    free(p); free(saved);
    printf("%d checks, %d failures (%s)\n",checks,failures,extended?"extended":"quick");
    return failures ? 1 : 0;
}
