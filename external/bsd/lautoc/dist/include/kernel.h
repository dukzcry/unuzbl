#include <sys/kmem.h>

#define malloc(size) kmem_alloc(size, KM_SLEEP)
#define free(ptr) kmem_free(ptr, sizeof(*ptr))

