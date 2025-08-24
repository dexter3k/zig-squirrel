#include "SQArray.hpp"

#include "GC.hpp"
#include "sqstate.h"

void SQArray::Extend(SQArray const * a) {
    size_t const xlen = a->_values.size();
    if (xlen) {
        for (size_t i = 0; i < xlen; i++) {
            Append(a->_values[i]);
        }
    }
}

#ifndef NO_GARBAGE_COLLECTOR
void SQArray::Mark(SQCollectable ** chain) {
    START_MARK()
        size_t const len = _values.size();
        for (size_t i = 0; i < len; i++) {
            GC::MarkObject(_values[i], chain);
        }
    END_MARK()
}
#endif
