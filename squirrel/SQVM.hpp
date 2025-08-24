#pragma once

#include "SQCollectable.hpp"
#include "sqopcodes.h"
#include "sqobject.h"

#define MIN_STACK_OVERHEAD 15

#define SQ_SUSPEND_FLAG -666
#define SQ_TAILCALL_FLAG -777
#define DONT_FALL_BACK 666

#define GET_FLAG_RAW                0x00000001
#define GET_FLAG_DO_NOT_RAISE_ERROR 0x00000002

struct SQExceptionTrap {
    SQInteger _stackbase;
    SQInteger _stacksize;
    SQInstruction *_ip;
    SQInteger _extarget;

    SQExceptionTrap()
        : _stackbase(0)
        , _stacksize(0)
        , _ip(nullptr)
        , _extarget(0)
    {}

    SQExceptionTrap(SQInteger ss, SQInteger stackbase, SQInstruction * ip, SQInteger ex_target)
        : _stackbase(stackbase)
        , _stacksize(ss)
        , _ip(ip)
        , _extarget(ex_target)
    {}
};

struct SQVM : public CHAINABLE_OBJ {
    struct CallInfo {
        SQInstruction * _ip;
        SQObjectPtr * _literals;
        SQObjectPtr _closure;
        SQGenerator * _generator;
        SQInt32 _etraps;
        SQInt32 _prevstkbase;
        SQInt32 _prevtop;
        SQInt32 _target;
        SQInt32 _ncalls;
        SQBool _root;
    };

    struct SuspendedState {
        bool suspended;
        bool root;
        uint32_t target;
        uint32_t traps;

        SuspendedState()
            : suspended(false)
            , root(false)
            , target(0)
            , traps(0)
        {}
    };
public:
    SQSharedState * _sharedstate;
    SQRELEASEHOOK release_hook;
    SQUserPointer release_hook_user_pointer;

    sqvector<SQObjectPtr> _stack;

    size_t call_stack_size;
    sqvector<CallInfo> call_stack;

    SQInteger stack_top;
    SQInteger _stackbase;
    SQObjectPtr _roottable;
    SQObjectPtr _lasterror;
    SQObjectPtr _errorhandler;

    bool _debughook;
    SQDEBUGHOOK _debughook_native;
    SQObjectPtr _debughook_closure;

    sqvector<SQExceptionTrap> _etraps;
    CallInfo *ci;

    size_t n_metamethod_calls;

    //suspend infos
    bool is_suspended;
    SQInteger _suspended_target;
private:
    SQOuter * _openouters;
    size_t n_native_calls;
    bool _suspended_root;
    SQInteger _suspended_traps;
    SQObjectPtr temp_reg;
public:
    enum ExecutionType {
        ET_CALL,
        ET_RESUME_GENERATOR,
        ET_RESUME_VM,
        ET_RESUME_THROW_VM
    };

    static bool IsEqual(SQObjectPtr const & o1, SQObjectPtr const & o2, bool & res);
    static bool IsFalse(SQObjectPtr &o);

    SQVM(SQSharedState * ss, SQVM * friend_vm, size_t stack_size);

    ~SQVM();

    void Release() {
        this->~SQVM();
        sq_vm_free(this, sizeof(*this));
    }

    bool Execute(SQObjectPtr & func, SQInteger nargs, SQInteger stackbase, SQObjectPtr & outres, SQBool raiseerror, ExecutionType et = ET_CALL);
	bool TailCall(SQClosure *closure, SQInteger firstparam, SQInteger nparams);
    bool Call(SQObjectPtr &closure, SQInteger nparams, SQInteger stackbase, SQObjectPtr &outres,SQBool raiseerror);
    SQRESULT Suspend();
    void CallDebugHook(SQInteger type,SQInteger forcedline=0);
    bool Get(const SQObjectPtr &self, const SQObjectPtr &key, SQObjectPtr &dest, SQUnsignedInteger getflags, SQInteger selfidx);
    bool Set(const SQObjectPtr &self, const SQObjectPtr &key, const SQObjectPtr &val, SQInteger selfidx);

    bool NewSlot(const SQObjectPtr &self, const SQObjectPtr &key, const SQObjectPtr &val,bool bstatic);
    bool NewSlotA(const SQObjectPtr &self,const SQObjectPtr &key,const SQObjectPtr &val,const SQObjectPtr &attrs,bool bstatic,bool raw);
    bool DeleteSlot(const SQObjectPtr &self, const SQObjectPtr &key, SQObjectPtr &res);
    bool Clone(const SQObjectPtr &self, SQObjectPtr &target);
    bool ObjCmp(const SQObjectPtr &o1, const SQObjectPtr &o2,SQInteger &res);
    bool StringCat(const SQObjectPtr &str, const SQObjectPtr &obj, SQObjectPtr &dest);
    bool ToString(const SQObjectPtr &o,SQObjectPtr &res);
    SQString *PrintObjVal(const SQObjectPtr &o);

    void Raise_Error(const SQChar *s, ...);
    void Raise_Error(const SQObjectPtr &desc);
    void Raise_IdxError(const SQObjectPtr &o);
    void Raise_CompareError(const SQObject &o1, const SQObject &o2);
    void Raise_ParamTypeError(SQInteger nparam,SQInteger typemask,SQInteger type);

    bool TypeOf(const SQObjectPtr &obj1, SQObjectPtr &dest);
    bool FOREACH_OP(SQObjectPtr &o1,SQObjectPtr &o2,SQObjectPtr &o3,SQObjectPtr &o4,SQInteger arg_2,int exitpos,int &jump);

#ifndef NO_GARBAGE_COLLECTOR
    void Mark(SQCollectable **chain);
    SQObjectType GetType() {return OT_THREAD;}
#endif
    void Finalize();

    bool EnterFrame(SQInteger newbase, SQInteger newtop, bool tailcall);

    void Remove(SQInteger n);

    inline void Pop() {
        _stack[--stack_top].Null();
    }

    inline void Pop(SQInteger n) {
        for (SQInteger i = 0; i < n; i++) {
            _stack[--stack_top].Null();
        }
    }

    void Push(SQObjectPtr const & o);
    void PushNull();
    SQObjectPtr &Top();
    SQObjectPtr &GetUp(SQInteger n);
    SQObjectPtr &GetAt(SQInteger n);
private:
    void GrowCallStack();
    bool CallNative(SQNativeClosure * nclosure, SQInteger nargs, SQInteger newbase, SQObjectPtr & retval, SQInt32 target, bool & suspend, bool & tailcall);
    bool StartCall(SQClosure * closure, SQInteger target, SQInteger nargs, SQInteger stackbase, bool tailcall);
    void CallErrorHandler(SQObjectPtr &e);
    SQInteger FallBackGet(const SQObjectPtr &self,const SQObjectPtr &key,SQObjectPtr &dest);
    bool InvokeDefaultDelegate(const SQObjectPtr &self,const SQObjectPtr &key,SQObjectPtr &dest);
    SQInteger FallBackSet(const SQObjectPtr &self,const SQObjectPtr &key,const SQObjectPtr &val);
    void FindOuter(SQObjectPtr &target, SQObjectPtr *stackindex);
    void RelocateOuters();
    void CloseOuters(SQObjectPtr *stackindex);
    bool CallMetaMethod(SQObjectPtr &closure, SQInteger nparams, SQObjectPtr &outres);
    bool ArithMetaMethod(uint8_t op, const SQObjectPtr &o1, const SQObjectPtr &o2, SQObjectPtr &dest);
    bool Return(SQInteger _arg0, SQInteger _arg1, SQObjectPtr &retval);
    bool ARITH_OP(uint8_t op,SQObjectPtr &trg,const SQObjectPtr &o1,const SQObjectPtr &o2);
    bool _ARITH_(uint8_t op, SQObjectPtr & out, SQObjectPtr const & o1, SQObjectPtr const & o2);
    bool BW_OP(uint8_t op,SQObjectPtr &trg,const SQObjectPtr &o1,const SQObjectPtr &o2);
    bool NEG_OP(SQObjectPtr &trg,const SQObjectPtr &o1);
    bool CMP_OP(CmpOP op, const SQObjectPtr &o1,const SQObjectPtr &o2,SQObjectPtr &res);
    bool CLOSURE_OP(SQObjectPtr &target, SQFunctionProto *func, SQInteger boundtarget);
    bool CLASS_OP(SQObjectPtr &target,SQInteger base,SQInteger attrs);
    bool PLOCAL_INC(uint8_t op,SQObjectPtr &target, SQObjectPtr &a, SQObjectPtr &incr);
    bool DerefInc(uint8_t op,SQObjectPtr &target, SQObjectPtr &self, SQObjectPtr &key, SQObjectPtr &incr, bool postfix,SQInteger arg0);
#ifdef _DEBUG_DUMP
    void dumpstack(SQInteger stackbase=-1, bool dumpall = false);
#endif
    void LeaveFrame();
};

inline SQObjectPtr & stack_get(HSQUIRRELVM v, SQInteger idx) {
    assert(idx != 0);

    return idx > 0
        ? v->GetAt(v->_stackbase + idx - 1)
        : v->GetUp(idx);
}

#define _ss(_vm_) (_vm_)->_sharedstate

#ifndef NO_GARBAGE_COLLECTOR
#define _opt_ss(_vm_) (_vm_)->_sharedstate
#else
#define _opt_ss(_vm_) NULL
#endif
