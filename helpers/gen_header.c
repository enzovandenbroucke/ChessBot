// Usage: gen_header <input.txt> <arrayName> <output.h> [uint64|int]
// Default: uint64_t in hexadecimal; int uses decimal values for offsets/lengths.
// Examples:
//   gen_header tables/rook_table.txt   rookTable   RookTable.h
//   gen_header tables/rook_offsets.txt rookOffsets RookOffsets.h int
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <ctype.h>
#include <stdbool.h>

int main(int argc, char** argv){
    if(argc != 4 && argc != 5){
        fprintf(stderr, "Usage: %s <input.txt> <arrayName> <output.h> [uint64|int]\n", argv[0]);
        return 1;
    }
    const char* inPath  = argv[1];
    const char* arrName = argv[2];
    const char* outPath = argv[3];
    const char* type    = (argc == 5) ? argv[4] : "uint64";

    bool isInt;
    if(strcmp(type, "uint64") == 0) isInt = false;
    else if(strcmp(type, "int") == 0) isInt = true;
    else {
        fprintf(stderr, "Unknown type '%s' (expected uint64 or int)\n", type);
        return 1;
    }

    FILE* in = fopen(inPath, "r");
    if(!in){ perror("fopen input"); return 1; }

    // Derive an uppercase include guard from the array name.
    char guard[256];
    snprintf(guard, sizeof(guard), "%s_H", arrName);
    for(char* p = guard; *p; p++) *p = toupper((unsigned char)*p);

    FILE* out = fopen(outPath, "w");
    if(!out){ perror("fopen output"); fclose(in); return 1; }

    fprintf(out, "#ifndef %s\n#define %s\n\n#include <stdint.h>\n\n", guard, guard);

    // Count input lines to determine the array length.
    long count = 0;
    char line[64];
    while(fgets(line, sizeof(line), in)) count++;
    rewind(in);

    fprintf(out, "static const %s %s[%ld] = {\n", isInt ? "int" : "uint64_t", arrName, count);

    long i = 0;
    while(fgets(line, sizeof(line), in)){
        if(isInt){
            long v = strtol(line, NULL, 10);
            fprintf(out, "%ld,", v);
        } else {
            uint64_t v = strtoull(line, NULL, 10);
            // Write bitboards as hexadecimal constants.
            fprintf(out, "0x%016" PRIX64 "ULL,", v);
        }
        i++;
        if(i % 8 == 0) fprintf(out, "\n"); else fprintf(out, " ");
    }
    if(i % 8 != 0) fprintf(out, "\n");

    fprintf(out, "};\n\n#endif // %s\n", guard);

    fclose(in);
    fclose(out);
    printf("Wrote %ld values (%s) to %s\n", count, isInt ? "int" : "uint64_t", outPath);
    return 0;
}
