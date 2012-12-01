#include <IOKit/IOLib.h>

#define MAXPHYS             (64 * 1024)

#define drvid               "[SASMegaRAID] "

#define IOPrint(arg...)     IOLog(drvid arg)
#if defined(DEBUG)
#define DbgPrint(arg...)    IOLog(drvid arg)
#else
#define DbgPrint(arg...)
#endif

#define nitems(_a)          (sizeof((_a)) / sizeof((_a)[0]))

#ifdef PPC
static inline UInt32 htole32(UInt32 x) {
    return (
            (x & 0xff) << 24 |
            (x & 0xff00) << 8 |
            (x & 0xff0000) >> 8 |
            (x & 0xff000000) >> 24
    );
}
static inline UInt64 htole64(UInt64 x) {
    return (
        (x & 0xff) << 56 |
        (x & 0xff00ULL) << 40 |
        (x & 0xff0000ULL) << 24 |
        (x & 0xff000000ULL) << 8 |
        (x & 0xff00000000ULL) >> 8 |
        (x & 0xff0000000000ULL) >> 24 |
        (x & 0xff000000000000ULL) >> 40 |
        (x & 0xff00000000000000ULL) >> 56
    );
}
#else
#define htole32(x) (x)
#define htole64(x) (x)
#endif