#include "kernel.h"
#include "../../../../../sys/lib/libkern/libkern.h"

void *realloc(void *ptr, size_t size)
{
  void *newptr;

  if (!size)
    return NULL;

  newptr = malloc(size);
  if (ptr) {
    memcpy(newptr, ptr, sizeof(*ptr));
    free(ptr);
  }
  return newptr;
}
