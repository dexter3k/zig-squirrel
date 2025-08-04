#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct {
	size_t len;
	size_t cap;
	uint8_t * data;
} Strbuf;

extern void strbuf_init(Strbuf * buf);
extern void strbuf_deinit(Strbuf * buf);
extern void strbuf_reset(Strbuf * buf);
extern void strbuf_push_back(Strbuf * buf, uint8_t value);

#ifdef __cplusplus
} // extern "C"
#endif
