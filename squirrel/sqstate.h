/*  see copyright notice in squirrel.h */
#ifndef _SQSTATE_H_
#define _SQSTATE_H_

#include "squtils.h"
#include "sqobject.h"
#include "strtab.h"

struct SQString;
struct SQTable;
//max number of character for a printed number
#define NUMBER_MAX_CHAR 50

struct RefTable {
    struct RefNode {
        SQObjectPtr obj;
        SQUnsignedInteger refs;
        struct RefNode *next;
    };
    RefTable();
    ~RefTable();
    void AddRef(SQObject &obj);
    SQBool Release(SQObject &obj);
    SQUnsignedInteger GetRefCount(SQObject &obj);
#ifndef NO_GARBAGE_COLLECTOR
    void Mark(SQCollectable **chain);
#endif
    void Finalize();
private:
    RefNode *Get(SQObject &obj,SQHash &mainpos,RefNode **prev,bool add);
    RefNode *Add(SQHash mainpos,SQObject &obj);
    void Resize(SQUnsignedInteger size);
    void AllocNodes(SQUnsignedInteger size);
    SQUnsignedInteger _numofslots;
    SQUnsignedInteger _slotused;
    RefNode *_nodes;
    RefNode *_freelist;
    RefNode **_buckets;
};

struct SQObjectPtr;

struct SQSharedState {
private:
    void Init();
public:
    SQSharedState()
        : _metamethods()
        , _systemstrings()
        , _stringtable(this)
#ifndef NO_GARBAGE_COLLECTOR
        , _gc_chain(nullptr)
#endif
        , _compilererrorhandler(nullptr)
        , _printfunc(nullptr)
        , _errorfunc(nullptr)
        , _debuginfo(false)
        , _notifyallexceptions(false)
        , _foreignptr(nullptr)
        , _releasehook(nullptr)
        , _scratchpad(nullptr)
        , _scratchpadsize(0)
    {
        Init();
    }

    ~SQSharedState();

    SQChar* GetScratchPad(SQInteger size);
    SQInteger GetMetaMethodIdxByName(const SQObjectPtr &name);

#ifndef NO_GARBAGE_COLLECTOR
    SQInteger CollectGarbage();
    void RunMark(SQCollectable **tchain);
    void ResurrectUnreachable(SQVM * vm);
    static void MarkObject(SQObjectPtr &o,SQCollectable **chain);
#endif

    sqvector<SQObjectPtr> _metamethods;
    SQObjectPtr _metamethodsmap;
    sqvector<SQObjectPtr> _systemstrings;
    SQStringTable _stringtable;
    RefTable _refs_table;
    SQObjectPtr _registry;
    SQObjectPtr _consts;
    SQObjectPtr _constructoridx;

#ifndef NO_GARBAGE_COLLECTOR
    SQCollectable *_gc_chain;
#endif

    SQObjectPtr _root_vm;
    SQObjectPtr _table_default_delegate;
    static const SQRegFunction _table_default_delegate_funcz[];
    SQObjectPtr _array_default_delegate;
    static const SQRegFunction _array_default_delegate_funcz[];
    SQObjectPtr _string_default_delegate;
    static const SQRegFunction _string_default_delegate_funcz[];
    SQObjectPtr _number_default_delegate;
    static const SQRegFunction _number_default_delegate_funcz[];
    SQObjectPtr _generator_default_delegate;
    static const SQRegFunction _generator_default_delegate_funcz[];
    SQObjectPtr _closure_default_delegate;
    static const SQRegFunction _closure_default_delegate_funcz[];
    SQObjectPtr _thread_default_delegate;
    static const SQRegFunction _thread_default_delegate_funcz[];
    SQObjectPtr _class_default_delegate;
    static const SQRegFunction _class_default_delegate_funcz[];
    SQObjectPtr _instance_default_delegate;
    static const SQRegFunction _instance_default_delegate_funcz[];
    SQObjectPtr _weakref_default_delegate;
    static const SQRegFunction _weakref_default_delegate_funcz[];

    SQCOMPILERERROR _compilererrorhandler;
    SQPRINTFUNCTION _printfunc;
    SQPRINTFUNCTION _errorfunc;
    bool _debuginfo;
    bool _notifyallexceptions;
    SQUserPointer _foreignptr;
    SQRELEASEHOOK _releasehook;
private:
    SQChar *_scratchpad;
    SQInteger _scratchpadsize;
};

#define _sp(s) (_sharedstate->GetScratchPad(s))
#define _spval (_sharedstate->GetScratchPad(-1))

#define _table_ddel     _table(_sharedstate->_table_default_delegate)
#define _array_ddel     _table(_sharedstate->_array_default_delegate)
#define _string_ddel    _table(_sharedstate->_string_default_delegate)
#define _number_ddel    _table(_sharedstate->_number_default_delegate)
#define _generator_ddel _table(_sharedstate->_generator_default_delegate)
#define _closure_ddel   _table(_sharedstate->_closure_default_delegate)
#define _thread_ddel    _table(_sharedstate->_thread_default_delegate)
#define _class_ddel     _table(_sharedstate->_class_default_delegate)
#define _instance_ddel  _table(_sharedstate->_instance_default_delegate)
#define _weakref_ddel   _table(_sharedstate->_weakref_default_delegate)

bool CompileTypemask(sqvector<SQInteger> &res,const SQChar *typemask);


#endif //_SQSTATE_H_
