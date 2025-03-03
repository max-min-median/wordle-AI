#ifndef MMM_DEBUG
#define MMM_DEBUG

#include <stdio.h>

#ifdef DEBUG
    #define DEBUG_PRINTF(...) printf(__VA_ARGS__)
#else
    #define DEBUG_PRINTF(...)
#endif  // DEBUG

#endif  // MMM_DEBUG