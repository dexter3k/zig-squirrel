#include "sqmem.h"

#include <stdlib.h>

void * sq_vm_malloc(size_t size) {
    return malloc(size);
}

void * sq_vm_realloc(void * p, size_t oldsize, size_t size) {
    (void)oldsize;

    return realloc(p, size);
}

void sq_vm_free(void * p, size_t size) {
    (void)size;

    free(p);
}
