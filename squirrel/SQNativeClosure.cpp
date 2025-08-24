#include "SQNativeClosure.hpp"

#include "GC.hpp"
#include "sqstate.h"

#ifndef NO_GARBAGE_COLLECTOR
void SQNativeClosure::Mark(SQCollectable ** chain) {
    START_MARK()
        for(size_t i = 0; i < _noutervalues; i++) {
        	GC::MarkObject(_outervalues[i], chain);
        }
    END_MARK()
}
#endif
