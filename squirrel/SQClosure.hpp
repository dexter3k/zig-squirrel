#pragma once

#include "SQFunctionProto.hpp"
#include "SQVM.hpp"

struct SQClass;

struct SQClosure : public CHAINABLE_OBJ {
    SQWeakRef * _env;
    SQWeakRef * _root;
    SQClass * _base;
    SQFunctionProto * _function;
    SQObjectPtr * _outervalues;
    SQObjectPtr * _defaultparams;
private:
    SQClosure(SQSharedState * ss, SQFunctionProto * func, SQWeakRef * root)
        : CHAINABLE_OBJ(ss)
        , _env(nullptr)
        , _root(root)
        , _base(nullptr)
        , _function(func)
        , _outervalues((SQObjectPtr *)sq_vm_malloc(func->_noutervalues * sizeof(SQObjectPtr)))
        , _defaultparams((SQObjectPtr *)sq_vm_malloc(func->_ndefaultparams * sizeof(SQObjectPtr)))
    {
        _function->IncreaseRefCount();
        _root->IncreaseRefCount();

        for (size_t i = 0; i < func->_noutervalues; i++) {
            new (&_outervalues[i]) SQObjectPtr();
        }
        for (size_t i = 0; i < func->_ndefaultparams; i++) {
            new (&_defaultparams[i]) SQObjectPtr();
        }
    }
public:
    static SQClosure * Create(SQSharedState * ss, SQFunctionProto * func, SQWeakRef * root) {
        SQClosure * nc = (SQClosure *)sq_vm_malloc(sizeof(SQClosure));

        new (nc) SQClosure(ss, func, root);

        return nc;
    }

    static bool Load(SQVM * v, SQUserPointer up, SQREADFUNC read, SQObjectPtr & ret);

    ~SQClosure();

    void Release() {
        for (size_t i = 0; i < _function->_noutervalues; i++) {
            _outervalues[i].~SQObjectPtr();
        }
        sq_vm_free(_outervalues, _function->_noutervalues * sizeof(SQObjectPtr));

        for (size_t i = 0; i < _function->_ndefaultparams; i++) {
            _defaultparams[i].~SQObjectPtr();
        }
        sq_vm_free(_defaultparams, _function->_ndefaultparams * sizeof(SQObjectPtr));

        _function->DecreaseRefCount();

        this->~SQClosure();
        sq_vm_free(this, sizeof(SQClosure));
    }

    void SetRoot(SQWeakRef * r) {
        _root->DecreaseRefCount();
        _root = r;
        _root->IncreaseRefCount();
    }

    SQClosure * Clone() {
        SQFunctionProto * f = _function;
        SQClosure * ret = SQClosure::Create(_opt_ss(this),f,_root);
        ret->_env = _env;
        if (ret->_env) {
            ret->_env->IncreaseRefCount();
        }
        _COPY_VECTOR(ret->_outervalues,_outervalues,f->_noutervalues);
        _COPY_VECTOR(ret->_defaultparams,_defaultparams,f->_ndefaultparams);
        return ret;
    }

    bool Save(SQVM *v, SQUserPointer up, SQWRITEFUNC write);

#ifndef NO_GARBAGE_COLLECTOR
    void Mark(SQCollectable ** chain);

    void Finalize() {
        SQFunctionProto *f = _function;
        _NULL_SQOBJECT_VECTOR(_outervalues, f->_noutervalues);
        _NULL_SQOBJECT_VECTOR(_defaultparams, f->_ndefaultparams);
    }

    SQObjectType GetType() {
        return OT_CLOSURE;
    }
#endif
};
