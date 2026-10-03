#ifndef EVAL_H
#define EVAL_H

#include "Piece.h"
#include "Move.h"
#include "Data.h"
#include "Types.h"

/* Historical baselines retained for experiments with older search versions. */
int tapered_eval(Position* pos); /* Full material/PST recomputation, side to move. */
int ancientEvalCompute(Position *pos); /* Material only, White's perspective. */
void recomputeEvaluation(Position* pos);

void applyMoveEvaluation(Position *pos,uint64_t m);
void undoMoveEvaluation(Position *pos,uint64_t m);

/* Experimental term: currently not included by eval() or evalNew(). */
void evaluateMobility(Position* pos, int* mgScore, int* egScore);
int staticEval(Position* pos);
int evalQS(Position* pos); /* Historical blend with cached pawn terms only. */
int eval(Position* pos); /* Pawn structure and bishop pair, used by v14/v16. */
int evalNew(Position* pos); /* Adds rook activity and king safety, used by v15/v17. */
int evalNewWhite(Position* pos); /* UI score from White's perspective. */

#endif
