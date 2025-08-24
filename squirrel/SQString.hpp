#pragma once

#include "sqobject.h"

class SQStringTable;

struct SQString : public SQRefCounted {
    friend class SQStringTable;

    SQStringTable * owner;
    SQString * _next;

    size_t _len;
    SQHash _hash;

    SQChar _val[1];

    SQString(SQStringTable * owner)
        : SQRefCounted()
        , owner(owner)
        , _next(nullptr)
        , _len(0)
        , _hash(0)
    {}
public:
    SQInteger Next(SQObjectPtr const & refpos, SQObjectPtr & outkey, SQObjectPtr & outval);

    void Release() override;
};