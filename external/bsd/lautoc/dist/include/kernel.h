#include <sys/malloc.h>

#define malloc(size) kern_malloc(size, M_WAITOK)
#define free(ptr) kern_free(ptr)
#define realloc(ptr, size) kern_realloc(ptr, size, M_WAITOK)
