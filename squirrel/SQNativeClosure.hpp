#pragma once

#include <new>

#include "SQCollectable.hpp"
#include "SQVM.hpp"

struct SQNativeClosure : public CHAINABLE_OBJ {
    SQObjectPtr _name;
    SQFUNCTION _function;
    SQObjectPtr * _outervalues;
    size_t _noutervalues;

    SQInteger _nparamscheck; // SIGNED
    sqvector<SQInteger> _typecheck;
    SQWeakRef * _env;
private:
    SQNativeClosure(SQSharedState *ss, SQFUNCTION func, SQObjectPtr * outerValues, size_t nOuters)
        : CHAINABLE_OBJ(ss)
        , _name()
        , _function(func)
        , _outervalues(outerValues)
        , _noutervalues(nOuters)
        , _env(nullptr)
    {
        for (size_t i = 0; i < _noutervalues; i++) {
            new (&_outervalues[i]) SQObjectPtr();
        }
    }

    ~SQNativeClosure() {
        if (_env) {
            _env->DecreaseRefCount();
        }
    }
public:
    static SQNativeClosure * Create(SQSharedState *ss, SQFUNCTION func, SQInteger nouters) {
        size_t const size = sizeof(SQNativeClosure) + nouters * sizeof(SQObjectPtr);
        SQNativeClosure * nc = (SQNativeClosure*)sq_vm_malloc(size);
        SQObjectPtr * outervalues = reinterpret_cast<SQObjectPtr *>(nc + 1);
        new (nc) SQNativeClosure(ss, func, outervalues, nouters);
        return nc;
    }

    SQNativeClosure * Clone() {
        SQNativeClosure * ret = SQNativeClosure::Create(_opt_ss(this),_function,_noutervalues);
        ret->_env = _env;
        if(ret->_env) {
            ret->_env->IncreaseRefCount();
        }
        ret->_name = _name;
        _COPY_VECTOR(ret->_outervalues,_outervalues,_noutervalues);
        ret->_typecheck.copy(_typecheck);
        ret->_nparamscheck = _nparamscheck;
        return ret;
    }

    void Release() {
        size_t const size = sizeof(SQNativeClosure)
            + _noutervalues * sizeof(SQObjectPtr);

        for (size_t i = 0; i < _noutervalues; i++) {
            _outervalues[i].~SQObjectPtr();
        }

        this->~SQNativeClosure();
        sq_free(this, size);
    }

#ifndef NO_GARBAGE_COLLECTOR
    void Mark(SQCollectable **chain);

    void Finalize() {
        _NULL_SQOBJECT_VECTOR(_outervalues,_noutervalues);
    }

    SQObjectType GetType() {
        return OT_NATIVECLOSURE;
    }
#endif
};
