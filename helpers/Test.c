/* Historical perft-divide driver; countMoves() is supplied by the old test.c.
 * This is not a supported standalone build. Use tests/engine_tests.c for checks.
 */
#include "Board.h"

#define test1 "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
#define test2 "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"
#define test3 "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1 "
#define test4 "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1"
#define test5 "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8"
#define test6 "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10"

void perft(Position* pos,char* fen, int depth) {
    initBoard(pos,fen);
	time_t startTime = clock();
	generateMoves(pos);
	int nodeCount = 0;
    int nb = (int)pos->moves[pos->currentDepth * 256];
	for(int i = 1; i <=  nb ;i++) {
		uint64_t m = pos->moves[i+pos->currentDepth*256];

		move(pos,m);
	    pos->currentDepth++;

		int x = countMoves(pos,depth-1);
		char a[3];
		char b[3];
		squareToString(a,prev(m));
		squareToString(b,next(m));
        printf("%s %s : %d\n", a, b, x);
		nodeCount += x;

		unmove(pos,m);
		pos->currentDepth--;
		}

		time_t endTime = clock();
		printf("Total: %d nodes in %f ms\n", nodeCount, (double) (endTime - startTime)/ CLOCKS_PER_SEC * 1000.0);
	}

void perft2(Position* pos,int depth) {
	char fen[128];
	boardToFen(pos,fen);
	perft(pos,fen,depth);
}

int main(void) {
	Position *pos = allocateMemory();
	perft(pos, test1,6);

    freePosition(pos);
	freeGlobalData();
	return 0;
}
