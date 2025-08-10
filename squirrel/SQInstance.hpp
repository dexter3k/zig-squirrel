#pragma once

#include "SQDelegable.hpp"
#include "sqclass.h"

struct SQInstance : public SQDelegable {
    SQClass * klass;
    SQUserPointer user_data;
    SQRELEASEHOOK _hook;
    size_t _memsize;
    SQObjectPtr _values[1];
private:
    SQInstance(SQSharedState * ss, SQClass * c, size_t memsize);
    SQInstance(SQSharedState * ss, SQInstance * c, size_t memsize);
    ~SQInstance();
public:
    static SQInstance * Create(SQSharedState * ss, SQClass * theclass) {
        // Question: why -1
        //   Answer: because SQInstance already contains one entry in _values[1]
        size_t const size = sizeof(SQInstance)
            + sq_aligning(sizeof(SQObjectPtr) * (theclass->_defaultvalues.size() > 0 ? theclass->_defaultvalues.size() - 1 : 0))
            + theclass->_udsize;
        SQInstance * newinst = (SQInstance *)sq_vm_malloc(size);
        new (newinst) SQInstance(ss, theclass, size);
        if (theclass->_udsize) {
            newinst->user_data = ((unsigned char *)newinst) + (size - theclass->_udsize);
        }
        return newinst;
    }

    SQInstance * Clone(SQSharedState *ss) {
        size_t const size = sizeof(SQInstance)
            + sq_aligning(sizeof(SQObjectPtr) * (klass->_defaultvalues.size() > 0 ? klass->_defaultvalues.size() - 1 : 0))
            + klass->_udsize;
        SQInstance * newinst = (SQInstance *)sq_vm_malloc(size);
        new (newinst) SQInstance(ss, this, size);
        if (klass->_udsize) {
            newinst->user_data = ((unsigned char *)newinst) + (size - klass->_udsize);
        }
        return newinst;
    }

    void Release() override {
        _uiRef++;
        if (_hook) {
            _hook(user_data, 0);
        }
        _uiRef--;
        if (_uiRef > 0) {
            return;
        }

        size_t const size = _memsize;
        this->~SQInstance();
        sq_vm_free(this, size);
    }

    void Finalize() override;

    bool Get(SQObjectPtr const & key, SQObjectPtr & val)  {
        if (!klass->_members->Get(key, val)) {
            return false;
        }

        if (_isfield(val)) {
            SQObjectPtr & o = _values[_member_idx(val)];
            val = _realval(o);
        } else {
            val = klass->_methods[_member_idx(val)].val;
        }

        return true;
    }

    bool Set(SQObjectPtr const & key, SQObjectPtr const & val) {
        SQObjectPtr idx;
        if (klass->_members->Get(key, idx) && _isfield(idx)) {
            _values[_member_idx(idx)] = val;
            return true;
        }
        return false;
    }
#ifndef NO_GARBAGE_COLLECTOR
    void Mark(SQCollectable ** chain) override;

    SQObjectType GetType() override {
        return OT_INSTANCE;
    }
#endif
    bool InstanceOf(SQClass *trg);
    bool GetMetaMethod(SQVM * v, SQMetaMethod mm, SQObjectPtr & res) override;
};
