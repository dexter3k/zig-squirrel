#include "SQVM.hpp"

#include <iostream>
#include <math.h>
#include <stdlib.h>

#include "sqopcodes.h"

#include "SQArray.hpp"
#include "SQClass.hpp"
#include "SQClosure.hpp"
#include "SQFunctionProto.hpp"
#include "SQGenerator.hpp"
#include "SQInstance.hpp"
#include "SQString.hpp"
#include "SQTable.hpp"
#include "SQUserData.hpp"
#include "SQOuter.hpp"
#include "SQNativeClosure.hpp"

#define MAX_NATIVE_CALLS 100

// sqbaselib.cpp
void sq_base_register(HSQUIRRELVM v);

struct AutoDec {
    AutoDec(size_t * n) {
        _n = n;
    }

    ~AutoDec() {
        (*_n)--;
    }

    size_t * _n;
};

#define TARGET _stack._vals[_stackbase+arg0]
#define STK(a) _stack._vals[_stackbase+(a)]
// static inline SQObjectPtr & 

static void CreateClassInstance(SQClass * klass, SQObjectPtr & instance, SQObjectPtr & constructor) {
    instance = klass->CreateInstance();
    if(!klass->GetConstructor(constructor)) {
        constructor.Null();
    }
}

bool SQVM::BW_OP(
    uint8_t op,
    SQObjectPtr & trg,
    SQObjectPtr const & o1,
    SQObjectPtr const & o2
) {
    if ((sq_type(o1) | sq_type(o2)) != OT_INTEGER) {
        Raise_Error("bitwise op between '%s' and '%s'", GetTypeName(o1), GetTypeName(o2));
        return false;
    }

    SQInteger i1 = _integer(o1);
    SQInteger i2 = _integer(o2);
    switch (op) {
    case BW_AND:     trg = i1 & i2; break;
    case BW_OR:      trg = i1 | i2; break;
    case BW_XOR:     trg = i1 ^ i2; break;
    case BW_SHIFTL:  trg = i1 << i2; break;
    case BW_SHIFTR:  trg = i1 >> i2; break;
    case BW_USHIFTR: trg = SQInteger(SQUnsignedInteger(i1) >> i2); break;
    default:
        Raise_Error("internal vm error bitwise op failed");
        return false;
    }

    return true;
}

bool SQVM::_ARITH_(
    uint8_t op,
    SQObjectPtr & out,
    SQObjectPtr const & o1,
    SQObjectPtr const & o2
) {
    uint32_t type_mask = sq_type(o1) | sq_type(o2);
    switch (type_mask) {
    case OT_INTEGER:
        switch (op) {
        case '+':
            out = SQInteger(SQUnsignedInteger(_integer(o1)) + SQUnsignedInteger(_integer(o2)));
            return true;
        case '-':
            out = SQInteger(SQUnsignedInteger(_integer(o1)) - SQUnsignedInteger(_integer(o2)));
            return true;
        case '*':
            out = SQInteger(SQUnsignedInteger(_integer(o1)) * SQUnsignedInteger(_integer(o2)));
            return true;
        case '/':{
            // TODO: overflow?
            SQInteger i2 = _integer(o2);
            if (i2 == 0) {
                Raise_Error("division by zero");
                return false;
            }
            out = _integer(o1) / i2;
            return true;
        }
        default:
            assert(0);
        }
    case OT_FLOAT | OT_INTEGER:
    case OT_FLOAT:
        switch (op) {
        case '+':
            out = tofloat(o1) + tofloat(o2);
            return true;
        case '-':
            out = tofloat(o1) - tofloat(o2);
            return true;
        case '*':
            out = tofloat(o1) * tofloat(o2);
            return true;
        case '/':
            out = tofloat(o1) / tofloat(o2);
            return true;
        default:
            assert(0);
        }
    default:
        return this->ARITH_OP(op, out, o1, o2);
    }
}

bool SQVM::ARITH_OP(
    uint8_t op,
    SQObjectPtr & trg,
    SQObjectPtr const & o1,
    SQObjectPtr const & o2
) {
    uint32_t tmask = sq_type(o1) | sq_type(o2);
    switch (tmask) {
    case OT_INTEGER:{
        SQInteger res;
        SQInteger i1 = _integer(o1);
        SQInteger i2 = _integer(o2);
        switch (op) {
        case '+': res = i1 + i2; break;
        case '-': res = i1 - i2; break;
        case '/': if (i2 == 0) { Raise_Error(_SC("division by zero")); return false; }
                else if (i2 == -1 && i1 == INT_MIN) { Raise_Error(_SC("integer overflow")); return false; }
                res = i1 / i2;
                break;
        case '*': res = i1 * i2; break;
        case '%': if (i2 == 0) { Raise_Error(_SC("modulo by zero")); return false; }
                else if (i2 == -1 && i1 == INT_MIN) { res = 0; break; }
                res = i1 % i2;
                break;
        default:
            assert(0);
        }
        trg = res;
        break;
    }
    case OT_FLOAT | OT_INTEGER:
    case OT_FLOAT:{
        SQFloat f1 = tofloat(o1);
        SQFloat f2 = tofloat(o2);
        switch (op) {
        case '+':
            trg = SQFloat(f1 + f2);
            return true;
        case '-':
            trg = SQFloat(f1 - f2);
            return true;
        case '/':
            trg = SQFloat(f1 / f2);
            return true;
        case '*':
            trg = SQFloat(f1 * f2);
            return true;
        case '%':
            trg = SQFloat(fmod(double(f1), double(f2)));
            return true;
        default:
            assert(0);
        }
    }
    default:
        if (op == '+' && (tmask & _RT_STRING)){
            if (!StringCat(o1, o2, trg)) {
                return false;
            }
        } else if (!ArithMetaMethod(op, o1, o2, trg)) {
            return false;
        }
    }
    return true;
}

SQVM::SQVM(SQSharedState * ss, SQVM * friend_vm, size_t stack_size)
    : CHAINABLE_OBJ(ss)
    , _sharedstate(ss)
    , release_hook(nullptr)
    , release_hook_user_pointer(nullptr)

    , _stack()

    , call_stack_size(0)
    , call_stack()

    , n_metamethod_calls(0)

    , is_suspended(false)

    , n_native_calls(0)

    , temp_reg()
{
    // TODO: no stack size checks?
    // initial stack must fit base lib at the very least
    _stack.resize(stack_size);
    call_stack.resize(4);

    _suspended_target = -1;
    _suspended_root = false;
    _suspended_traps = -1;

    _lasterror.Null();
    _errorhandler.Null();
    _debughook = false;
    _debughook_native = NULL;
    _debughook_closure.Null();
    _openouters = NULL;
    ci = NULL;

    _stackbase = 0;
    stack_top = 0;

    if (!friend_vm) {
        _roottable = SQTable::Create(ss, 0);
        sq_base_register(this);
    } else {
        _roottable = friend_vm->_roottable;
        _errorhandler = friend_vm->_errorhandler;
        _debughook = friend_vm->_debughook;
        _debughook_native = friend_vm->_debughook_native;
        _debughook_closure = friend_vm->_debughook_closure;
    }
}

SQVM::~SQVM() {
    Finalize();
}

void SQVM::Finalize() {
    if (release_hook) {
        release_hook(release_hook_user_pointer, 0);
        release_hook = nullptr;
    }

    if (_openouters) {
        CloseOuters(&_stack._vals[0]);
    }

    _roottable.Null();
    _lasterror.Null();
    _errorhandler.Null();
    _debughook = false;
    _debughook_native = NULL;
    _debughook_closure.Null();
    temp_reg.Null();
    call_stack.resize(0);

    size_t const stack_size = _stack.size();
    for (size_t i = 0; i < stack_size; i++) {
        _stack[i].Null();
    }
}

void SQVM::GrowCallStack() {
    call_stack.resize(call_stack.size() * 2);
}

bool SQVM::ArithMetaMethod(uint8_t op,const SQObjectPtr &o1,const SQObjectPtr &o2,SQObjectPtr &dest) {
    SQMetaMethod mm;
    switch (op) {
        case _SC('+'): mm=MT_ADD; break;
        case _SC('-'): mm=MT_SUB; break;
        case _SC('/'): mm=MT_DIV; break;
        case _SC('*'): mm=MT_MUL; break;
        case _SC('%'): mm=MT_MODULO; break;
        default:
            mm = MT_ADD;
            assert(0);
            break; //shutup compiler
    }

    SQDelegable * delegable = nullptr;
    switch (sq_type(o1)) {
    case OT_TABLE:
        delegable = _table(o1);
        break;
    case OT_USERDATA:
        delegable = _userdata(o1);
        break;
    case OT_INSTANCE:
        delegable = _instance(o1);
        break;
    default:
        break;
    }

    SQObjectPtr closure;
    if (delegable && delegable->_delegate && delegable->GetMetaMethod(this, mm, closure)) {
        Push(o1);
        Push(o2);
        return CallMetaMethod(closure, 2, dest);
    }

    Raise_Error(_SC("arith op %c on between '%s' and '%s'"), op, GetTypeName(o1), GetTypeName(o2));

    return false;
}

bool SQVM::NEG_OP(SQObjectPtr & trg, SQObjectPtr const & o) {
    SQDelegable * delegable = nullptr;
    switch(sq_type(o)) {
    case OT_INTEGER:
        // Prevent undefined behaviour when o == INT_MIN
        trg = SQInteger(-SQUnsignedInteger(_integer(o)));
        return true;
    case OT_FLOAT:
        trg = -_float(o);
        return true;
    case OT_TABLE:
        delegable = _table(o);
        break;
    case OT_USERDATA:
        delegable = _userdata(o);
        break;
    case OT_INSTANCE:
        delegable = _instance(o);
        break;
    default:
        break; //shutup compiler
    }

    SQObjectPtr closure;
    if (delegable && delegable->_delegate && delegable->GetMetaMethod(this, MT_UNM, closure)) {
        Push(o);
        if(!CallMetaMethod(closure, 1, temp_reg)) return false;
        _Swap(trg,temp_reg);
        return true;
    }

    Raise_Error(_SC("attempt to negate a %s"), GetTypeName(o));

    return false;
}

bool SQVM::ObjCmp(const SQObjectPtr &o1, const SQObjectPtr &o2, SQInteger &result) {
    SQObjectType t1 = sq_type(o1);
    SQObjectType t2 = sq_type(o2);
    if(t1 == t2) {
        if(_rawval(o1) == _rawval(o2)) {
            result = 0;
            return true;
        }

        SQDelegable * delegable = nullptr;
        switch(t1){
        case OT_STRING:
            result = strcmp(_stringval(o1), _stringval(o2));
            return true;
        case OT_INTEGER:
            result = (_integer(o1)<_integer(o2)) ? -1 : 1;
            return true;
        case OT_FLOAT:
            result = (_float(o1)<_float(o2)) ? -1 : 1;
            return true;
        case OT_TABLE:
            delegable = _table(o1);
            break;
        case OT_USERDATA:
            delegable = _userdata(o1);
            break;
        case OT_INSTANCE:
            delegable = _instance(o1);
            break;
        default:
            break;
        }

        SQObjectPtr closure;
        if (delegable && delegable->_delegate && delegable->GetMetaMethod(this, MT_CMP, closure)) {
            Push(o1);
            Push(o2);

            SQObjectPtr res;
            if (!CallMetaMethod(closure, 2, res)) {
                return false;
            } else if (sq_type(res) != OT_INTEGER) {
                Raise_Error(_SC("_cmp must return an integer"));
                return false;
            }

            result = _integer(res);
            return true;
        }
        
        result = _userpointer(o1) < _userpointer(o2) ? -1 : 1;
        return true;
    }

    if (sq_isnumeric(o1) && sq_isnumeric(o2)) {
        if (t1 == OT_INTEGER && t2 == OT_FLOAT) {
            if (_integer(o1) == _float(o2)) {
                result = 0;
            } else if (_integer(o1) < _float(o2)) {
                result = -1;
            } else {
                result = 1;
            }
            return true;
        }

        if (_float(o1) == _integer(o2)) {
            result = 0;
        } else if (_float(o1) < _integer(o2)) {
            result = -1;
        } else {
            result = 1;
        }

        return true;
    }

    if (t1 == OT_NULL) {
        result = -1;
        return true;
    } else if (t2 == OT_NULL) {
        result = 1;
        return true;
    } else {
        Raise_CompareError(o1, o2);
        return false;
    }
}

bool SQVM::CMP_OP(CmpOP op, SQObjectPtr const & o1, SQObjectPtr const & o2, SQObjectPtr & res) {
    SQInteger r;
    if (!ObjCmp(o1, o2, r)) {
        return false;
    }

    switch(op) {
    case CMP_G: res = (r > 0); return true;
    case CMP_GE: res = (r >= 0); return true;
    case CMP_L: res = (r < 0); return true;
    case CMP_LE: res = (r <= 0); return true;
    case CMP_3W: res = r; return true;
    default:
        assert(0); // todo?
    }
}

bool SQVM::ToString(const SQObjectPtr &o,SQObjectPtr &res)
{
    switch(sq_type(o)) {
    case OT_STRING:
        res = o;
        return true;
    case OT_FLOAT:
        scsprintf(_sp(sq_rsl(NUMBER_MAX_CHAR+1)),sq_rsl(NUMBER_MAX_CHAR),_SC("%g"),_float(o));
        break;
    case OT_INTEGER:
        scsprintf(_sp(sq_rsl(NUMBER_MAX_CHAR+1)),sq_rsl(NUMBER_MAX_CHAR),_PRINT_INT_FMT,_integer(o));
        break;
    case OT_BOOL:
        scsprintf(_sp(sq_rsl(6)),sq_rsl(6),_integer(o)?_SC("true"):_SC("false"));
        break;
    case OT_NULL:
        scsprintf(_sp(sq_rsl(5)),sq_rsl(5),_SC("null"));
        break;
    case OT_TABLE:
    case OT_USERDATA:
    case OT_INSTANCE:
        if(_delegable(o)->_delegate) {
            SQObjectPtr closure;
            if(_delegable(o)->GetMetaMethod(this, MT_TOSTRING, closure)) {
                Push(o);
                if(CallMetaMethod(closure,1,res)) {
                    if(sq_type(res) == OT_STRING)
                        return true;
                }
                else {
                    return false;
                }
            }
        }
    default:
        scsprintf(_sp(sq_rsl((sizeof(void*)*2)+NUMBER_MAX_CHAR)),sq_rsl((sizeof(void*)*2)+NUMBER_MAX_CHAR),_SC("(%s : 0x%p)"),GetTypeName(o),(void*)_rawval(o));
    }
    res = this->_sharedstate->gc.AddString(_spval, strlen(_spval));
    return true;
}

bool SQVM::StringCat(SQObjectPtr const & str, SQObjectPtr const & obj, SQObjectPtr & dest) {
    SQObjectPtr a, b;
    if(!ToString(str, a) || !ToString(obj, b)) {
        return false;
    }

    size_t const l = _string(a)->_len;
    size_t const ol = _string(b)->_len;
    dest = this->_sharedstate->gc.ConcatStrings(_stringval(a), l, _stringval(b), ol);
    return true;
}

bool SQVM::TypeOf(SQObjectPtr const & obj1, SQObjectPtr & dest) {
    if (is_delegable(obj1) && _delegable(obj1)->_delegate) {
        SQObjectPtr closure;
        if (_delegable(obj1)->GetMetaMethod(this, MT_TYPEOF, closure)) {
            Push(obj1);
            return CallMetaMethod(closure, 1, dest);
        }
    }
    char const * name = GetTypeName(obj1);
    dest = this->_sharedstate->gc.AddString(name, strlen(name));
    return true;
}

bool SQVM::StartCall(
    SQClosure * closure,
    SQInteger target,
    SQInteger nargs,
    SQInteger stackbase,
    bool tailcall
) {
    SQFunctionProto * func = closure->_function;

    size_t paramssize = func->_nparameters;
    size_t newtop = stackbase + func->_stacksize;
    if (func->_varparams) {
        paramssize--;
        if (size_t(nargs) < paramssize) {
            Raise_Error("wrong number of parameters (%d passed, at least %d required)",
                int(nargs),
                int(paramssize));
            return false;
        }

        size_t nvargs = nargs - paramssize;
        SQArray * arr = SQArray::Create(this->_sharedstate, nvargs);
        size_t pbase = stackbase+paramssize;
        for(size_t n = 0; n < nvargs; n++) {
            arr->_values[n] = _stack._vals[pbase];
            _stack._vals[pbase].Null();
            pbase++;

        }
        _stack._vals[stackbase+paramssize] = arr;
    } else if (paramssize != size_t(nargs)) {
        size_t ndef = func->_ndefaultparams;
        size_t diff;
        if (ndef && size_t(nargs) < paramssize && (diff = paramssize - nargs) <= ndef) {
            for (size_t n = ndef - diff; n < ndef; n++) {
                _stack._vals[stackbase + (nargs++)] = closure->_defaultparams[n];
            }
        } else {
            Raise_Error("wrong number of parameters (%d passed, %d required)",
              int(nargs),
              int(paramssize));
            return false;
        }
    }

    if (closure->_env) {
        _stack._vals[stackbase] = closure->_env->_obj;
    }

    if (!EnterFrame(stackbase, newtop, tailcall)) {
        return false;
    }

    ci->_closure  = closure;
    ci->_literals = func->_literals;
    ci->_ip       = func->_instructions;
    ci->_target   = (SQInt32)target;

    if (_debughook) {
        CallDebugHook(_SC('c'));
    }

    if (closure->_function->_bgenerator) {
        SQFunctionProto * f = closure->_function;
        SQGenerator * gen = SQGenerator::Create(_ss(this), closure);
        // TODO: fix -1, see yield codegen in the compiler
        assert(f->_stacksize != 0);
        if(!gen->Yield(this, SQInteger(f->_stacksize) - 1)) {
            return false;
        }
        SQObjectPtr temp;
        Return(1, target, temp);
        STK(target) = gen;
    }

    return true;
}

bool SQVM::Return(SQInteger _arg0, SQInteger _arg1, SQObjectPtr & retval) {
    bool _isroot = ci->_root ? true : false;
    SQInteger callerbase = _stackbase - ci->_prevstkbase;

    if (_debughook) {
        for(SQInteger i=0; i<ci->_ncalls; i++) {
            CallDebugHook(_SC('r'));
        }
    }

    SQObjectPtr *dest;
    if (_isroot) {
        dest = &(retval);
    } else if (ci->_target == -1) {
        dest = NULL;
    } else {
        dest = &_stack._vals[callerbase + ci->_target];
    }
    if (dest) {
        if(_arg0 != 0xFF) {
            *dest = _stack._vals[_stackbase+_arg1];
        }
        else {
            dest->Null();
        }
        //*dest = (_arg0 != 0xFF) ? _stack._vals[_stackbase+_arg1] : _null_;
    }
    LeaveFrame();
    return _isroot;
}

bool SQVM::PLOCAL_INC(
    uint8_t op,
    SQObjectPtr & target,
    SQObjectPtr & a,
    SQObjectPtr & incr
) {
    SQObjectPtr trg;
    if (!ARITH_OP( op , trg, a, incr)) {
        return false;
    }
    target = a;
    a = trg;
    return true;
}

bool SQVM::DerefInc(
    uint8_t op,
    SQObjectPtr & target,
    SQObjectPtr & self,
    SQObjectPtr & key,
    SQObjectPtr & incr,
    bool postfix,
    SQInteger selfidx
) {
    // TODO: op needs validation!!!

    SQObjectPtr tmp;
    SQObjectPtr tself = self;
    SQObjectPtr tkey = key;
    if (!Get(tself, tkey, tmp, 0, selfidx)) {
        return false;
    }

    if (!ARITH_OP( op , target, tmp, incr)) {
        return false;
    }
    if (!Set(tself, tkey, target, selfidx)) {
        return false;
    }
    if (postfix) {
        target = tmp;
    }
    return true;
}

#define arg0 (_i_._arg0)
#define sarg0 ((SQInteger)*((const signed char *)&_i_._arg0))
#define arg1 (_i_._arg1)
#define sarg1 (*((const SQInt32 *)&_i_._arg1))
#define arg2 (_i_._arg2)
#define arg3 (_i_._arg3)
#define sarg3 ((SQInteger)*((const signed char *)&_i_._arg3))

SQRESULT SQVM::Suspend() {
    if (is_suspended) {
        return sq_throwerror(this, "cannot suspend an already suspended vm");
    }

    if (n_native_calls != 2) {
        return sq_throwerror(this, "cannot suspend through native calls/metamethods");
    }

    return SQ_SUSPEND_FLAG;
}

bool SQVM::FOREACH_OP(
    SQObjectPtr &o1,
    SQObjectPtr &o2,
    SQObjectPtr &o3,
    SQObjectPtr &o4,
    SQInteger SQ_UNUSED_ARG(arg_2),
    int exitpos,
    int & jump
) {
    switch (sq_type(o1)) {
    case OT_TABLE: {
        SQInteger nrefidx = _table(o1)->Next(false, o4, o2, o3);
        if (nrefidx == -1) {
            jump = exitpos;
            return true;
        }
        o4 = nrefidx;
        jump = 1;
        return true;
    }
    case OT_ARRAY: {
        SQInteger nrefidx = _array(o1)->Next(o4, o2, o3);
        if (nrefidx == -1) {
            jump = exitpos;
            return true;
        }
        o4 = nrefidx;
        jump = 1;
        return true;
    }
    case OT_STRING: {
        SQInteger nrefidx = _string(o1)->Next(o4, o2, o3);
        if (nrefidx == -1) {
            jump = exitpos;
            return true;
        }
        o4 = nrefidx;
        jump = 1;
        return true;
    }
    case OT_CLASS: {
        SQInteger nrefidx = _class(o1)->Next(o4, o2, o3);
        if (nrefidx == -1) {
            jump = exitpos;
            return true;
        }
        o4 = nrefidx;
        jump = 1;
        return true;
    }
    case OT_USERDATA:
    case OT_INSTANCE: {
        if (!_delegable(o1)->_delegate) {
            return false;
        }

        SQObjectPtr closure;
        if (!_delegable(o1)->GetMetaMethod(this, MT_NEXTI, closure)) {
            Raise_Error("_nexti failed");
            return false;
        }

        Push(o1);
        Push(o4);
        SQObjectPtr itr;
        if (!CallMetaMethod(closure, 2, itr)) {
            return false;
        }
        o4 = o2 = itr;

        if (sq_type(itr) == OT_NULL) {
            jump = exitpos;
            return true;
        }

        if(!Get(o1, itr, o3, 0, DONT_FALL_BACK)) {
            Raise_Error("_nexti returned an invalid idx");
            return false;
        }

        jump = 1;
        return true;
    }
    case OT_GENERATOR:
        if (_generator(o1)->_state == SQGenerator::eDead) {
            jump = exitpos;
            return true;
        }

        if (_generator(o1)->_state == SQGenerator::eSuspended) {
            SQInteger idx = 0;
            if(sq_type(o4) == OT_INTEGER) {
                idx = _integer(o4) + 1;
            }
            o2 = idx;
            o4 = idx;
            _generator(o1)->Resume(this, o3);

            jump = 0;
            return true;
        }

        return false;
    default:
        Raise_Error("cannot iterate %s", GetTypeName(o1));
        return false;
    }
}

#define COND_LITERAL (arg3!=0?ci->_literals[arg1]:STK(arg1))

#define SQ_THROW() { goto exception_trap; }

#define _GUARD(exp) { if(!exp) { SQ_THROW();} }

bool SQVM::CLOSURE_OP(SQObjectPtr &target, SQFunctionProto *func,SQInteger boundtarget)
{
    SQInteger nouters;
    SQClosure *closure = SQClosure::Create(_ss(this), func,_table(_roottable)->GetWeakRef(OT_TABLE));
    if((nouters = func->_noutervalues)) {
        for(SQInteger i = 0; i<nouters; i++) {
            SQOuterVar &v = func->_outervalues[i];
            switch(v._type){
            case otLOCAL:
                FindOuter(closure->_outervalues[i], &STK(_integer(v._src)));
                break;
            case otOUTER:
                closure->_outervalues[i] = _closure(ci->_closure)->_outervalues[_integer(v._src)];
                break;
            }
        }
    }
    SQInteger ndefparams;
    if((ndefparams = func->_ndefaultparams)) {
        for(SQInteger i = 0; i < ndefparams; i++) {
            SQInteger spos = func->_defaultparams[i];
            closure->_defaultparams[i] = _stack._vals[_stackbase + spos];
        }
    }
    if (boundtarget != 0xFF) {
        SQObjectPtr &val = _stack._vals[_stackbase + boundtarget];
        SQObjectType t = sq_type(val);
        if (t == OT_TABLE || t == OT_CLASS || t == OT_INSTANCE || t == OT_ARRAY) {
            closure->_env = _refcounted(val)->GetWeakRef(t);
            closure->_env->IncreaseRefCount();
        }
        else {
            Raise_Error(_SC("cannot bind a %s as environment object"), IdType2Name(t));
            closure->Release();
            return false;
        }
    }
    target = closure;
    return true;

}


bool SQVM::CLASS_OP(SQObjectPtr & target, SQInteger baseclass, SQInteger attributes) {
    SQClass * base = NULL;
    SQObjectPtr attrs;
    if (baseclass != -1) {
        if (sq_type(_stack._vals[_stackbase + baseclass]) != OT_CLASS) {
            Raise_Error("trying to inherit from a %s", GetTypeName(_stack._vals[_stackbase + baseclass]));
            return false;
        }
        base = _class(_stack._vals[_stackbase + baseclass]);
    }
    // attributes == MAX_FUNC_STACKSIZE would still be valid?
    if(attributes != MAX_FUNC_STACKSIZE) {
        attrs = _stack._vals[_stackbase + attributes];
    }
    target = SQClass::Create(_ss(this),base);
    if(sq_type(_class(target)->_metamethods[MT_INHERITED]) != OT_NULL) {
        int nparams = 2;
        SQObjectPtr ret;
        Push(target); Push(attrs);
        if(!Call(_class(target)->_metamethods[MT_INHERITED],nparams,stack_top - nparams, ret, false)) {
            Pop(nparams);
            return false;
        }
        Pop(nparams);
    }
    _class(target)->_attributes = attrs;
    return true;
}

bool SQVM::IsEqual(SQObjectPtr const & o1, SQObjectPtr const & o2, bool & res) {
    SQObjectType t1 = sq_type(o1), t2 = sq_type(o2);
    if (t1 == t2) {
        if (t1 == OT_FLOAT) {
            res = (_float(o1) == _float(o2));
        }
        else {
            res = (_rawval(o1) == _rawval(o2));
        }
    } else {
        if (sq_isnumeric(o1) && sq_isnumeric(o2)) {
            res = (tofloat(o1) == tofloat(o2));
        }
        else {
            res = false;
        }
    }
    return true;
}

bool SQVM::IsFalse(SQObjectPtr & o) {
    if (sq_type(o) == OT_FLOAT && _float(o) == SQFloat(0.0)) {
        return true;
    }

#if !defined(SQUSEDOUBLE) || (defined(SQUSEDOUBLE) && defined(_SQ64))
    return _integer(o) == 0;
#else
    return sq_type(o) != OT_FLOAT && _integer(o) == 0;
#endif
}

bool SQVM::Execute(
    SQObjectPtr &closure,
    SQInteger nargs,
    SQInteger stackbase,
    SQObjectPtr &outres,
    SQBool raiseerror,
    ExecutionType et
) {
    if (n_native_calls + 1 > MAX_NATIVE_CALLS) {
        Raise_Error("Native stack overflow");
        return false;
    }

    n_native_calls++;
    AutoDec ad(&n_native_calls);

    SQInteger traps = 0;
    CallInfo *prevci = ci;

    switch(et) {
    case ET_CALL: {
        temp_reg = closure;
        if(!StartCall(_closure(temp_reg), stack_top - nargs, nargs, stackbase, false)) {
            //call the handler if there are no calls in the stack, if not relies on the previous node
            if (ci == NULL) {
                CallErrorHandler(_lasterror);
            }
            return false;
        }
        if(ci == prevci) {
            outres = STK(stack_top-nargs);
            return true;
        }
        ci->_root = SQTrue;
        }
        break;
    case ET_RESUME_GENERATOR: 
        if(!_generator(closure)->Resume(this, outres)) {
            return false;
        }
        ci->_root = SQTrue;
        traps += ci->_etraps;
        break;
    case ET_RESUME_VM:
    case ET_RESUME_THROW_VM:
        traps = _suspended_traps;
        ci->_root = _suspended_root ? SQTrue : SQFalse;
        is_suspended = false;
        if(et  == ET_RESUME_THROW_VM) { SQ_THROW(); }
        break;
    }

exception_restore:
    for(;;) {
        const SQInstruction &_i_ = *ci->_ip++;
        switch (_i_.op) {
        case _OP_LINE:
            if (_debughook) {
                CallDebugHook(_SC('l'), arg1);
            }
            continue;
        case _OP_LOAD:
            TARGET = ci->_literals[arg1];
            continue;
        case _OP_LOADINT:
#ifndef _SQ64
            TARGET = (SQInteger)arg1; continue;
#else
            TARGET = (SQInteger)((SQInt32)arg1); continue;
#endif
        case _OP_LOADFLOAT:
            TARGET = *((const SQFloat *)&arg1); continue;
        case _OP_DLOAD: TARGET = ci->_literals[arg1]; STK(arg2) = ci->_literals[arg3];continue;
        case _OP_TAILCALL:{
            SQObjectPtr &t = STK(arg1);
            if (sq_type(t) == OT_CLOSURE
                && (!_closure(t)->_function->_bgenerator)){
                SQObjectPtr clo = t;
                SQInteger last_top = stack_top;
                if (_openouters) {
                    CloseOuters(&(_stack._vals[_stackbase]));
                }
                for (SQInteger i = 0; i < arg3; i++) STK(i) = STK(arg2 + i);
                _GUARD(StartCall(_closure(clo), ci->_target, arg3, _stackbase, true));
                if (last_top >= stack_top) {
                    stack_top = last_top;
                }
                continue;
            }
                          }
        case _OP_CALL: {
                SQObjectPtr clo = STK(arg1);
                switch (sq_type(clo)) {
                case OT_CLOSURE:
                    _GUARD(StartCall(_closure(clo), sarg0, arg3, _stackbase+arg2, false));
                    continue;
                case OT_NATIVECLOSURE: {
                    bool suspend;
                    bool tailcall;
                    _GUARD(CallNative(_nativeclosure(clo), arg3, _stackbase+arg2, clo, (SQInt32)sarg0, suspend, tailcall));
                    if(suspend){
                        is_suspended = true;
                        _suspended_target = sarg0;
                        _suspended_root = ci->_root ? true : false;
                        _suspended_traps = traps;
                        outres = clo;
                        return true;
                    }
                    if(sarg0 != -1 && !tailcall) {
                        STK(arg0) = clo;
                    }
                                       }
                    continue;
                case OT_CLASS:{
                    SQObjectPtr inst;
                    CreateClassInstance(_class(clo), inst, clo);
                    if(sarg0 != -1) {
                        STK(arg0) = inst;
                    }
                    SQInteger stkbase;
                    switch(sq_type(clo)) {
                        case OT_CLOSURE:
                            stkbase = _stackbase+arg2;
                            _stack._vals[stkbase] = inst;
                            _GUARD(StartCall(_closure(clo), -1, arg3, stkbase, false));
                            break;
                        case OT_NATIVECLOSURE:
                            bool dummy;
                            stkbase = _stackbase+arg2;
                            _stack._vals[stkbase] = inst;
                            _GUARD(CallNative(_nativeclosure(clo), arg3, stkbase, clo, -1, dummy, dummy));
                            break;
                        default: break; //shutup GCC 4.x
                    }
                    }
                    break;
                case OT_TABLE:
                case OT_USERDATA:
                case OT_INSTANCE:{
                    SQObjectPtr closure;
                    if(_delegable(clo)->_delegate && _delegable(clo)->GetMetaMethod(this,MT_CALL,closure)) {
                        Push(clo);
                        for (SQInteger i = 0; i < arg3; i++) Push(STK(arg2 + i));
                        if(!CallMetaMethod(closure, arg3+1, clo)) SQ_THROW();
                        if(sarg0 != -1) {
                            STK(arg0) = clo;
                        }
                        break;
                    }

                    //Raise_Error(_SC("attempt to call '%s'"), GetTypeName(clo));
                    //SQ_THROW();
                  }
                default:
                    Raise_Error(_SC("attempt to call '%s'"), GetTypeName(clo));
                    SQ_THROW();
                }
            }
              continue;
        case _OP_PREPCALL:
        case _OP_PREPCALLK: {
                SQObjectPtr & key = (_i_.op == _OP_PREPCALLK)
                    ? (ci->_literals)[arg1]
                    : STK(arg1);
                SQObjectPtr & o = STK(arg2);
                if (!Get(o, key, temp_reg,0,arg2)) {
                    SQ_THROW();
                }
                STK(arg3) = o;
                _Swap(TARGET,temp_reg);//TARGET = temp_reg;
            }
            continue;
        case _OP_GETK:
            if (!Get(STK(arg2), ci->_literals[arg1], temp_reg, 0,arg2)) {
                SQ_THROW();
            }
            _Swap(TARGET,temp_reg);//TARGET = temp_reg;
            continue;
        case _OP_MOVE: TARGET = STK(arg1); continue;
        case _OP_NEWSLOT:
            _GUARD(NewSlot(STK(arg1), STK(arg2), STK(arg3),false));
            if(arg0 != 0xFF) TARGET = STK(arg3);
            continue;
        case _OP_DELETE: _GUARD(DeleteSlot(STK(arg1), STK(arg2), TARGET)); continue;
        case _OP_SET:
            if (!Set(STK(arg1), STK(arg2), STK(arg3),arg1)) { SQ_THROW(); }
            if (arg0 != 0xFF) TARGET = STK(arg3);
            continue;
        case _OP_GET:
            if (!Get(STK(arg1), STK(arg2), temp_reg, 0,arg1)) { SQ_THROW(); }
            _Swap(TARGET,temp_reg);//TARGET = temp_reg;
            continue;
        case _OP_EQ:{
            bool res;
            if(!IsEqual(STK(arg2),COND_LITERAL,res)) { SQ_THROW(); }
            TARGET = res?true:false;
            }continue;
        case _OP_NE:{
            bool res;
            if(!IsEqual(STK(arg2),COND_LITERAL,res)) { SQ_THROW(); }
            TARGET = (!res)?true:false;
            } continue;
        case _OP_ADD:
            _GUARD(_ARITH_('+', TARGET, STK(arg2), STK(arg1)));
            continue;
        case _OP_SUB:
            _GUARD(_ARITH_('-', TARGET, STK(arg2), STK(arg1)));
            continue;
        case _OP_MUL:
            _GUARD(_ARITH_('*', TARGET, STK(arg2), STK(arg1)));
            continue;
        case _OP_DIV:
            _GUARD(_ARITH_('/', TARGET, STK(arg2), STK(arg1)));
            continue;
        case _OP_MOD:
            ARITH_OP('%',TARGET,STK(arg2),STK(arg1));
            continue;
        case _OP_BITW:
            _GUARD(BW_OP(arg3, TARGET, STK(arg2), STK(arg1)));
            continue;
        case _OP_RETURN:
            if ((ci)->_generator) {
                (ci)->_generator->Kill();
            }
            if (Return(arg0, arg1, temp_reg)) {
                assert(traps == 0);
                _Swap(outres,temp_reg);
                return true;
            }
            continue;
        case _OP_LOADNULLS:{ for(SQInt32 n=0; n < arg1; n++) STK(arg0+n).Null(); }continue;
        case _OP_LOADROOT:  {
            SQWeakRef *w = _closure(ci->_closure)->_root;
            if(sq_type(w->_obj) != OT_NULL) {
                TARGET = w->_obj;
            } else {
                TARGET = _roottable; //shoud this be like this? or null
            }
                            }
            continue;
        case _OP_LOADBOOL: TARGET = arg1?true:false; continue;
        case _OP_DMOVE: STK(arg0) = STK(arg1); STK(arg2) = STK(arg3); continue;
        case _OP_JMP: ci->_ip += (sarg1); continue;
        //case _OP_JNZ: if(!IsFalse(STK(arg0))) ci->_ip+=(sarg1); continue;
        case _OP_JCMP:
            _GUARD(CMP_OP((CmpOP)arg3,STK(arg2),STK(arg0),temp_reg));
            if(IsFalse(temp_reg)) ci->_ip+=(sarg1);
            continue;
        case _OP_JZ: if(IsFalse(STK(arg0))) ci->_ip+=(sarg1); continue;
        case _OP_GETOUTER: {
            SQClosure *cur_cls = _closure(ci->_closure);
            SQOuter *otr = _outer(cur_cls->_outervalues[arg1]);
            TARGET = *(otr->_valptr);
            }
        continue;
        case _OP_SETOUTER: {
            SQClosure *cur_cls = _closure(ci->_closure);
            SQOuter   *otr = _outer(cur_cls->_outervalues[arg1]);
            *(otr->_valptr) = STK(arg2);
            if(arg0 != 0xFF) {
                TARGET = STK(arg2);
            }
            }
        continue;
        case _OP_NEWOBJ:
            switch(arg3) {
            case NOT_TABLE:
                TARGET = SQTable::Create(_ss(this), arg1);
                continue;
            case NOT_ARRAY:
                TARGET = SQArray::Create(_ss(this), 0);
                _array(TARGET)->Reserve(arg1);
                continue;
            case NOT_CLASS:
                _GUARD(CLASS_OP(TARGET, arg1, arg2));
                continue;
            default:
                assert(0);
                continue;
            }
        case _OP_APPENDARRAY:
            {
                SQObject val;
                val._unVal.raw = 0;
            switch(arg2) {
            case AAT_STACK:
                val = STK(arg1); break;
            case AAT_LITERAL:
                val = ci->_literals[arg1]; break;
            case AAT_INT:
                val._type = OT_INTEGER;
#ifndef _SQ64
                val._unVal.nInteger = (SQInteger)arg1;
#else
                val._unVal.nInteger = (SQInteger)((SQInt32)arg1);
#endif
                break;
            case AAT_FLOAT:
                val._type = OT_FLOAT;
                val._unVal.fFloat = *((const SQFloat *)&arg1);
                break;
            case AAT_BOOL:
                val._type = OT_BOOL;
                val._unVal.nInteger = arg1;
                break;
            default: val._type = OT_INTEGER; assert(0); break;

            }
            _array(STK(arg0))->Append(val); continue;
            }
        case _OP_COMPARITH: {
            SQInteger selfidx = (SQUnsignedInteger(arg1) & 0xFFFF0000) >> 16;
            _GUARD(DerefInc(arg3, TARGET, STK(selfidx), STK(arg2), STK(arg1 & 0x0000FFFF), false, selfidx));
            continue;
        }
        case _OP_INC: {
            SQObjectPtr o(sarg3);
            _GUARD(DerefInc('+',TARGET, STK(arg1), STK(arg2), o, false, arg1));
            continue;
        }
        case _OP_INCL: {
            SQObjectPtr &a = STK(arg1);
            if(sq_type(a) == OT_INTEGER) {
                a._unVal.nInteger = _integer(a) + sarg3;
            }
            else {
                SQObjectPtr o(sarg3); //_GUARD(LOCAL_INC('+',TARGET, STK(arg1), o));
                _GUARD(_ARITH_('+', a, a, o));
            }
                       } continue;
        case _OP_PINC: {SQObjectPtr o(sarg3); _GUARD(DerefInc('+',TARGET, STK(arg1), STK(arg2), o, true, arg1));} continue;
        case _OP_PINCL: {
            SQObjectPtr &a = STK(arg1);
            if(sq_type(a) == OT_INTEGER) {
                TARGET = a;
                a._unVal.nInteger = _integer(a) + sarg3;
            }
            else {
                SQObjectPtr o(sarg3); _GUARD(PLOCAL_INC('+',TARGET, STK(arg1), o));
            }

                    } continue;
        case _OP_CMP:   _GUARD(CMP_OP((CmpOP)arg3,STK(arg2),STK(arg1),TARGET))  continue;
        case _OP_EXISTS: TARGET = Get(STK(arg1), STK(arg2), temp_reg, GET_FLAG_DO_NOT_RAISE_ERROR | GET_FLAG_RAW, DONT_FALL_BACK) ? true : false; continue;
        case _OP_INSTANCEOF:
            if(sq_type(STK(arg1)) != OT_CLASS)
            {Raise_Error(_SC("cannot apply instanceof between a %s and a %s"),GetTypeName(STK(arg1)),GetTypeName(STK(arg2))); SQ_THROW();}
            TARGET = (sq_type(STK(arg2)) == OT_INSTANCE) ? (_instance(STK(arg2))->InstanceOf(_class(STK(arg1)))?true:false) : false;
            continue;
        case _OP_AND:
            if(IsFalse(STK(arg2))) {
                TARGET = STK(arg2);
                ci->_ip += (sarg1);
            }
            continue;
        case _OP_OR:
            if(!IsFalse(STK(arg2))) {
                TARGET = STK(arg2);
                ci->_ip += (sarg1);
            }
            continue;
        case _OP_NEG: _GUARD(NEG_OP(TARGET,STK(arg1))); continue;
        case _OP_NOT: TARGET = IsFalse(STK(arg1)); continue;
        case _OP_BWNOT:
            if(sq_type(STK(arg1)) == OT_INTEGER) {
                SQInteger t = _integer(STK(arg1));
                TARGET = SQInteger(~t);
                continue;
            }
            Raise_Error(_SC("attempt to perform a bitwise op on a %s"), GetTypeName(STK(arg1)));
            SQ_THROW();
        case _OP_CLOSURE: {
            SQClosure *c = ci->_closure._unVal.pClosure;
            SQFunctionProto *fp = c->_function;
            if(!CLOSURE_OP(TARGET,fp->_functions[arg1]._unVal.pFunctionProto, arg2)) { SQ_THROW(); }
            continue;
        }
        case _OP_YIELD: {
            if (ci->_generator) {
                // Uhh, sarg1 == MAX_FUNC_STACKSIZE is still valid, I think?
                if (sarg1 <= MAX_FUNC_STACKSIZE) {
                    temp_reg = STK(arg1);
                }
                if (_openouters) {
                    CloseOuters(&_stack._vals[_stackbase]);
                }
                // TODO: fix -1, see yield codegen in the compiler
                _GUARD(ci->_generator->Yield(this, uint8_t(arg2 - 1)));
                traps -= ci->_etraps;
                if (sarg1 <= MAX_FUNC_STACKSIZE) {
                    _Swap(STK(arg1), temp_reg);
                }
            } else {
                Raise_Error("trying to yield a '%s',only genenerator can be yielded", GetTypeName(ci->_generator));
                SQ_THROW();
            }

            if (Return(arg0, arg1, temp_reg)) {
                assert(traps == 0);
                outres = temp_reg;
                return true;
            }

            continue;
        }
        case _OP_RESUME:
            if (sq_type(STK(arg1)) != OT_GENERATOR) {
                Raise_Error("trying to resume a '%s',only genenerator can be resumed", GetTypeName(STK(arg1)));
                SQ_THROW();
            }
            _GUARD(_generator(STK(arg1))->Resume(this, TARGET));
            traps += ci->_etraps;
            continue;
        case _OP_FOREACH: {
            int tojump;
            _GUARD(FOREACH_OP(STK(arg0), STK(arg2), STK(arg2 + 1), STK(arg2 + 2), arg2, sarg1, tojump));
            ci->_ip += tojump;
            continue;
        }
        case _OP_POSTFOREACH:
            assert(sq_type(STK(arg0)) == OT_GENERATOR);
            if(_generator(STK(arg0))->_state == SQGenerator::eDead)
                ci->_ip += (sarg1 - 1);
            continue;
        case _OP_CLONE:
            _GUARD(Clone(STK(arg1), TARGET));
            continue;
        case _OP_TYPEOF:
            _GUARD(TypeOf(STK(arg1), TARGET));
            continue;
        case _OP_PUSHTRAP:{
            SQInstruction *_iv = _closure(ci->_closure)->_function->_instructions;
            _etraps.push_back(SQExceptionTrap(stack_top,_stackbase, &_iv[(ci->_ip-_iv)+arg1], arg0)); traps++;
            ci->_etraps++;
            continue;
        }
        case _OP_POPTRAP: {
            for(SQInteger i = 0; i < arg0; i++) {
                _etraps.pop_back(); traps--;
                ci->_etraps--;
            }
                          }
            continue;
        case _OP_THROW: Raise_Error(TARGET); SQ_THROW(); continue;
        case _OP_NEWSLOTA:
            _GUARD(NewSlotA(STK(arg1),STK(arg2),STK(arg3),(arg0&NEW_SLOT_ATTRIBUTES_FLAG) ? STK(arg2-1) : SQObjectPtr(),(arg0&NEW_SLOT_STATIC_FLAG)?true:false,false));
            continue;
        case _OP_GETBASE:{
            SQClosure *clo = _closure(ci->_closure);
            if(clo->_base) {
                TARGET = clo->_base;
            }
            else {
                TARGET.Null();
            }
            continue;
        }
        case _OP_CLOSE:
            if (_openouters) {
                CloseOuters(&(STK(arg1)));
            }
            continue;
        }

    }

exception_trap:
    {
        SQObjectPtr currerror = _lasterror;
        SQInteger last_top = stack_top;

        if(_ss(this)->_notifyallexceptions || (!traps && raiseerror)) {
            CallErrorHandler(currerror);
        }

        while( ci ) {
            if(ci->_etraps > 0) {
                SQExceptionTrap &et = _etraps.top();
                ci->_ip = et._ip;
                stack_top = et._stacksize;
                _stackbase = et._stackbase;
                _stack._vals[_stackbase + et._extarget] = currerror;
                _etraps.pop_back(); traps--; ci->_etraps--;
                while(last_top >= stack_top) _stack._vals[last_top--].Null();
                goto exception_restore;
            }
            else if (_debughook) {
                    //notify debugger of a "return"
                    //even if it really an exception unwinding the stack
                    for(SQInteger i = 0; i < ci->_ncalls; i++) {
                        CallDebugHook(_SC('r'));
                    }
            }
            if(ci->_generator) ci->_generator->Kill();
            bool mustbreak = ci && ci->_root;
            LeaveFrame();
            if(mustbreak) break;
        }

        _lasterror = currerror;
        return false;
    }

    assert(0);
    return false;
}

void SQVM::CallErrorHandler(SQObjectPtr & error) {
    if (sq_type(_errorhandler) == OT_NULL) {
        return;
    }
    
    SQObjectPtr out;
    Push(_roottable);
    Push(error);
    Call(_errorhandler, 2, stack_top - 2, out, SQFalse);
    Pop(2);
}


void SQVM::CallDebugHook(SQInteger type,SQInteger forcedline) {
    _debughook = false;
    SQFunctionProto *func=_closure(ci->_closure)->_function;
    if(_debughook_native) {
        const SQChar *src = sq_type(func->_sourcename) == OT_STRING?_stringval(func->_sourcename):NULL;
        const SQChar *fname = sq_type(func->_name) == OT_STRING?_stringval(func->_name):NULL;
        SQInteger line = forcedline?forcedline:func->GetLine(ci->_ip);
        _debughook_native(this,type,src,line,fname);
    }
    else {
        SQObjectPtr temp_reg;
        SQInteger nparams=5;
        Push(_roottable); Push(type); Push(func->_sourcename); Push(forcedline?forcedline:func->GetLine(ci->_ip)); Push(func->_name);
        Call(_debughook_closure,nparams,stack_top-nparams,temp_reg,SQFalse);
        Pop(nparams);
    }
    _debughook = true;
}

bool SQVM::CallNative(
    SQNativeClosure * native_closure,
    SQInteger nargs,
    SQInteger newbase,
    SQObjectPtr & retval,
    SQInt32 target,
    bool & suspend,
    bool & tailcall
) {
    if (n_native_calls + 1 > MAX_NATIVE_CALLS) {
        Raise_Error("Native stack overflow");
        return false;
    }

    SQInteger nparamscheck = native_closure->_nparamscheck;
    if (nparamscheck) {
        // Exact number of arguments
        if (nparamscheck > 0 && nparamscheck != nargs) {
            Raise_Error("wrong number of parameters");
            return false;
        }

        // "at least" number of arguments
        if (nparamscheck < 0 && nargs < -nparamscheck) {
            Raise_Error("wrong number of parameters");
            return false;
        }
    }

    sqvector<SQInteger> & tc = native_closure->_typecheck;
    size_t n_type_checks = tc.size();
    for (size_t i = 0; SQInteger(i) < nargs && i < n_type_checks; i++) {
        if (tc._vals[i] == -1) {
            continue;
        }

        if (sq_type(_stack._vals[size_t(newbase) + i]) & tc._vals[i]) {
            continue;
        }
        
        Raise_ParamTypeError(i, tc._vals[i], sq_type(_stack._vals[size_t(newbase) + i]));
        return false;
    }

    SQInteger newtop = newbase + nargs + native_closure->_noutervalues;
    if (!EnterFrame(newbase, newtop, false)) {
        return false;
    }

    ci->_closure = native_closure;
    ci->_target = target;

    SQInteger outers = native_closure->_noutervalues;
    for (SQInteger i = 0; i < outers; i++) {
        _stack._vals[newbase + nargs + i] = native_closure->_outervalues[i];
    }

    if (native_closure->_env) {
        _stack._vals[newbase] = native_closure->_env->_obj;
    }

    n_native_calls++;
    SQInteger ret = (native_closure->_function)(this);
    n_native_calls--;

    if (ret == SQ_TAILCALL_FLAG) {
        suspend = false;
        tailcall = true;
        return true;
    }

    if (ret == SQ_SUSPEND_FLAG) {
        suspend = true;
        tailcall = false;
        retval = _stack._vals[stack_top-1];
        LeaveFrame();
        return true;
    }

    if (ret < 0) {
        suspend = false;
        tailcall = false;
        LeaveFrame();
        Raise_Error(_lasterror);
        return false;
    }

    suspend = false;
    tailcall = false;

    if (ret) {
        retval = _stack._vals[stack_top-1];
    } else {
        retval.Null();
    }

    LeaveFrame();
    return true;
}

bool SQVM::TailCall(SQClosure *closure, SQInteger parambase,SQInteger nparams)
{
    SQInteger last_top = stack_top;
    SQObjectPtr clo = closure;
    if (ci->_root)
    {
        Raise_Error("root calls cannot invoke tailcalls");
        return false;
    }
    for (SQInteger i = 0; i < nparams; i++) STK(i) = STK(parambase + i);
    bool ret = StartCall(closure, ci->_target, nparams, _stackbase, true);
    if (last_top >= stack_top) {
        stack_top = last_top;
    }
    return ret;
}

#define FALLBACK_OK         0
#define FALLBACK_NO_MATCH   1
#define FALLBACK_ERROR      2

bool SQVM::Get(
    SQObjectPtr const & self,
    SQObjectPtr const & key,
    SQObjectPtr & dest,
    SQUnsignedInteger getflags,
    SQInteger selfidx
) {
    switch (sq_type(self)) {
    case OT_TABLE:
        if(_table(self)->Get(key,dest)) return true;
        break;
    case OT_ARRAY:
        if (sq_isnumeric(key)) {
            if (_array(self)->Get(tointeger(key), dest)) {
                return true;
            }

            if ((getflags & GET_FLAG_DO_NOT_RAISE_ERROR) == 0) {
                Raise_IdxError(key);
            }

            return false;
        }
        break;
    case OT_INSTANCE:
        if(_instance(self)->Get(key, dest)) return true;
        break;
    case OT_CLASS:
        if(_class(self)->Get(key,dest)) return true;
        break;
    case OT_STRING:
        if(sq_isnumeric(key)){
            SQInteger n = tointeger(key);
            SQInteger len = _string(self)->_len;
            if (n < 0) { n += len; }
            if (n >= 0 && n < len) {
                dest = SQInteger(_stringval(self)[n]);
                return true;
            }
            if ((getflags & GET_FLAG_DO_NOT_RAISE_ERROR) == 0) Raise_IdxError(key);
            return false;
        }
        break;
    default:
        break; //shut up compiler
    }

    if ((getflags & GET_FLAG_RAW) == 0) {
        switch(FallBackGet(self,key,dest)) {
            case FALLBACK_OK: return true; //okie
            case FALLBACK_NO_MATCH: break; //keep falling back
            case FALLBACK_ERROR: return false; // the metamethod failed
        }
        if(InvokeDefaultDelegate(self,key,dest)) {
            return true;
        }
    }

    if(selfidx == 0) {
        SQWeakRef *w = _closure(ci->_closure)->_root;
        if(sq_type(w->_obj) != OT_NULL) {
            if(Get(*((const SQObjectPtr *)&w->_obj),key,dest,0,DONT_FALL_BACK)) {
                return true;
            }
        }
    }

    if ((getflags & GET_FLAG_DO_NOT_RAISE_ERROR) == 0) {
        Raise_IdxError(key);
    }

    return false;
}

bool SQVM::InvokeDefaultDelegate(
    SQObjectPtr const & self,
    SQObjectPtr const & key,
    SQObjectPtr & dest)
{
    switch (sq_type(self)) {
    case OT_CLASS:
        return _class_ddel->Get(key, dest);
    case OT_TABLE:
        return _table_ddel->Get(key, dest);
    case OT_ARRAY:
        return _array_ddel->Get(key, dest);
    case OT_STRING:
        return _string_ddel->Get(key, dest);
    case OT_INSTANCE:
        return _instance_ddel->Get(key, dest);
    case OT_INTEGER:
    case OT_FLOAT:
    case OT_BOOL:
        return _number_ddel->Get(key, dest);
    case OT_GENERATOR:
        return _generator_ddel->Get(key, dest);
    case OT_CLOSURE:
    case OT_NATIVECLOSURE:
        return _closure_ddel->Get(key, dest);
    case OT_THREAD:
        return _thread_ddel->Get(key, dest);
    case OT_WEAKREF:
        return _weakref_ddel->Get(key, dest);
    default:
        return false;
    }
}


SQInteger SQVM::FallBackGet(
    SQObjectPtr const & self,
    SQObjectPtr const & key,
    SQObjectPtr & dest
) {
    switch (sq_type(self)) {
    case OT_TABLE:
    case OT_USERDATA:
        if (!_delegable(self)->_delegate) {
            return FALLBACK_NO_MATCH;
        }
        if (Get(SQObjectPtr(_delegable(self)->_delegate), key, dest, GET_FLAG_DO_NOT_RAISE_ERROR, DONT_FALL_BACK)) {
            return FALLBACK_OK;
        }
        [[fallthrough]];
    case OT_INSTANCE: {
        SQObjectPtr closure;
        if (_delegable(self)->GetMetaMethod(this, MT_GET, closure)) {
            Push(self);
            Push(key);

            n_metamethod_calls++;
            AutoDec ad(&n_metamethod_calls);

            if(Call(closure, 2, stack_top - 2, dest, SQFalse)) {
                Pop(2);
                return FALLBACK_OK;
            }
            else {
                Pop(2);
                if(sq_type(_lasterror) != OT_NULL) { //NULL means "clean failure" (not found)
                    return FALLBACK_ERROR;
                }
            }
        }
        break;
    }
    default:
        break;//shutup GCC 4.x
    }
    // no metamethod or no fallback type
    return FALLBACK_NO_MATCH;
}

bool SQVM::Set(const SQObjectPtr &self,const SQObjectPtr &key,const SQObjectPtr &val,SQInteger selfidx)
{
    switch(sq_type(self)){
    case OT_TABLE:
        if(_table(self)->Set(key,val)) return true;
        break;
    case OT_INSTANCE:
        if(_instance(self)->Set(key,val)) return true;
        break;
    case OT_ARRAY:
        if(!sq_isnumeric(key)) { Raise_Error(_SC("indexing %s with %s"),GetTypeName(self),GetTypeName(key)); return false; }
        if(!_array(self)->Set(tointeger(key),val)) {
            Raise_IdxError(key);
            return false;
        }
        return true;
    case OT_USERDATA: break; // must fall back
    default:
        Raise_Error(_SC("trying to set '%s'"),GetTypeName(self));
        return false;
    }

    switch(FallBackSet(self,key,val)) {
        case FALLBACK_OK: return true; //okie
        case FALLBACK_NO_MATCH: break; //keep falling back
        case FALLBACK_ERROR: return false; // the metamethod failed
    }
    if(selfidx == 0) {
        if(_table(_roottable)->Set(key,val))
            return true;
    }
    Raise_IdxError(key);
    return false;
}

SQInteger SQVM::FallBackSet(const SQObjectPtr &self,const SQObjectPtr &key,const SQObjectPtr &val)
{
    switch(sq_type(self)) {
    case OT_TABLE:
        if(_table(self)->_delegate) {
            if(Set(_table(self)->_delegate,key,val,DONT_FALL_BACK)) return FALLBACK_OK;
        }
        //keps on going
    case OT_INSTANCE:
    case OT_USERDATA:{
        SQObjectPtr closure;
        SQObjectPtr t;
        if(_delegable(self)->GetMetaMethod(this, MT_SET, closure)) {
            Push(self);Push(key);Push(val);

            n_metamethod_calls++;
            AutoDec ad(&n_metamethod_calls);

            if(Call(closure, 3, stack_top - 3, t, SQFalse)) {
                Pop(3);
                return FALLBACK_OK;
            }
            else {
                Pop(3);
                if(sq_type(_lasterror) != OT_NULL) { //NULL means "clean failure" (not found)
                    return FALLBACK_ERROR;
                }
            }
        }
                     }
        break;
        default: break;//shutup GCC 4.x
    }
    // no metamethod or no fallback type
    return FALLBACK_NO_MATCH;
}

bool SQVM::Clone(const SQObjectPtr &self,SQObjectPtr &target)
{
    SQObjectPtr temp_reg;
    SQObjectPtr newobj;
    switch(sq_type(self)){
    case OT_TABLE:
        newobj = _table(self)->Clone();
        goto cloned_mt;
    case OT_INSTANCE: {
        newobj = _instance(self)->Clone(_ss(this));
cloned_mt:
        SQObjectPtr closure;
        if(_delegable(newobj)->_delegate && _delegable(newobj)->GetMetaMethod(this,MT_CLONED,closure)) {
            Push(newobj);
            Push(self);
            if(!CallMetaMethod(closure,2,temp_reg))
                return false;
        }
        }
        target = newobj;
        return true;
    case OT_ARRAY:
        target = _array(self)->Clone();
        return true;
    default:
        Raise_Error(_SC("cloning a %s"), GetTypeName(self));
        return false;
    }
}

bool SQVM::NewSlotA(const SQObjectPtr &self,const SQObjectPtr &key,const SQObjectPtr &val,const SQObjectPtr &attrs,bool bstatic,bool raw)
{
    if(sq_type(self) != OT_CLASS) {
        Raise_Error(_SC("object must be a class"));
        return false;
    }
    SQClass *c = _class(self);
    if(!raw) {
        SQObjectPtr &mm = c->_metamethods[MT_NEWMEMBER];
        if(sq_type(mm) != OT_NULL ) {
            Push(self); Push(key); Push(val);
            Push(attrs);
            Push(bstatic);
            return CallMetaMethod(mm,5,temp_reg);
        }
    }
    if(!NewSlot(self, key, val,bstatic))
        return false;
    if(sq_type(attrs) != OT_NULL) {
        c->SetAttributes(key,attrs);
    }
    return true;
}

bool SQVM::NewSlot(const SQObjectPtr &self,const SQObjectPtr &key,const SQObjectPtr &val,bool bstatic)
{
    if(sq_type(key) == OT_NULL) { Raise_Error(_SC("null cannot be used as index")); return false; }
    switch(sq_type(self)) {
    case OT_TABLE: {
        bool rawcall = true;
        if(_table(self)->_delegate) {
            SQObjectPtr res;
            if(!_table(self)->Get(key,res)) {
                SQObjectPtr closure;
                if(_delegable(self)->_delegate && _delegable(self)->GetMetaMethod(this,MT_NEWSLOT,closure)) {
                    Push(self);Push(key);Push(val);
                    if(!CallMetaMethod(closure,3,res)) {
                        return false;
                    }
                    rawcall = false;
                }
                else {
                    rawcall = true;
                }
            }
        }
        if(rawcall) _table(self)->NewSlot(key,val); //cannot fail

        break;}
    case OT_INSTANCE: {
        SQObjectPtr res;
        SQObjectPtr closure;
        if(_delegable(self)->_delegate && _delegable(self)->GetMetaMethod(this,MT_NEWSLOT,closure)) {
            Push(self);Push(key);Push(val);
            if(!CallMetaMethod(closure,3,res)) {
                return false;
            }
            break;
        }
        Raise_Error(_SC("class instances do not support the new slot operator"));
        return false;
        break;}
    case OT_CLASS:
        if(!_class(self)->NewSlot(_ss(this),key,val,bstatic)) {
            if(_class(self)->_locked) {
                Raise_Error(_SC("trying to modify a class that has already been instantiated"));
                return false;
            }
            else {
                SQObjectPtr oval = PrintObjVal(key);
                Raise_Error(_SC("the property '%s' already exists"),_stringval(oval));
                return false;
            }
        }
        break;
    default:
        Raise_Error(_SC("indexing %s with %s"),GetTypeName(self),GetTypeName(key));
        return false;
        break;
    }
    return true;
}



bool SQVM::DeleteSlot(const SQObjectPtr &self,const SQObjectPtr &key,SQObjectPtr &res)
{
    switch(sq_type(self)) {
    case OT_TABLE:
    case OT_INSTANCE:
    case OT_USERDATA: {
        SQObjectPtr t;
        //bool handled = false;
        SQObjectPtr closure;
        if(_delegable(self)->_delegate && _delegable(self)->GetMetaMethod(this,MT_DELSLOT,closure)) {
            Push(self);Push(key);
            return CallMetaMethod(closure,2,res);
        }
        else {
            if(sq_type(self) == OT_TABLE) {
                if(_table(self)->Get(key,t)) {
                    _table(self)->Remove(key);
                }
                else {
                    Raise_IdxError((const SQObject &)key);
                    return false;
                }
            }
            else {
                Raise_Error(_SC("cannot delete a slot from %s"),GetTypeName(self));
                return false;
            }
        }
        res = t;
                }
        break;
    default:
        Raise_Error(_SC("attempt to delete a slot from a %s"),GetTypeName(self));
        return false;
    }
    return true;
}

bool SQVM::Call(SQObjectPtr &closure, SQInteger nparams, SQInteger stackbase, SQObjectPtr &outres, SQBool raiseerror) {
    switch(sq_type(closure)) {
    case OT_CLOSURE:
        return Execute(closure, nparams, stackbase, outres, raiseerror);
    case OT_NATIVECLOSURE:{
        bool dummy;
        return CallNative(_nativeclosure(closure), nparams, stackbase, outres, -1, dummy, dummy);
    }
    case OT_CLASS: {
        SQObjectPtr constr;
        SQObjectPtr temp;
        CreateClassInstance(_class(closure), outres, constr);
        SQObjectType ctype = sq_type(constr);
        if (ctype == OT_NATIVECLOSURE || ctype == OT_CLOSURE) {
            _stack[stackbase] = outres;
            return Call(constr,nparams,stackbase,temp,raiseerror);
        }
        return true;
    }
    default:
        break;
    }

    Raise_Error(_SC("attempt to call '%s'"), GetTypeName(closure));
    return false;
}

bool SQVM::CallMetaMethod(SQObjectPtr & closure, SQInteger nparams, SQObjectPtr & outres) {
    n_metamethod_calls++;

    bool const status = Call(closure, nparams, stack_top - nparams, outres, SQFalse);

    n_metamethod_calls--;
    Pop(nparams);

    return status;
}

void SQVM::FindOuter(SQObjectPtr & target, SQObjectPtr * stackindex) {
    SQOuter ** pp = &_openouters;
    SQOuter * p;
    SQOuter * otr;

    while ((p = *pp) != NULL && p->_valptr >= stackindex) {
        if (p->_valptr == stackindex) {
            target = SQObjectPtr(p);
            return;
        }
        pp = &p->_next;
    }

    otr = SQOuter::Create(this->_sharedstate, stackindex);
    otr->_next = *pp;
    otr->_idx  = (stackindex - _stack._vals);
    otr->IncreaseRefCount();
    *pp = otr;
    target = SQObjectPtr(otr);
}

bool SQVM::EnterFrame(SQInteger newbase, SQInteger newtop, bool tailcall) {
    if (!tailcall) {
        if (call_stack_size == call_stack.size()) {
            GrowCallStack();
        }

        ci = &call_stack[call_stack_size++];
        ci->_prevstkbase = (SQInt32)(newbase - _stackbase);
        ci->_prevtop = (SQInt32)(stack_top - _stackbase);
        ci->_etraps = 0;
        ci->_ncalls = 1;
        ci->_generator = NULL;
        ci->_root = SQFalse;
    }
    else {
        ci->_ncalls++;
    }

    _stackbase = newbase;
    stack_top = newtop;
    if(newtop + MIN_STACK_OVERHEAD > (SQInteger)_stack.size()) {
        if (n_metamethod_calls) {
            // why?
            Raise_Error(_SC("stack overflow, cannot resize stack while in a metamethod"));
            return false;
        }
        _stack.resize(newtop + (MIN_STACK_OVERHEAD << 2));
        RelocateOuters();
    }
    return true;
}

void SQVM::LeaveFrame() {
    SQInteger last_top = stack_top;
    SQInteger last_stackbase = _stackbase;
    size_t css = --call_stack_size;

    /* First clean out the call stack frame */
    ci->_closure.Null();
    _stackbase -= ci->_prevstkbase;
    stack_top = _stackbase + ci->_prevtop;
    ci = (css)
        ? &call_stack[css - 1]
        : nullptr;

    if (_openouters) {
        CloseOuters(&(_stack._vals[last_stackbase]));
    }

    while (last_top >= stack_top) {
        _stack._vals[last_top--].Null();
    }
}

void SQVM::RelocateOuters() {
    SQOuter *p = _openouters;
    while (p) {
        p->_valptr = _stack._vals + p->_idx;
        p = p->_next;
    }
}

void SQVM::CloseOuters(SQObjectPtr *stackindex) {
  SQOuter *p;
  while ((p = _openouters) != NULL && p->_valptr >= stackindex) {
    p->_value = *(p->_valptr);
    p->_valptr = &p->_value;
    _openouters = p->_next;
    p->DecreaseRefCount();
    p = nullptr;
  }
}

void SQVM::Remove(SQInteger n) {
    n = (n >= 0)?n + _stackbase - 1:stack_top + n;
    for(SQInteger i = n; i < stack_top; i++){
        _stack[i] = _stack[i+1];
    }
    _stack[stack_top].Null();
    stack_top--;
}

void SQVM::PushNull() {
    _stack[stack_top].Null();
    stack_top += 1;
}

void SQVM::Push(SQObjectPtr const & o) {
    _stack[stack_top] = o;
    stack_top += 1;
}

SQObjectPtr & SQVM::Top() {
    return _stack[stack_top - 1];
}

SQObjectPtr & SQVM::GetUp(SQInteger n) {
    return _stack[stack_top + n];
}

SQObjectPtr & SQVM::GetAt(SQInteger n) {
    return _stack[n];
}

#ifdef _DEBUG_DUMP
void SQVM::dumpstack(SQInteger stackbase,bool dumpall)
{
    SQInteger size=dumpall?_stack.size():stack_top;
    SQInteger n=0;
    scprintf(_SC("\n>>>>stack dump<<<<\n"));
    CallInfo & ci = call_stack[call_stack_size - 1];
    scprintf(_SC("IP: %p\n"),ci._ip);
    scprintf(_SC("prev stack base: %d\n"),ci._prevstkbase);
    scprintf(_SC("prev top: %d\n"),ci._prevtop);
    for(SQInteger i=0;i<size;i++){
        SQObjectPtr &obj=_stack[i];
        if(stackbase==i)scprintf(_SC(">"));else scprintf(_SC(" "));
        scprintf(_SC("[") _PRINT_INT_FMT _SC("]:"),n);
        switch(sq_type(obj)){
        case OT_FLOAT:          scprintf(_SC("FLOAT %.3f"),_float(obj));break;
        case OT_INTEGER:        scprintf(_SC("INTEGER ") _PRINT_INT_FMT,_integer(obj));break;
        case OT_BOOL:           scprintf(_SC("BOOL %s"),_integer(obj)?_SC("true"):_SC("false"));break;
        case OT_STRING:         scprintf(_SC("STRING %s"),_stringval(obj));break;
        case OT_NULL:           scprintf(_SC("NULL"));  break;
        case OT_TABLE:          scprintf(_SC("TABLE %p[%p]"),_table(obj),_table(obj)->_delegate);break;
        case OT_ARRAY:          scprintf(_SC("ARRAY %p"),_array(obj));break;
        case OT_CLOSURE:        scprintf(_SC("CLOSURE [%p]"),_closure(obj));break;
        case OT_NATIVECLOSURE:  scprintf(_SC("NATIVECLOSURE"));break;
        case OT_USERDATA:       scprintf(_SC("USERDATA %p[%p]"),_userdataval(obj),_userdata(obj)->_delegate);break;
        case OT_GENERATOR:      scprintf(_SC("GENERATOR %p"),_generator(obj));break;
        case OT_THREAD:         scprintf(_SC("THREAD [%p]"),_thread(obj));break;
        case OT_USERPOINTER:    scprintf(_SC("USERPOINTER %p"),_userpointer(obj));break;
        case OT_CLASS:          scprintf(_SC("CLASS %p"),_class(obj));break;
        case OT_INSTANCE:       scprintf(_SC("INSTANCE %p"),_instance(obj));break;
        case OT_WEAKREF:        scprintf(_SC("WEAKREF %p"),_weakref(obj));break;
        default:
            assert(0);
            break;
        };
        scprintf(_SC("\n"));
        ++n;
    }
}
#endif

#ifndef NO_GARBAGE_COLLECTOR
void SQVM::Mark(SQCollectable **chain)
{
    START_MARK()
        GC::MarkObject(_lasterror,chain);
        GC::MarkObject(_errorhandler,chain);
        GC::MarkObject(_debughook_closure,chain);
        GC::MarkObject(_roottable, chain);
        GC::MarkObject(temp_reg, chain);
        for(size_t i = 0; i < _stack.size(); i++) GC::MarkObject(_stack[i], chain);
        for(size_t k = 0; k < call_stack_size; k++) GC::MarkObject(call_stack[k]._closure, chain);
    END_MARK()
}
#endif