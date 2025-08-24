#pragma once

#include <new>

#include "SQCollectable.hpp"
#include "SQVM.hpp"

struct SQGenerator : public CHAINABLE_OBJ {
    enum SQGeneratorState {
        eRunning,
        eSuspended,
        eDead
    };

    SQObjectPtr closure;
    sqvector<SQObjectPtr> stack;
    SQVM::CallInfo _ci;
    sqvector<SQExceptionTrap> _etraps;
    SQGeneratorState _state;
private:
    SQGenerator(SQSharedState *ss, SQClosure *closure)
        : CHAINABLE_OBJ(ss)
        , closure(closure)
        , stack()
        , _etraps()
        , _state(eRunning)
    {
        _ci._generator = nullptr;
    }
public:
    static SQGenerator *Create(SQSharedState *ss, SQClosure *closure){
        SQGenerator *nc=(SQGenerator*)sq_vm_malloc(sizeof(SQGenerator));
        new (nc) SQGenerator(ss,closure);
        return nc;
    }

    void Release() {
        this->~SQGenerator();
        sq_vm_free(this, sizeof(*this));
    }

    void Kill() {
        _state = eDead;
        stack.resize(0);
        closure.Null();
    }

    bool Yield(SQVM *v,SQInteger target);

    bool Resume(SQVM *v,SQObjectPtr &dest);

#ifndef NO_GARBAGE_COLLECTOR
    void Mark(SQCollectable **chain);

    void Finalize() {
        stack.resize(0);
        closure.Null();
    }

    SQObjectType GetType() {
        return OT_GENERATOR;
    }
#endif
};
