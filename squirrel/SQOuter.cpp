#include "SQOuter.hpp"

#include "GC.hpp"
#include "sqobject.h"
#include "sqstate.h"

#ifndef NO_GARBAGE_COLLECTOR
void SQOuter::Mark(SQCollectable ** chain) {
    START_MARK()
    /* If the valptr points to a closed value, that value is alive */
    if (_valptr == &_value) {
        GC::MarkObject(_value, chain);
    }
    END_MARK()
}
#endif
