#include "kernel.h"
#include "../../../../../sys/lib/libkern/libkern.h"

void *realloc(void *ptr, size_t size)
{
  void *newptr;
  size_t osize;

  if (!size)
    return NULL;

  newptr = malloc(size);
  if (ptr) {
    osize = sizeof(*ptr);
    memcpy(newptr, ptr, (osize < size) ? osize : size);
    free(ptr);
  }
  return newptr;
}
