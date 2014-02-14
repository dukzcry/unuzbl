#define _M_WAITOK	0x0000
void *kern_malloc(unsigned long, int);
void kern_free(void *);
void *kern_realloc(void *, unsigned long, int);

#define malloc(size) kern_malloc(size, _M_WAITOK)
#define free(ptr) kern_free(ptr)
#define realloc(ptr, size) kern_realloc(ptr, size, _M_WAITOK)
