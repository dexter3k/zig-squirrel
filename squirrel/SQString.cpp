#include "SQString.hpp"

#include "sqstate.h"

SQInteger SQString::Next(SQObjectPtr const & refpos, SQObjectPtr & outkey, SQObjectPtr & outval) {
    SQUnsignedInteger idx = (SQUnsignedInteger)TranslateIndex(refpos);
    if (idx >= _len) {
        return -1;
    }

    outkey = SQInteger(idx);
    outval = SQInteger(SQUnsignedInteger(_val[idx]));

    return idx + 1;
}

void SQString::Release() {
    owner->Remove(this);
}
