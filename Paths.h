#ifndef CHESSBOT_PATHS_H
#define CHESSBOT_PATHS_H

#include <stdlib.h>

/* Relative defaults are resolved from the working directory (bitboards). */
static inline const char* openingBookPath(void) {
    const char* path = getenv("CHESSBOT_BOOK");
    return path && *path ? path : "resources/komodo.bin";
}

#endif
