#pragma once

#include "sqvector.hpp"
#include "sqobject.h"

#include "GC.hpp"
#include "RefTable.hpp"

struct SQString;
struct SQTable;
//max number of character for a printed number
#define NUMBER_MAX_CHAR 50

struct SQObjectPtr;

struct SQSharedState {
public:
    GC gc;

    SQRELEASEHOOK _releasehook;
    SQUserPointer _foreignptr;
    SQObjectPtr _constructoridx;
private:
    char *_scratchpad;
    size_t _scratchpadsize;
public:
    SQSharedState();
    ~SQSharedState();

    char * GetScratchPad(size_t size);
    SQInteger GetMetaMethodIdxByName(const SQObjectPtr &name);

#ifndef NO_GARBAGE_COLLECTOR
    SQInteger CollectGarbage();
    void RunMark(SQCollectable ** tchain);
    void ResurrectUnreachable(SQVM * vm);
#endif

    sqvector<SQObjectPtr> _metamethods;
    SQObjectPtr _metamethodsmap;
    sqvector<SQObjectPtr> _systemstrings;
    RefTable _refs_table;
    SQObjectPtr _registry;
    SQObjectPtr _consts;

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
};

#define _sp(s) (_sharedstate->GetScratchPad(s))
#define _spval (_sharedstate->GetScratchPad(0))

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
