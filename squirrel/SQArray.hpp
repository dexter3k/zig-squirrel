#pragma once

#include <new>

#include "SQCollectable.hpp"
#include "SQVM.hpp"

struct SQArray : public CHAINABLE_OBJ
{
private:
    SQArray(SQSharedState * ss, size_t nsize)
        : CHAINABLE_OBJ(ss)
    {
        _values.resize(nsize);
    }
public:
    static SQArray* Create(SQSharedState *ss,SQInteger nInitialSize){
        SQArray * newarray = (SQArray*)sq_vm_malloc(sizeof(SQArray));
        new (newarray) SQArray(ss, nInitialSize);
        return newarray;
    }

    void Finalize() {
        _values.resize(0);
    }

#ifndef NO_GARBAGE_COLLECTOR
    void Mark(SQCollectable ** chain);

    SQObjectType GetType() {
        return OT_ARRAY;
    }
#endif

    bool Get(SQInteger const nidx, SQObjectPtr & val) {
        if (SQUnsignedInteger(nidx) >= _values.size()) {
            return false;
        }

        SQObjectPtr & o = _values[nidx];
        val = _realval(o);
        return true;
    }

    bool Set(SQInteger const nidx, SQObjectPtr const & val) {
        if (SQUnsignedInteger(nidx) >= _values.size()) {
            return false;
        }

        _values[nidx] = val;
        return true;
    }

    SQInteger Next(SQObjectPtr const & refpos, SQObjectPtr & outkey, SQObjectPtr & outval) {
        SQUnsignedInteger idx = TranslateIndex(refpos);
        if (idx >= _values.size()) {
            return -1;
        }

        outkey = SQInteger(idx);
        SQObjectPtr & o = _values[idx];
        outval = _realval(o);

        return idx + 1;
    }

    SQArray * Clone() {
        SQArray * anew = Create(_opt_ss(this), 0);
        anew->_values.copy(_values);
        return anew;
    }

    size_t Size() const {
        return _values.size();
    }

    void Resize(SQInteger size) {
        SQObjectPtr _null;
        Resize(size,_null);
    }

    void Resize(SQInteger size,SQObjectPtr &fill) {
        _values.resize(size,fill);
        ShrinkIfNeeded();
    }

    void Reserve(SQInteger size) {
        _values.reserve(size);
    }

    void Append(const SQObject &o) {
        _values.push_back(o);
    }

    void Extend(const SQArray *a);

    SQObjectPtr &Top() {
        return _values.top();
    }

    void Pop() {
        _values.pop_back();
        ShrinkIfNeeded();
    }

    bool Insert(SQInteger idx,const SQObject &val){
        if(idx < 0 || idx > (SQInteger)_values.size()) {
            return false;
        }
        _values.insert(idx,val);
        return true;
    }

    void ShrinkIfNeeded() {
        if (_values.size() <= _values.capacity() >> 2) {
            _values.shrinktofit();
        }
    }

    bool Remove(SQInteger idx) {
        if(idx < 0 || idx >= (SQInteger)_values.size()) {
            return false;
        }
        _values.remove(idx);
        ShrinkIfNeeded();
        return true;
    }

    void Release() {
        this->~SQArray();
        sq_vm_free(this, sizeof(*this));
    }

    sqvector<SQObjectPtr> _values;
};
