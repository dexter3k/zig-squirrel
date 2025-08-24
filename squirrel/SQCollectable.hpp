#pragma once

#ifndef NO_GARBAGE_COLLECTOR

#include "sqobject.h"

struct SQSharedState;


struct SQCollectable : public SQRefCounted {
    // Idea:
    // we can introduce a fake SQCollectable root node
    // we then won't need to pass chain to the RemoveFromChain
    // And thus no need to store _sharedstate where it isn't needed

    SQCollectable * _next;
    SQCollectable * _prev;
    SQSharedState * _sharedstate;

    SQCollectable(SQSharedState * ss);
    virtual ~SQCollectable();

    void AddToChain(SQCollectable ** chain) {
        _prev = nullptr;
        _next = *chain;
        *chain = this;
        if (_next) {
            _next->_prev = this;
        }
    }

    void RemoveFromChain(SQCollectable ** chain) {
        if (_prev) {
            _prev->_next = _next;
        } else {
            *chain = _next;
        }

        if (_next) {
            _next->_prev = _prev;
        }

        _next = nullptr;
        _prev = nullptr;
    }

    void UnMark() {
        _uiRef &= ~MARK_FLAG;
    }

    virtual SQObjectType GetType() = 0;
    virtual void Release() = 0;
    virtual void Mark(SQCollectable ** chain) = 0;
    virtual void Finalize() = 0;
};

#endif // NO_GARBAGE_COLLECTOR
