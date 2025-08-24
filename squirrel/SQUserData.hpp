#pragma once

#include <cstdint>

#include "sqstate.h"
#include "SQDelegable.hpp"
#include "SQVM.hpp"

struct SQUserData : SQDelegable {
    size_t _size;
    SQRELEASEHOOK _hook;
    SQUserPointer _typetag;
private:
    SQUserData(SQSharedState * ss, size_t size)
        : SQDelegable(ss)
        , _size(size)
        , _hook(nullptr)
        , _typetag(nullptr)
    {}

    ~SQUserData() {
        SetDelegate(NULL);
    }
public:
    static SQUserData * Create(SQSharedState * ss, size_t size) {
        SQUserData* ud = (SQUserData*)sq_vm_malloc(sizeof(SQUserData) + size);
        new (ud) SQUserData(ss, size);
        return ud;
    }

    void Release() {
        size_t const user_data_size = this->_size;
        if (_hook) {
            _hook(SQUserPointer(this + 1), user_data_size);
        }

        this->~SQUserData();
        sq_vm_free(this, sizeof(SQUserData) + user_data_size);
    }

#ifndef NO_GARBAGE_COLLECTOR
    void Mark(SQCollectable ** chain);

    void Finalize() {
        SetDelegate(nullptr);
    }

    SQObjectType GetType() {
        return OT_USERDATA;
    }
#endif
};
