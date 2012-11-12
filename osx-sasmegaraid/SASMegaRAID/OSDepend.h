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