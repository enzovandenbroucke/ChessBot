/* Historical magic-number and table-packing experiments. Entry points are kept
 * for provenance; this tool is outside the supported build. See helpers/README.md.
 */
#include "Board.h"
#include <inttypes.h>   // For PRIu64
#include <math.h>

uint64_t* rookMagic2;
uint64_t* bishopMagic2;
uint64_t** subsetBishop2;
uint64_t** subsetRook2;
uint64_t* rookTable2;
uint64_t* bishopTable2;
uint64_t* rookOffsets2;
uint64_t* bishopOffsets2;

uint64_t* rookLength2;
uint64_t* bishopLength2;
uint64_t* subsetBishopLength2;
uint64_t* subsetRookLength2;

bool isOnBoard(int r, int f) {
    return (unsigned)r < 8 && (unsigned)f < 8;
}

uint64_t squareBitboard(int r, int f) {
    return 1ULL << (r * 8 + f);
}

uint64_t bishopMaskSlow(int sq) {
    uint64_t mask = 0;

    int r = sq / 8;
    int f = sq % 8;

    static const int dr[4] = { 1, 1, -1, -1 };
    static const int df[4] = { 1, -1, 1, -1 };

    for (int d = 0; d < 4; d++) {
        int rr = r + dr[d];
        int ff = f + df[d];

        // Exclude edge squares from the magic mask.
        while (rr > 0 && rr < 7 && ff > 0 && ff < 7) {
            mask |= squareBitboard(rr, ff);
            rr += dr[d];
            ff += df[d];
        }
    }

    return mask;
}

uint64_t rookMaskSlow(int sq) {
    uint64_t mask = 0;

    int r = sq / 8;
    int f = sq % 8;

    static const int dr[4] = { 1, -1, 0, 0 };
    static const int df[4] = { 0, 0, 1, -1 };

    for (int d = 0; d < 4; d++) {
        int rr = r + dr[d];
        int ff = f + df[d];

        while (isOnBoard(rr +dr[d], ff+df[d])) {
            mask |= squareBitboard(rr, ff);

            rr += dr[d];
            ff += df[d];
        }
    }

    return mask;
}

void readArray(const char *filename, uint64_t t[], int size) {

    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file for reading");
        assert(false);
        return;
    }

    char line[64];
    for(int i = 0; i < size; i++){
        fgets(line, sizeof(line), file);
        line[strcspn(line, "\r\n")] = '\0';
        assert(line[0] != '\0');

        char *endptr;
        t[i] = strtoull(line, &endptr, 10);
    }
    fclose(file);
}

void writeArray(const char *filename, uint64_t t[], int size){
    FILE *file = fopen(filename, "w");
    if (!file) {
        perror("Error opening file for reading");
        assert(false);
        return;
    }
    for (int i = 0; i < size; i++) {
        // Use PRIu64 to correctly format the 64-bit integer
        if (fprintf(file, "%" PRIu64 "\n", t[i]) < 0) {
            perror("Error writing to file");
            fclose(file);
            return;
        }
    }
    fclose(file);
}

void readMatrix(const char *filename, uint64_t** t, int size1, int size2) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file for reading");
        assert(false);
        return;
    }

    char line[64];
    for(int i = 0; i < size1; i++){
        for(int j = 0; j < size2; j++){
            fgets(line, sizeof(line), file);
            line[strcspn(line, "\r\n")] = '\0';
            assert(line[0] != '\0');

            char *endptr;
            t[i][j] = strtoull(line, &endptr, 10);
        }
    }
    fclose(file);
}

void writeMatrix(const char *filename, uint64_t** t, int size1, int size2){
    FILE *file = fopen(filename, "w");
    if (!file) {
        perror("Error opening file for reading");
        assert(false);
        return;
    }
    for (int i = 0; i < size1; i++) {
        for(int j=0 ; j < size2; j++){
            if (fprintf(file, "%" PRIu64 "\n", t[i][j]) < 0) {
                perror("Error writing to file");
                fclose(file);
                return;
            }
        }
    }
    fclose(file);
}

bool checkMagicBishop(uint64_t magic,int bitCount,int i){
    assert(0 <= bitCount && bitCount < 64);
    int size = 1 << bitCount;
    int* seen = malloc(size*sizeof(int));
    for(int j = 0; j<size;j++){
        seen[j] = -1;
    }
    int cap = subsetBishopLength2[i];
    int processed[cap];
    uint64_t ans[cap];
    for(int j = 0; j < cap; j++){
        processed[j] = 0;
        ans[j] = bishopAttacks(i,subsetBishop2[i][j]);
    }
    int n = 0;
    for(int k = 0 ; k < cap ; k++){
        uint64_t index = ((subsetBishop2[i][k] & bishopMasks[i]) * magic) >> (64-bitCount);
        if(seen[index] == -1 || ans[k] == ans[seen[index]]){
            processed[n] = index;
            n++;
            seen[index] = k;
        } else {
            free(seen);
            return false;
        }
    }
    free(seen);
    return true;
}

bool checkMagicRook(uint64_t magic,int bitCount,int i){
    assert(0 <= bitCount && bitCount < 64);
    int size = 1 << bitCount;
    int* seen = malloc(size*sizeof(int));
    for(int j = 0; j<size;j++){
        seen[j] = -1;
    }
    int cap = subsetRookLength2[i];
    int processed[cap];
    uint64_t ans[cap];
    for(int j = 0; j < cap; j++){
        processed[j] = 0;
        ans[j] = rookAttacks(i,subsetRook2[i][j]);
    }
    int n = 0;
    for(int k = 0 ; k < cap ; k++){
        uint64_t index = ((subsetRook2[i][k] & rookMasks[i]) * magic) >> (64-bitCount);
        if(seen[index] == -1 || ans[k] == ans[seen[index]]){
            processed[n] = index;
            n++;
            seen[index] = k;
        } else {
            free(seen);
            return false;
        }
    }
    free(seen);
    return true;
}

void checkAllMagic(){
    for(int i = 0; i < 64; i++){
        printf("i = %d\n",i);
        assert(checkMagicBishop(bishopMagic[i],bishopLengths[i],i));
        assert(checkMagicRook(rookMagic[i],rookLengths[i],i));
    }
}

void checkTables(){
    checkAllMagic();
    printf("No collisions found. Checking attack tables.\n");
    for(int i = 0; i < 64; i++){
        int x = bishopLengths[i];
        int cap = subsetBishopLength2[i];
        for(int j = 0; j < cap; j++){
            uint64_t occupied = subsetBishop2[i][j];
            int index = ((occupied & bishopMasks[i]) * bishopMagic[i]) >> (64 - x);
            index += bishopOffsets[i];
            assert(bishopTable[index] == bishopAttacks(i,occupied));
        }
        x = rookLengths[i];
        cap = subsetRookLength2[i];
        for(int j = 0; j < cap; j++){
            uint64_t occupied = subsetRook2[i][j];
            int index = ((occupied & rookMasks[i]) * rookMagic[i]) >> (64 - x);
            index += rookOffsets[i];

            if(!(rookTable[index] == rookAttacks(i,occupied))) printf("Table mismatch: square=%d, subset=%d, occupancy=%llu",i,j,occupied);
        }
    }
    printf("All attack-table checks passed.\n");
}

void checkAllMagic2(){
    for(int i = 0; i < 64; i++){
        assert(checkMagicBishop(bishopMagic2[i],bishopLength2[i],i));
        assert(checkMagicRook(rookMagic2[i],rookLength2[i],i));
    }
}

void initSubsets(){
    for(int sq = 0; sq < 64 ; sq++){
        uint64_t bishop = bishopMasks[sq];
        int bits = __builtin_popcountll(bishopMasks[sq]);
        int cap  = 1 << bits;
        subsetBishopLength2[sq] = cap;
        for(int bitIndex = 0; bitIndex < bits; bitIndex++){
            int i = __builtin_ctzll(bishop);
            bishop &= bishop - 1;
            bool b = true;
            for(int k = 0; k < cap; k++){
                if(k % (1 << bitIndex) == 0) b = !b;
                if(b) subsetBishop2[sq][k] |= 1ULL << i;
            }
        }
        uint64_t rook = rookMasks[sq];
        bits = __builtin_popcountll(rookMasks[sq]);
        cap = 1 << bits;
        subsetRookLength2[sq] = cap;
        for(int bitIndex = 0; bitIndex < bits; bitIndex++){
            int i = __builtin_ctzll(rook);
            rook &= rook - 1;
            bool b = true;
            for(int k = 0; k < cap; k++){
                if(k % (1 << bitIndex) == 0) b = !b;
                if(b) subsetRook2[sq][k] |= 1ULL << i;
            }
        }
        printf("Square %d completed !\n", sq);
    }
    writeArray("tables/bishop_subsets_length.txt",subsetBishopLength2,64);
    writeArray("tables/rook_subsets_length.txt",subsetRookLength2,64);
    writeMatrix("tables/bishop_subsets.txt",subsetBishop2,64,512);
    writeMatrix("tables/rook_subsets.txt",subsetRook2,64,4096);
}

void findMagicBishop(){

    while(true){

        int i = 0;
        int maximum = 1;
        int y = -16;
        for(int j = 0; j<64;j++){
            int x = bishopLength2[j] - __builtin_popcountll(bishopMasks[j]);
            if(y < x){
                maximum = bishopLength2[j];
                i = j;
                y = x;
            }
        }
        printf("Big loop ! i = %d, maximum = %d\n\n",i,maximum);

        for(int x = 0; x < 10000; x++){
            bool b = true;
            uint64_t candidate =  rand64() & rand64() & rand64();
            b = checkMagicBishop(candidate,maximum-1,i);
            if(!b) { continue;}
            printf("Found a new magic number for sq = %d !\n", i);
            bishopMagic2[i] = candidate;
            int len = maximum - 1;
            while(len >= 2){
                bool b = true;
                b = checkMagicBishop(candidate,len-1,i);
                if(b) len--;
                else break;
            }
            bishopLength2[i] = len;
            writeArray("tables/bishop_length.txt",bishopLength2,64);
            writeArray("tables/bishop_magic.txt",bishopMagic2,64);
            break;
        }
    }
}

void findMagicRook(){

    while(true){
        int i = rand() % 64;
        int maximum = 1;
        int y = 0;
        for(int k = 0; k<64;k++){
            int j = (i+k)%64;
            int x = (int)rookLength2[j] - __builtin_popcountll(rookMasks[j]) + 12;
            if(y < x){
                maximum = rookLength2[j];
                i = j;
                y = x;
            }
        }

        printf("\nBig loop ! i = %d, maximum = %d\n",i,maximum);

        for(int x = 0; x < 1000000; x++){
            bool b = true;
            uint64_t candidate =  rand64() & rand64() & rand64();
            b = checkMagicRook(candidate,maximum-1,i);
            if(!b) { continue;}
            printf("Found a new magic number for sq = %d !\n", i);
            rookMagic2[i] = candidate;
            int len = maximum - 1;
            while(len >= 2){
                bool b = true;
                b = checkMagicRook(candidate,len-1,i);
                if(b) len--;
                else break;
            }
            rookLength2[i] = len;
            writeArray("tables/rook_length.txt",rookLength2,64);
            writeArray("tables/rook_magic.txt",rookMagic2,64);
            break;
        }
    }
}

void createTables(){
    checkAllMagic2();
    bishopTable2 = calloc(BISHOP_MOVES_SIZE,sizeof(uint64_t));
    bishopOffsets2 = calloc(64,sizeof(uint64_t));
    int offset = 0;
    for (int i = 0; i < 64; i++) {
        int bits = bishopLength2[i];
        int cap = subsetBishopLength2[i];   // Occupancy count, before magic-index compression.
        int x = 1 << bits;
        bishopOffsets2[i] = (uint64_t) offset;
        for(int j = 0; j < cap; j++){
            uint64_t occupied = subsetBishop2[i][j];
            int index = (int)(((occupied & bishopMasks[i]) * bishopMagic2[i]) >> (64-bits));
            bishopTable2[offset + index] = bishopAttacks(i,occupied);
        }
        offset += x;   // Advance by the compressed table size.
    }
    assert(offset == BISHOP_MOVES_SIZE);
    writeArray("tables/bishop_table.txt",bishopTable2,BISHOP_MOVES_SIZE);
    writeArray("tables/bishop_offsets.txt",bishopOffsets2,64);

    rookTable2 = calloc(ROOK_MOVES_SIZE,sizeof(uint64_t));
    rookOffsets2 = calloc(64,sizeof(uint64_t));
    offset = 0;
    for (int i = 0; i < 64; i++) {
        int bits = rookLength2[i];
        int cap = subsetRookLength2[i];   // Occupancy count, before magic-index compression.
        int x = 1 << bits;
        rookOffsets2[i] = (uint64_t) offset;
        for(int j = 0; j < cap; j++){
            uint64_t occupied = subsetRook2[i][j];
            int index = (int)(((occupied & rookMasks[i]) * rookMagic2[i]) >> (64-bits));
            rookTable2[offset + index] = rookAttacks(i,occupied);
        }
        offset += x;   // Advance by the compressed table size.
    }
    assert(offset == ROOK_MOVES_SIZE);
    writeArray("tables/rook_table.txt",rookTable2,ROOK_MOVES_SIZE);
    writeArray("tables/rook_offsets.txt",rookOffsets2,64);
}

void checkTables2(){
    checkAllMagic2();
    printf("No collisions found. Checking attack tables.\n");
    for(int i = 0; i < 64; i++){
        int x = bishopLength2[i];
        int cap = subsetBishopLength2[i];
        for(int j = 0; j < cap; j++){
            uint64_t occupied = subsetBishop2[i][j];
            int index = ((occupied & bishopMasks[i]) * bishopMagic2[i]) >> (64 - x);
            index += bishopOffsets2[i];
            if(!(bishopTable2[index] == bishopAttacks(i,occupied))) printf("Table mismatch: square=%d, subset=%d, occupancy=%llu",i,j,occupied);
            assert(bishopTable2[index] == bishopAttacks(i,occupied));
        }
        x = rookLength2[i];
        cap = subsetRookLength2[i];
        for(int j = 0; j < cap; j++){
            uint64_t occupied = subsetRook2[i][j];
            int index = ((occupied & rookMasks[i]) * rookMagic2[i]) >> (64 - x);
            index += rookOffsets2[i];

            if(!(rookTable2[index] == rookAttacks(i,occupied))) printf("Table mismatch: square=%d, subset=%d, occupancy=%llu",i,j,occupied);
        }
    }
    printf("All attack-table checks passed.\n");
}

uint64_t* sharedRookTable;
uint64_t*  rookSharedOffsets;
int highestUsed = 0;

bool fits(int sq, int offset){
    int cap = subsetRookLength2[sq];   // 2^bits[sq]
    for(int k = 0; k < cap; k++){
        uint64_t occ = subsetRook2[sq][k] & rookMasks[sq];
        int idx = (int)((occ * rookMagic2[sq]) >> (64 - rookLength2[sq]));
        uint64_t want = rookAttacks(sq, subsetRook2[sq][k]);
        uint64_t at = sharedRookTable[offset + idx];
        if(at != 0 && at != want) return false;
    }
    return true;
}

void place(int sq, int offset){
    int cap = subsetRookLength2[sq];
    for(int k = 0; k < cap; k++){
        uint64_t occ = subsetRook2[sq][k] & rookMasks[sq];
        int idx = (int)((occ * rookMagic2[sq]) >> (64 - rookLength2[sq]));
        sharedRookTable[offset + idx] = rookAttacks(sq, subsetRook2[sq][k]);
        if(offset + idx > highestUsed) highestUsed = offset + idx;
    }
    rookSharedOffsets[sq] = offset;
}

// Place larger tables first; sort squareOrder by descending index width.

uint64_t* lmrTable;

void initLMR() {
    for (int depth = 0; depth < MAX_DEPTH; depth++) {
        for (int mvs = 0; mvs < 256; mvs++) {
            if (depth >= 1 && mvs >= 1) {
                // Truncate the reduction formula to an integer.

                int x = (int)(0.75 + (log(depth) * log(mvs)) / 2.25);
                lmrTable[depth + 64*mvs] = (uint64_t) x;
            } else {
                lmrTable[depth + 64*mvs] = 0;
            }
        }
    }
}

int main(){
    srand((unsigned int)time(NULL));

    subsetBishop2 = malloc(64 * sizeof(uint64_t*));
    subsetRook2 = malloc(64 * sizeof(uint64_t*));
    subsetBishopLength2 = calloc(64,sizeof(uint64_t));
    subsetRookLength2 = calloc(64,sizeof(uint64_t));
    for(int sq = 0; sq < 64; sq++){
        subsetBishop2[sq] = calloc(512, sizeof(uint64_t));
        subsetRook2[sq] = calloc(4096, sizeof(uint64_t));

    }
    initSubsets();
    readMatrix("tables/bishop_subsets.txt",subsetBishop2,64,512);
    readArray("tables/bishop_subsets_length.txt", subsetBishopLength2, 64);
    readMatrix("tables/rook_subsets.txt",subsetRook2,64,4096);
    readArray("tables/rook_subsets_length.txt", subsetRookLength2, 64);

    rookLength2 = calloc(64,sizeof(uint64_t));
    rookMagic2 = calloc(64,sizeof(uint64_t));
    readArray("tables/rook_magic.txt",rookMagic2,64);
    readArray("tables/rook_length.txt",rookLength2,64);
    bishopLength2 = calloc(64,sizeof(uint64_t));
    bishopMagic2 = calloc(64,sizeof(uint64_t));
    readArray("tables/bishop_magic.txt",bishopMagic2,64);
    readArray("tables/bishop_length.txt",bishopLength2,64);

    bishopTable2 = calloc(BISHOP_MOVES_SIZE,sizeof(uint64_t));
    bishopOffsets2 = calloc(64,sizeof(uint64_t));
    rookTable2 = calloc(ROOK_MOVES_SIZE,sizeof(uint64_t));
    rookOffsets2 = calloc(64,sizeof(uint64_t));
    readArray("tables/bishop_table.txt",bishopTable2,BISHOP_MOVES_SIZE);
    readArray("tables/bishop_offsets.txt",bishopOffsets2,64);
    readArray("tables/rook_table.txt",rookTable2,ROOK_MOVES_SIZE);
    readArray("tables/rook_offsets.txt",rookOffsets2,64);

    sharedRookTable = calloc(262144, sizeof(uint64_t));
    rookSharedOffsets = malloc(64*sizeof(uint64_t));
    int squareOrder[64];
    int lengths[64];
    for(int i = 0; i< 64; i++ ){
        squareOrder[i] = i;
        lengths[i] = rookLength2[i];
    }
    for(int i = 0; i < 63; i++){
        for(int j = i+1; j >= 1; j--){
            int x = lengths[j];
            if( x <= lengths[j-1]) break;
            lengths[j] = lengths[j-1];
            lengths[j-1] = x;
            int y = squareOrder[j];
            squareOrder[j] = squareOrder[j-1];
            squareOrder[j-1] = y;
        }
    }
    for(int s = 0; s < 64; s++){
        int sq = squareOrder[s];
        int offset = 0;
        while(!fits(sq, offset)) offset++;
        place(sq, offset);
    }
    writeArray("tables/rook_shared_table.txt",sharedRookTable,highestUsed+1);
    writeArray("tables/rook_shared_offsets.txt",rookSharedOffsets,64);

    return 0;
}

