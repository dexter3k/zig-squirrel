#include "SQString.hpp"

#include "sqstate.h"

SQString * SQString::Create(SQSharedState *ss, SQChar const * s, SQInteger len) {
    return ss->_stringtable.Add(s, len);
}

SQString * SQString::Concat(SQSharedState* ss, SQChar const * a, SQInteger alen, SQChar const * b, SQInteger blen) {
    return ss->_stringtable.Concat(a, alen, b, blen);
}

SQInteger SQString::Next(SQObjectPtr const & refpos, SQObjectPtr & outkey, SQObjectPtr & outval) {
    SQInteger idx = (SQInteger)TranslateIndex(refpos);
    while(idx < _len){
        outkey = (SQInteger)idx;
        outval = (SQInteger)((SQUnsignedInteger)_val[idx]);
        //return idx for the next iteration
        return ++idx;
    }
    //nothing to iterate anymore
    return -1;
}

void SQString::Release() {
    owner->Remove(this);
}
