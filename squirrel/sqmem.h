#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <sqconfig.h>

#define sq_aligning(v) (((size_t)(v) + (SQ_ALIGNMENT-1)) & (~(SQ_ALIGNMENT-1)))

extern void * sq_vm_malloc(size_t size);
extern void * sq_vm_realloc(void * p, size_t oldsize, size_t size);
extern void sq_vm_free(void * p, size_t size);

#ifdef __cplusplus
} // extern "C"
#endif
