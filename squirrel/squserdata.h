/*  see copyright notice in squirrel.h */
#ifndef _SQUSERDATA_H_
#define _SQUSERDATA_H_

#include "SQDelegable.hpp"

struct SQUserData : SQDelegable {
    SQUserData(SQSharedState * ss)
        : SQDelegable()
        , _size(0)
        , _hook(nullptr)
        , _typetag(nullptr)
    {
        INIT_CHAIN();
        ADD_TO_CHAIN(&_ss(this)->_gc_chain, this);
    }

    ~SQUserData() {
        REMOVE_FROM_CHAIN(&_ss(this)->_gc_chain, this);
        SetDelegate(NULL);
    }

    static SQUserData* Create(SQSharedState *ss, SQInteger size) {
        // sizeof(SQUserData) has to be already aligned, no?
        // answer: yes, sizeof(Type) == k * alignof(Type), for some k >= 1

        SQUserData* ud = (SQUserData*)sq_vm_malloc(sizeof(SQUserData) + size);
        new (ud) SQUserData(ss);

        ud->_size = size;
        ud->_typetag = 0;
        return ud;
    }
#ifndef NO_GARBAGE_COLLECTOR
    void Mark(SQCollectable **chain);
    void Finalize(){SetDelegate(NULL);}
    SQObjectType GetType(){ return OT_USERDATA;}
#endif
    void Release() {
        if (_hook) _hook((SQUserPointer)sq_aligning(this + 1),_size);
        SQInteger tsize = _size;
        this->~SQUserData();
        sq_vm_free(this, sq_aligning(sizeof(SQUserData)) + tsize);
    }


    SQInteger _size;
    SQRELEASEHOOK _hook;
    SQUserPointer _typetag;
};

#endif //_SQUSERDATA_H_
