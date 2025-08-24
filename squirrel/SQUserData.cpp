#include "SQUserData.hpp"

#include "SQTable.hpp"

#ifndef NO_GARBAGE_COLLECTOR
void SQUserData::Mark(SQCollectable ** chain) {
    START_MARK()
        if (_delegate) {
            _delegate->Mark(chain);
        }
    END_MARK()
}
#endif
