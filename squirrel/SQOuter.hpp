#pragma once

#include <new>

#include "SQCollectable.hpp"

struct SQOuter : public CHAINABLE_OBJ {
private:
    SQOuter(SQSharedState *ss, SQObjectPtr *outer)
        : CHAINABLE_OBJ(ss)
    {
        _valptr = outer;
        _next = NULL;
    }
public:
    static SQOuter * Create(SQSharedState *ss, SQObjectPtr *outer) {
        SQOuter *nc  = (SQOuter*)sq_vm_malloc(sizeof(SQOuter));
        new (nc) SQOuter(ss, outer);
        return nc;
    }

    void Release() {
        this->~SQOuter();
        sq_vm_free(this, sizeof(SQOuter));
    }

#ifndef NO_GARBAGE_COLLECTOR
    void Mark(SQCollectable ** chain);

    void Finalize() {
        _value.Null();
    }

    SQObjectType GetType() {
        return OT_OUTER;
    }
#endif

    SQObjectPtr * _valptr;  /* pointer to value on stack, or _value below */
    SQInteger _idx;     /* idx in stack array, for relocation */
    SQObjectPtr _value;   /* value of outer after stack frame is closed */
    SQOuter * _next;    /* pointer to next outer when frame is open   */
};
