#include <stdint.h>

typedef struct {
    uint32_t cap;
    uint32_t size;
    void ** table;
    void * user_pointer;
} StringTable;

extern StringTable * strtab_init(void * user_pointer) {
    (void)user_pointer;

    return 0;
}
