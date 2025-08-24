#include "SQGenerator.hpp"

#include "GC.hpp"
#include "sqstate.h"

bool SQGenerator::Yield(SQVM *v, SQInteger target) {
    if (_state == eSuspended) {
        v->Raise_Error("internal vm error, yielding dead generator");
        return false;
    }
    if (_state == eDead) {
        v->Raise_Error(_SC("internal vm error, yielding a dead generator"));
        return false;
    }

    SQInteger size = v->stack_top - v->_stackbase;
    stack.resize(size);

    SQObject _this = v->_stack[v->_stackbase];
    stack._vals[0] = ISREFCOUNTED(sq_type(_this))
        ? SQObjectPtr(_refcounted(_this)->GetWeakRef(sq_type(_this)))
        : _this;

    // TODO: fix <=, see yield codegen in the compiler
    for (SQInteger n = 1; n <= target; n++) {
        stack._vals[n] = v->_stack[v->_stackbase + n];
    }

    for (SQInteger j = 0; j < size; j++) {
        v->_stack[v->_stackbase + j].Null();
    }

    _ci = *v->ci;
    _ci._generator = NULL;
    for (SQInteger i = 0; i < _ci._etraps; i++) {
        _etraps.push_back(v->_etraps.top());
        v->_etraps.pop_back();
        // store relative stack base and size in case of resume to other stack_top
        SQExceptionTrap & et = _etraps.back();
        et._stackbase -= v->_stackbase;
        et._stacksize -= v->_stackbase;
    }

    _state = eSuspended;
    return true;
}

bool SQGenerator::Resume(SQVM *v, SQObjectPtr & dest) {
    if (_state == eDead) {
        v->Raise_Error("resuming dead generator");
        return false;
    }
    if (_state == eRunning) {
        v->Raise_Error("resuming active generator");
        return false;
    }

    size_t const size = stack.size();
    size_t const target = &dest - &(v->_stack._vals[v->_stackbase]);
    assert(target <= 255);

    size_t const newbase = v->stack_top;
    if (!v->EnterFrame(v->stack_top, v->stack_top + size, false)) {
        return false;
    }
    v->ci->_generator   = this;
    v->ci->_target      = (SQInt32)target;
    v->ci->_closure     = _ci._closure;
    v->ci->_ip          = _ci._ip;
    v->ci->_literals    = _ci._literals;
    v->ci->_ncalls      = _ci._ncalls;
    v->ci->_etraps      = _ci._etraps;
    v->ci->_root        = _ci._root;


    for (SQInteger i = 0; i < _ci._etraps; i++) {
        v->_etraps.push_back(_etraps.top());
        _etraps.pop_back();

        // restore absolute stack base and size
        SQExceptionTrap & et = v->_etraps.back();
        et._stackbase += newbase;
        et._stacksize += newbase;
    }

    SQObject _this = stack._vals[0];
    v->_stack[v->_stackbase] = sq_type(_this) == OT_WEAKREF
        ? _weakref(_this)->_obj
        : _this;

    for (size_t n = 1; n < size; n++) {
        v->_stack[v->_stackbase+n] = stack._vals[n];
        stack._vals[n].Null();
    }

    _state = eRunning;

    if (v->_debughook) {
        v->CallDebugHook('c');
    }

    return true;
}

#ifndef NO_GARBAGE_COLLECTOR
void SQGenerator::Mark(SQCollectable ** chain) {
    START_MARK()
        for (size_t i = 0; i < stack.size(); i++) {
            GC::MarkObject(stack[i], chain);
        }

        GC::MarkObject(closure, chain);
    END_MARK()
}
#endif // NO_GARBAGE_COLLECTOR
