#pragma once

#include "SQCollectable.hpp"
#include "sqobject.h"

struct SQDelegable : public CHAINABLE_OBJ {
    SQTable * _delegate;

    SQDelegable(SQSharedState * ss)
        : CHAINABLE_OBJ(ss)
        , _delegate(nullptr)
    {}

    bool SetDelegate(SQTable * m);

    virtual bool GetMetaMethod(SQVM * v, SQMetaMethod mm, SQObjectPtr & res);
};
