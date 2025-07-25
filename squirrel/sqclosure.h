#pragma once

struct SQFunctionProto;
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
        : _env(nullptr)
        , _root(root)
        , _base(nullptr)
        , _function(func)
        , _outervalues((SQObjectPtr *)sq_vm_malloc(func->_noutervalues * sizeof(SQObjectPtr)))
        , _defaultparams((SQObjectPtr *)sq_vm_malloc(func->_ndefaultparams * sizeof(SQObjectPtr)))
    {
        __ObjAddRef(_function);
        __ObjAddRef(_root);

        INIT_CHAIN();
        ADD_TO_CHAIN(&this->_sharedstate->_gc_chain, this);

        for (SQInteger i = 0; i < func->_noutervalues; i++) {
            new (&_outervalues[i]) SQObjectPtr();
        }
        for (SQInteger i = 0; i < func->_ndefaultparams; i++) {
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

    void Release(){
        for (SQInteger i = 0; i < _function->_noutervalues; i++) {
            _outervalues[i].~SQObjectPtr();
        }
        sq_vm_free(_outervalues, _function->_noutervalues * sizeof(SQObjectPtr));

        for (SQInteger i = 0; i < _function->_ndefaultparams; i++) {
            _defaultparams[i].~SQObjectPtr();
        }
        sq_vm_free(_defaultparams, _function->_ndefaultparams * sizeof(SQObjectPtr));

        __ObjRelease(_function);

        this->~SQClosure();
        sq_vm_free(this, sizeof(SQClosure));
    }

    void SetRoot(SQWeakRef * r) {
        __ObjRelease(_root);
        _root = r;
        __ObjAddRef(_root);
    }

    SQClosure * Clone() {
        SQFunctionProto * f = _function;
        SQClosure * ret = SQClosure::Create(_opt_ss(this),f,_root);
        ret->_env = _env;
        if(ret->_env) __ObjAddRef(ret->_env);
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

//////////////////////////////////////////////
struct SQOuter : public CHAINABLE_OBJ
{

private:
    SQOuter(SQSharedState *ss, SQObjectPtr *outer){_valptr = outer; _next = NULL; INIT_CHAIN(); ADD_TO_CHAIN(&_ss(this)->_gc_chain,this); }

public:
    static SQOuter *Create(SQSharedState *ss, SQObjectPtr *outer)
    {
        SQOuter *nc  = (SQOuter*)sq_vm_malloc(sizeof(SQOuter));
        new (nc) SQOuter(ss, outer);
        return nc;
    }
    ~SQOuter() { REMOVE_FROM_CHAIN(&_ss(this)->_gc_chain,this); }

    void Release()
    {
        this->~SQOuter();
        sq_vm_free(this,sizeof(SQOuter));
    }

#ifndef NO_GARBAGE_COLLECTOR
    void Mark(SQCollectable **chain);
    void Finalize() { _value.Null(); }
    SQObjectType GetType() {return OT_OUTER;}
#endif

    SQObjectPtr *_valptr;  /* pointer to value on stack, or _value below */
    SQInteger    _idx;     /* idx in stack array, for relocation */
    SQObjectPtr  _value;   /* value of outer after stack frame is closed */
    SQOuter     *_next;    /* pointer to next outer when frame is open   */
};

//////////////////////////////////////////////
struct SQGenerator : public CHAINABLE_OBJ
{
    enum SQGeneratorState{eRunning,eSuspended,eDead};
private:
    SQGenerator(SQSharedState *ss,SQClosure *closure){_closure=closure;_state=eRunning;_ci._generator=NULL;INIT_CHAIN();ADD_TO_CHAIN(&_ss(this)->_gc_chain,this);}
public:
    static SQGenerator *Create(SQSharedState *ss,SQClosure *closure){
        SQGenerator *nc=(SQGenerator*)sq_vm_malloc(sizeof(SQGenerator));
        new (nc) SQGenerator(ss,closure);
        return nc;
    }
    ~SQGenerator()
    {
        REMOVE_FROM_CHAIN(&_ss(this)->_gc_chain,this);
    }
    void Kill(){
        _state=eDead;
        _stack.resize(0);
        _closure.Null();}
    void Release(){
        sq_delete(this,SQGenerator);
    }

    bool Yield(SQVM *v,SQInteger target);
    bool Resume(SQVM *v,SQObjectPtr &dest);
#ifndef NO_GARBAGE_COLLECTOR
    void Mark(SQCollectable **chain);
    void Finalize(){_stack.resize(0);_closure.Null();}
    SQObjectType GetType() {return OT_GENERATOR;}
#endif
    SQObjectPtr _closure;
    SQObjectPtrVec _stack;
    SQVM::CallInfo _ci;
    ExceptionsTraps _etraps;
    SQGeneratorState _state;
};

#define _CALC_NATVIVECLOSURE_SIZE(noutervalues) (sizeof(SQNativeClosure) + (noutervalues*sizeof(SQObjectPtr)))

struct SQNativeClosure : public CHAINABLE_OBJ
{
private:
    SQNativeClosure(SQSharedState *ss,SQFUNCTION func){_function=func;INIT_CHAIN();ADD_TO_CHAIN(&_ss(this)->_gc_chain,this); _env = NULL;}
public:
    static SQNativeClosure *Create(SQSharedState *ss,SQFUNCTION func,SQInteger nouters)
    {
        SQInteger size = _CALC_NATVIVECLOSURE_SIZE(nouters);
        SQNativeClosure *nc=(SQNativeClosure*)sq_vm_malloc(size);
        new (nc) SQNativeClosure(ss,func);
        nc->_outervalues = (SQObjectPtr *)(nc + 1);
        nc->_noutervalues = nouters;
        _CONSTRUCT_VECTOR(SQObjectPtr,nc->_noutervalues,nc->_outervalues);
        return nc;
    }
    SQNativeClosure *Clone()
    {
        SQNativeClosure * ret = SQNativeClosure::Create(_opt_ss(this),_function,_noutervalues);
        ret->_env = _env;
        if(ret->_env) __ObjAddRef(ret->_env);
        ret->_name = _name;
        _COPY_VECTOR(ret->_outervalues,_outervalues,_noutervalues);
        ret->_typecheck.copy(_typecheck);
        ret->_nparamscheck = _nparamscheck;
        return ret;
    }
    ~SQNativeClosure()
    {
        __ObjRelease(_env);
        _env = nullptr;
        REMOVE_FROM_CHAIN(&_ss(this)->_gc_chain,this);
    }
    void Release(){
        SQInteger size = _CALC_NATVIVECLOSURE_SIZE(_noutervalues);
        _DESTRUCT_VECTOR(SQObjectPtr,_noutervalues,_outervalues);
        this->~SQNativeClosure();
        sq_free(this,size);
    }

#ifndef NO_GARBAGE_COLLECTOR
    void Mark(SQCollectable **chain);
    void Finalize() { _NULL_SQOBJECT_VECTOR(_outervalues,_noutervalues); }
    SQObjectType GetType() {return OT_NATIVECLOSURE;}
#endif
    SQInteger _nparamscheck;
    SQIntVec _typecheck;
    SQObjectPtr *_outervalues;
    SQUnsignedInteger _noutervalues;
    SQWeakRef *_env;
    SQFUNCTION _function;
    SQObjectPtr _name;
};
