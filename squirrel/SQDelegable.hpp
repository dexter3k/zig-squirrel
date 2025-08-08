#pragma once

#include "sqobject.h"

struct SQDelegable : public CHAINABLE_OBJ {
    SQTable * _delegate;

    SQDelegable()
        : CHAINABLE_OBJ()
        , _delegate(nullptr)
    {}

    bool SetDelegate(SQTable * m);

    virtual bool GetMetaMethod(SQVM * v, SQMetaMethod mm, SQObjectPtr & res);
};
