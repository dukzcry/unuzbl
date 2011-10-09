#define UInt32	unsigned int
#define drvid   "[SASMegaRAID] "

#define NRelease(x)	do { if(x) { (x)->release(); (x) = 0; } } while(0)
#define nitems(_a)      (sizeof((_a)) / sizeof((_a)[0]))
#define ISSET(t, f)     ((t) & (f))
#define eqmin(a,b)      ((a) <= (b) ? (a) : (b))

#define MyPanic(arg...) panic(drvid arg) 
#if defined(DEBUG)
#define DbgPrint(arg...) IOLog(drvid arg)
#define DnbgPrint(n, arg...) do { if(n) IOLog(drvid arg); } while(0)
#else
#define DbgPrint(arg...)
#endif
#define IOPrint(arg...) IOLog(drvid arg)