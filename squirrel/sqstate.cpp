#include "sqstate.h"

#include <algorithm>

#include "GC.hpp"
#include "SQArray.hpp"
#include "SQClass.hpp"
#include "SQFunctionProto.hpp"
#include "SQGenerator.hpp"
#include "SQInstance.hpp"
#include "SQString.hpp"
#include "SQUserData.hpp"
#include "SQVM.hpp"
#include "SQNativeClosure.hpp"
#include "SQClosure.hpp"
#include "SQOuter.hpp"

bool CompileTypemask(sqvector<SQInteger> & res, SQChar const * typemask) {
    SQInteger mask = 0;

    while (*typemask != 0) {
        switch (*typemask) {
        case 'o': mask |= _RT_NULL; break;
        case 'i': mask |= _RT_INTEGER; break;
        case 'f': mask |= _RT_FLOAT; break;
        case 'n': mask |= (_RT_FLOAT | _RT_INTEGER); break;
        case 's': mask |= _RT_STRING; break;
        case 't': mask |= _RT_TABLE; break;
        case 'a': mask |= _RT_ARRAY; break;
        case 'u': mask |= _RT_USERDATA; break;
        case 'c': mask |= (_RT_CLOSURE | _RT_NATIVECLOSURE); break;
        case 'b': mask |= _RT_BOOL; break;
        case 'g': mask |= _RT_GENERATOR; break;
        case 'p': mask |= _RT_USERPOINTER; break;
        case 'v': mask |= _RT_THREAD; break;
        case 'x': mask |= _RT_INSTANCE; break;
        case 'y': mask |= _RT_CLASS; break;
        case 'r': mask |= _RT_WEAKREF; break;
        case '.':
            typemask++;
            res.push_back(-1);
            mask = 0;
            continue;
        case ' ':
            typemask++;
            continue; // ignores spaces
        default:
            return false;
        }

        typemask++;
        if (*typemask == '|') {
            typemask++;
            if(*typemask == 0) {
                return false;
            }
            continue;
        }

        res.push_back(mask);
        mask = 0;
    }

    return true;
}

static void new_metamethod(SQSharedState * state, char const * name) {
    state->_metamethods.push_back(state->gc.AddString(name, strlen(name)));

    _table(state->_metamethodsmap)->NewSlot(
        state->_metamethods.back(),
        SQInteger(state->_metamethods.size() - 1)
    );
}

static SQTable *CreateDefaultDelegate(SQSharedState *ss, const SQRegFunction *funcz) {
    SQInteger i=0;
    SQTable *t=SQTable::Create(ss,0);
    while(funcz[i].name!=0){
        SQNativeClosure *nc = SQNativeClosure::Create(ss,funcz[i].f,0);
        nc->_nparamscheck = funcz[i].nparamscheck;
        nc->_name = ss->gc.AddString(funcz[i].name, strlen(funcz[i].name));
        if (funcz[i].typemask && !CompileTypemask(nc->_typecheck, funcz[i].typemask)) {
            return NULL;
        }
        t->NewSlot(ss->gc.AddString(funcz[i].name, strlen(funcz[i].name)), nc);
        i++;
    }
    return t;
}

extern "C" void test1(void);

SQSharedState::SQSharedState()
    : gc()
    , _releasehook(nullptr)
    , _foreignptr(nullptr)
    , _constructoridx()
    , _scratchpad(nullptr)
    , _scratchpadsize(0)

    , _metamethods()
    , _systemstrings()
    , _compilererrorhandler(nullptr)
    , _printfunc(nullptr)
    , _errorfunc(nullptr)
    , _debuginfo(false)
    , _notifyallexceptions(false)
{
    // test1();

    _metamethodsmap = SQTable::Create(this, MT_LAST - 1);

    _systemstrings.push_back(gc.AddString("null", strlen("null")));
    _systemstrings.push_back(gc.AddString("table", strlen("table")));
    _systemstrings.push_back(gc.AddString("array", strlen("array")));
    _systemstrings.push_back(gc.AddString("closure", strlen("closure")));
    _systemstrings.push_back(gc.AddString("string", strlen("string")));
    _systemstrings.push_back(gc.AddString("userdata", strlen("userdata")));
    _systemstrings.push_back(gc.AddString("integer", strlen("integer")));
    _systemstrings.push_back(gc.AddString("float", strlen("float")));
    _systemstrings.push_back(gc.AddString("userpointer", strlen("userpointer")));
    _systemstrings.push_back(gc.AddString("function", strlen("function")));
    _systemstrings.push_back(gc.AddString("generator", strlen("generator")));
    _systemstrings.push_back(gc.AddString("thread", strlen("thread")));
    _systemstrings.push_back(gc.AddString("class", strlen("class")));
    _systemstrings.push_back(gc.AddString("instance", strlen("instance")));
    _systemstrings.push_back(gc.AddString("bool", strlen("bool")));

    new_metamethod(this, MM_ADD);
    new_metamethod(this, MM_SUB);
    new_metamethod(this, MM_MUL);
    new_metamethod(this, MM_DIV);
    new_metamethod(this, MM_UNM);
    new_metamethod(this, MM_MODULO);
    new_metamethod(this, MM_SET);
    new_metamethod(this, MM_GET);
    new_metamethod(this, MM_TYPEOF);
    new_metamethod(this, MM_NEXTI);
    new_metamethod(this, MM_CMP);
    new_metamethod(this, MM_CALL);
    new_metamethod(this, MM_CLONED);
    new_metamethod(this, MM_NEWSLOT);
    new_metamethod(this, MM_DELSLOT);
    new_metamethod(this, MM_TOSTRING);
    new_metamethod(this, MM_NEWMEMBER);
    new_metamethod(this, MM_INHERITED);

    _constructoridx = gc.AddString("constructor", strlen("constructor"));
    _registry = SQTable::Create(this, 0);
    _consts = SQTable::Create(this, 0);

    _table_default_delegate = CreateDefaultDelegate(this,_table_default_delegate_funcz);
    _array_default_delegate = CreateDefaultDelegate(this,_array_default_delegate_funcz);
    _string_default_delegate = CreateDefaultDelegate(this,_string_default_delegate_funcz);
    _number_default_delegate = CreateDefaultDelegate(this,_number_default_delegate_funcz);
    _closure_default_delegate = CreateDefaultDelegate(this,_closure_default_delegate_funcz);
    _generator_default_delegate = CreateDefaultDelegate(this,_generator_default_delegate_funcz);
    _thread_default_delegate = CreateDefaultDelegate(this,_thread_default_delegate_funcz);
    _class_default_delegate = CreateDefaultDelegate(this,_class_default_delegate_funcz);
    _instance_default_delegate = CreateDefaultDelegate(this,_instance_default_delegate_funcz);
    _weakref_default_delegate = CreateDefaultDelegate(this,_weakref_default_delegate_funcz);
}

SQSharedState::~SQSharedState() {
    if (_releasehook) {
        _releasehook(_foreignptr, 0);
    }

    // We must drop all current refs,
    // so that GC below can collect all remaining refs
    // This also helps detect any leaks
    _registry.Null();
    _consts.Null();
    _metamethodsmap.Null();
    _root_vm.Null();
    _table_default_delegate.Null();
    _array_default_delegate.Null();
    _string_default_delegate.Null();
    _number_default_delegate.Null();
    _closure_default_delegate.Null();
    _generator_default_delegate.Null();
    _thread_default_delegate.Null();
    _class_default_delegate.Null();
    _instance_default_delegate.Null();
    _weakref_default_delegate.Null();

    _refs_table.Finalize();

    if (_scratchpad) {
        sq_vm_free(_scratchpad, _scratchpadsize);
    }

#ifndef NO_GARBAGE_COLLECTOR
    SQCollectable * t = gc.chain_root;
    SQCollectable * nx = NULL;
    if (t) {
        t->_uiRef++;
        while (t) {
            t->Finalize();

            nx = t->_next;
            if (nx) {
                nx->_uiRef++;
            }

            t->_uiRef--;
            if (t->_uiRef == 0) {
                t->Release();
            }

            t = nx;
        }
    }
    // Entire GC chain must be free
    // Otherwise there are leaks
    assert(gc.chain_root == nullptr);
#endif
}

SQInteger SQSharedState::GetMetaMethodIdxByName(SQObjectPtr const & name) {
    if(sq_type(name) != OT_STRING) {
        return -1;
    }

    SQObjectPtr ret;
    if(_table(_metamethodsmap)->Get(name,ret)) {
        return _integer(ret);
    }

    return -1;
}

#ifndef NO_GARBAGE_COLLECTOR

void SQSharedState::RunMark(SQCollectable ** tchain) {
    SQVM * vms = _thread(_root_vm);

    vms->Mark(tchain);

    _refs_table.Mark(tchain);
    GC::MarkObject(_registry,tchain);
    GC::MarkObject(_consts,tchain);
    GC::MarkObject(_metamethodsmap,tchain);
    GC::MarkObject(_table_default_delegate,tchain);
    GC::MarkObject(_array_default_delegate,tchain);
    GC::MarkObject(_string_default_delegate,tchain);
    GC::MarkObject(_number_default_delegate,tchain);
    GC::MarkObject(_generator_default_delegate,tchain);
    GC::MarkObject(_thread_default_delegate,tchain);
    GC::MarkObject(_closure_default_delegate,tchain);
    GC::MarkObject(_class_default_delegate,tchain);
    GC::MarkObject(_instance_default_delegate,tchain);
    GC::MarkObject(_weakref_default_delegate,tchain);
}

void SQSharedState::ResurrectUnreachable(SQVM * vm) {
    SQCollectable * tchain = NULL;

    RunMark(&tchain);

    SQCollectable *resurrected = gc.chain_root;
    SQCollectable *t = resurrected;

    gc.chain_root = tchain;

    SQArray *ret = NULL;
    if(resurrected) {
        ret = SQArray::Create(this,0);
        SQCollectable *rlast = NULL;
        while(t) {
            rlast = t;
            SQObjectType type = t->GetType();
            if(type != OT_FUNCPROTO && type != OT_OUTER) {
                SQObject sqo;
                sqo._type = type;
                sqo._unVal.pRefCounted = t;
                ret->Append(sqo);
            }
            t = t->_next;
        }

        assert(rlast->_next == NULL);
        rlast->_next = gc.chain_root;
        if(gc.chain_root)
        {
            gc.chain_root->_prev = rlast;
        }
        gc.chain_root = resurrected;
    }

    t = gc.chain_root;
    while(t) {
        t->UnMark();
        t = t->_next;
    }

    if(ret) {
        SQObjectPtr temp = ret;
        vm->Push(temp);
    }
    else {
        vm->PushNull();
    }
}

SQInteger SQSharedState::CollectGarbage() {
    SQInteger n = 0;
    SQCollectable *tchain = NULL;

    RunMark(&tchain);

    SQCollectable *t = gc.chain_root;
    SQCollectable *nx = NULL;
    if(t) {
        t->_uiRef++;
        while(t) {
            t->Finalize();
            nx = t->_next;
            if (nx) nx->_uiRef++;
            t->_uiRef--;
            // And why would that be non zero?
            if(t->_uiRef == 0) {
                t->Release();
            }
            t = nx;
            n++;
        }
    }

    t = tchain;
    while(t) {
        t->UnMark();
        t = t->_next;
    }
    gc.chain_root = tchain;

    return n;
}
#endif

char * SQSharedState::GetScratchPad(size_t size) {
    if (size == 0) {
        return _scratchpad;
    }

    if (_scratchpadsize < size) {
        size_t const newsize = std::max(size + (size >> 1), size);
        _scratchpad = (char*)sq_vm_realloc(_scratchpad, _scratchpadsize, newsize);
        _scratchpadsize = newsize;
    } else if (_scratchpadsize / 32 >= size) {
        size_t const newsize = std::max(_scratchpadsize >> 1, size);
        _scratchpad = (char*)sq_vm_realloc(_scratchpad, _scratchpadsize, newsize);
        _scratchpadsize = newsize;
    }
    return _scratchpad;
}
