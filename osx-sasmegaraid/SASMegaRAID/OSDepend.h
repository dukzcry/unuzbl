#define drvid   "[SASMegaRAID] "

#if defined(DEBUG)
#define DbgPrint(arg...) IOLog(drvid arg)
#define DnbgPrint(n, arg...) do { if(n) IOLog(drvid arg); } while(0)
#else
#define DbgPrint(arg...)
#define DnbgPrint(n, arg...)
#endif
#define IOPrint(arg...) IOLog(drvid arg)

#define nitems(_a)      (sizeof((_a)) / sizeof((_a)[0]))
#define ISSET(t, f)     ((t) & (f))
#define eqmin(a,b)      ((a) <= (b) ? (a) : (b))
#define MAXPHYS         (64 * 1024)

static inline unsigned short bswap_16(unsigned short x) {
    return (x>>8) | (x<<8);
}

static inline unsigned int bswap_32(unsigned int x) {
    return (bswap_16(x&0xffff)<<16) | (bswap_16(x>>16));
}
static inline unsigned long long bswap_64(unsigned long long x) {
    return (((unsigned long long)bswap_32(x&0xffffffffull))<<32) |
    (bswap_32(x>>32));
}
#define htole32(x) bswap_32 (x)
#define htole64(x) bswap_64 (x)