#pragma once

#include "sqobject.h"

class SQStringTable;

struct SQString : public SQRefCounted {
public:
    static SQString *Create(SQSharedState *ss, SQChar const * a, SQInteger len = -1);
    static SQString* Concat(SQSharedState* ss, SQChar const * a, SQInteger alen, SQChar const * b, SQInteger blen);

    SQInteger Next(SQObjectPtr const & refpos, SQObjectPtr & outkey, SQObjectPtr & outval);

    void Release() override;

    SQStringTable * owner;
    SQString * _next;

    SQInteger _len;
    SQHash _hash;

    SQChar _val[1];
};