/*
    see copyright notice in squirrel.h
*/
#include <algorithm>

#include "sqpcheader.h"
#include "sqopcodes.h"
#include "sqvm.h"
#include "sqfuncproto.h"
#include "sqclosure.h"
#include "sqstring.h"
#include "sqtable.h"
#include "sqarray.h"
#include "squserdata.h"
#include "sqclass.h"

bool CompileTypemask(sqvector<SQInteger> &res,const SQChar *typemask)
{
    SQInteger i = 0;
    SQInteger mask = 0;
    while(typemask[i] != 0) {
        switch(typemask[i]) {
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
            case '.': mask = -1; res.push_back(mask); i++; mask = 0; continue;
            case ' ': i++; continue; //ignores spaces
            default:
                return false;
        }
        i++;
        if(typemask[i] == '|') {
            i++;
            if(typemask[i] == 0)
                return false;
            continue;
        }
        res.push_back(mask);
        mask = 0;

    }
    return true;
}

static void new_metamethod(SQSharedState * state, char const * name) {
    state->_metamethods.push_back(SQString::Create(state, name));
    _table(state->_metamethodsmap)->NewSlot(
        state->_metamethods.back(),
        SQInteger(state->_metamethods.size() - 1));
}

static SQTable *CreateDefaultDelegate(SQSharedState *ss, const SQRegFunction *funcz) {
    SQInteger i=0;
    SQTable *t=SQTable::Create(ss,0);
    while(funcz[i].name!=0){
        SQNativeClosure *nc = SQNativeClosure::Create(ss,funcz[i].f,0);
        nc->_nparamscheck = funcz[i].nparamscheck;
        nc->_name = SQString::Create(ss,funcz[i].name);
        if(funcz[i].typemask && !CompileTypemask(nc->_typecheck,funcz[i].typemask)) {
            return NULL;
        }
        t->NewSlot(SQString::Create(ss,funcz[i].name),nc);
        i++;
    }
    return t;
}

void SQSharedState::Init() {
    _metamethodsmap = SQTable::Create(this, MT_LAST - 1);

    _systemstrings.push_back(SQString::Create(this, "null"));
    _systemstrings.push_back(SQString::Create(this, "table"));
    _systemstrings.push_back(SQString::Create(this, "array"));
    _systemstrings.push_back(SQString::Create(this, "closure"));
    _systemstrings.push_back(SQString::Create(this, "string"));
    _systemstrings.push_back(SQString::Create(this, "userdata"));
    _systemstrings.push_back(SQString::Create(this, "integer"));
    _systemstrings.push_back(SQString::Create(this, "float"));
    _systemstrings.push_back(SQString::Create(this, "userpointer"));
    _systemstrings.push_back(SQString::Create(this, "function"));
    _systemstrings.push_back(SQString::Create(this, "generator"));
    _systemstrings.push_back(SQString::Create(this, "thread"));
    _systemstrings.push_back(SQString::Create(this, "class"));
    _systemstrings.push_back(SQString::Create(this, "instance"));
    _systemstrings.push_back(SQString::Create(this, "bool"));

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

    _constructoridx = SQString::Create(this, "constructor");
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
    if(_releasehook) {
        _releasehook(_foreignptr, 0);
        _releasehook = NULL;
    }

    _constructoridx.Null();
    _table(_registry)->Finalize();
    _table(_consts)->Finalize();
    _table(_metamethodsmap)->Finalize();
    _registry.Null();
    _consts.Null();
    _metamethodsmap.Null();

    while (!_systemstrings.empty()) {
        _systemstrings.back().Null();
        _systemstrings.pop_back();
    }

    _thread(_root_vm)->Finalize();
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
#ifndef NO_GARBAGE_COLLECTOR
    SQCollectable *t = _gc_chain;
    SQCollectable *nx = NULL;
    if(t) {
        t->_uiRef++;
        while(t) {
            t->Finalize();
            nx = t->_next;
            if(nx) nx->_uiRef++;
            if(--t->_uiRef == 0)
                t->Release();
            t = nx;
        }
    }
    assert(_gc_chain==NULL); //just to proove a theory
    while(_gc_chain){
        _gc_chain->_uiRef++;
        _gc_chain->Release();
    }
#endif
    if(_scratchpad)sq_vm_free(_scratchpad,_scratchpadsize);
}


SQInteger SQSharedState::GetMetaMethodIdxByName(const SQObjectPtr &name)
{
    if(sq_type(name) != OT_STRING)
        return -1;
    SQObjectPtr ret;
    if(_table(_metamethodsmap)->Get(name,ret)) {
        return _integer(ret);
    }
    return -1;
}

#ifndef NO_GARBAGE_COLLECTOR

void SQSharedState::MarkObject(SQObjectPtr &o,SQCollectable **chain)
{
    switch(sq_type(o)){
    case OT_TABLE:_table(o)->Mark(chain);break;
    case OT_ARRAY:_array(o)->Mark(chain);break;
    case OT_USERDATA:_userdata(o)->Mark(chain);break;
    case OT_CLOSURE:_closure(o)->Mark(chain);break;
    case OT_NATIVECLOSURE:_nativeclosure(o)->Mark(chain);break;
    case OT_GENERATOR:_generator(o)->Mark(chain);break;
    case OT_THREAD:_thread(o)->Mark(chain);break;
    case OT_CLASS:_class(o)->Mark(chain);break;
    case OT_INSTANCE:_instance(o)->Mark(chain);break;
    case OT_OUTER:_outer(o)->Mark(chain);break;
    case OT_FUNCPROTO:_funcproto(o)->Mark(chain);break;
    default: break; //shutup compiler
    }
}

void SQSharedState::RunMark(SQVM* SQ_UNUSED_ARG(vm),SQCollectable **tchain)
{
    SQVM *vms = _thread(_root_vm);

    vms->Mark(tchain);

    _refs_table.Mark(tchain);
    MarkObject(_registry,tchain);
    MarkObject(_consts,tchain);
    MarkObject(_metamethodsmap,tchain);
    MarkObject(_table_default_delegate,tchain);
    MarkObject(_array_default_delegate,tchain);
    MarkObject(_string_default_delegate,tchain);
    MarkObject(_number_default_delegate,tchain);
    MarkObject(_generator_default_delegate,tchain);
    MarkObject(_thread_default_delegate,tchain);
    MarkObject(_closure_default_delegate,tchain);
    MarkObject(_class_default_delegate,tchain);
    MarkObject(_instance_default_delegate,tchain);
    MarkObject(_weakref_default_delegate,tchain);

}

void SQSharedState::ResurrectUnreachable(SQVM * vm) {
    SQCollectable *tchain=NULL;

    RunMark(vm,&tchain);

    SQCollectable *resurrected = _gc_chain;
    SQCollectable *t = resurrected;

    _gc_chain = tchain;

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
        rlast->_next = _gc_chain;
        if(_gc_chain)
        {
            _gc_chain->_prev = rlast;
        }
        _gc_chain = resurrected;
    }

    t = _gc_chain;
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

SQInteger SQSharedState::CollectGarbage(SQVM *vm)
{
    SQInteger n = 0;
    SQCollectable *tchain = NULL;

    RunMark(vm,&tchain);

    SQCollectable *t = _gc_chain;
    SQCollectable *nx = NULL;
    if(t) {
        t->_uiRef++;
        while(t) {
            t->Finalize();
            nx = t->_next;
            if(nx) nx->_uiRef++;
            if(--t->_uiRef == 0)
                t->Release();
            t = nx;
            n++;
        }
    }

    t = tchain;
    while(t) {
        t->UnMark();
        t = t->_next;
    }
    _gc_chain = tchain;

    return n;
}
#endif

#ifndef NO_GARBAGE_COLLECTOR
void SQCollectable::AddToChain(SQCollectable **chain,SQCollectable *c)
{
    c->_prev = NULL;
    c->_next = *chain;
    if(*chain) (*chain)->_prev = c;
    *chain = c;
}

void SQCollectable::RemoveFromChain(SQCollectable **chain,SQCollectable *c)
{
    if(c->_prev) c->_prev->_next = c->_next;
    else *chain = c->_next;
    if(c->_next)
        c->_next->_prev = c->_prev;
    c->_next = NULL;
    c->_prev = NULL;
}
#endif

SQChar* SQSharedState::GetScratchPad(SQInteger size)
{
    SQInteger newsize;
    if (size > 0) {
        if (_scratchpadsize < size) {
            newsize = (SQInteger)((SQUnsignedInteger)size + (size >> 1));
            newsize = std::max(newsize, size); //check for overflow
            _scratchpad = (SQChar*)sq_vm_realloc(_scratchpad, _scratchpadsize, newsize);
            _scratchpadsize = newsize;

        }
        else if ((_scratchpadsize >> 5) >= size) {
            newsize = _scratchpadsize >> 1;
            newsize = std::max(newsize, size); //check for overflow
            _scratchpad = (SQChar*)sq_vm_realloc(_scratchpad, _scratchpadsize, newsize);
            _scratchpadsize = newsize;
        }
    }
    return _scratchpad;
}

RefTable::RefTable()
{
    AllocNodes(4);
}

void RefTable::Finalize()
{
    RefNode *nodes = _nodes;
    for(SQUnsignedInteger n = 0; n < _numofslots; n++) {
        nodes->obj.Null();
        nodes++;
    }
}

RefTable::~RefTable()
{
    sq_vm_free(_buckets,(_numofslots * sizeof(RefNode *)) + (_numofslots * sizeof(RefNode)));
}

#ifndef NO_GARBAGE_COLLECTOR
void RefTable::Mark(SQCollectable **chain)
{
    RefNode *nodes = (RefNode *)_nodes;
    for(SQUnsignedInteger n = 0; n < _numofslots; n++) {
        if(sq_type(nodes->obj) != OT_NULL) {
            SQSharedState::MarkObject(nodes->obj,chain);
        }
        nodes++;
    }
}
#endif

void RefTable::AddRef(SQObject &obj)
{
    SQHash mainpos;
    RefNode *prev;
    RefNode *ref = Get(obj,mainpos,&prev,true);
    ref->refs++;
}

SQUnsignedInteger RefTable::GetRefCount(SQObject &obj)
{
     SQHash mainpos;
     RefNode *prev;
     RefNode *ref = Get(obj,mainpos,&prev,true);
     return ref->refs;
}


SQBool RefTable::Release(SQObject &obj)
{
    SQHash mainpos;
    RefNode *prev;
    RefNode *ref = Get(obj,mainpos,&prev,false);
    if(ref) {
        if(--ref->refs == 0) {
            SQObjectPtr o = ref->obj;
            if(prev) {
                prev->next = ref->next;
            }
            else {
                _buckets[mainpos] = ref->next;
            }
            ref->next = _freelist;
            _freelist = ref;
            _slotused--;
            ref->obj.Null();
            //<<FIXME>>test for shrink?
            return SQTrue;
        }
    }
    else {
        assert(0);
    }
    return SQFalse;
}

void RefTable::Resize(SQUnsignedInteger size)
{
    RefNode **oldbucks = _buckets;
    RefNode *t = _nodes;
    SQUnsignedInteger oldnumofslots = _numofslots;
    AllocNodes(size);
    //rehash
    SQUnsignedInteger nfound = 0;
    for(SQUnsignedInteger n = 0; n < oldnumofslots; n++) {
        if(sq_type(t->obj) != OT_NULL) {
            //add back;
            assert(t->refs != 0);
            RefNode *nn = Add(::HashObj(t->obj)&(_numofslots-1),t->obj);
            nn->refs = t->refs;
            t->obj.Null();
            nfound++;
        }
        t++;
    }
    (void)nfound;
    assert(nfound == oldnumofslots);
    sq_vm_free(oldbucks,(oldnumofslots * sizeof(RefNode *)) + (oldnumofslots * sizeof(RefNode)));
}

RefTable::RefNode *RefTable::Add(SQHash mainpos,SQObject &obj)
{
    RefNode *t = _buckets[mainpos];
    RefNode *newnode = _freelist;
    newnode->obj = obj;
    _buckets[mainpos] = newnode;
    _freelist = _freelist->next;
    newnode->next = t;
    assert(newnode->refs == 0);
    _slotused++;
    return newnode;
}

RefTable::RefNode *RefTable::Get(SQObject &obj,SQHash &mainpos,RefNode **prev,bool add)
{
    RefNode *ref;
    mainpos = ::HashObj(obj)&(_numofslots-1);
    *prev = NULL;
    for (ref = _buckets[mainpos]; ref; ) {
        if(_rawval(ref->obj) == _rawval(obj) && sq_type(ref->obj) == sq_type(obj))
            break;
        *prev = ref;
        ref = ref->next;
    }
    if(ref == NULL && add) {
        if(_numofslots == _slotused) {
            assert(_freelist == 0);
            Resize(_numofslots*2);
            mainpos = ::HashObj(obj)&(_numofslots-1);
        }
        ref = Add(mainpos,obj);
    }
    return ref;
}

void RefTable::AllocNodes(SQUnsignedInteger size)
{
    RefNode **bucks;
    RefNode *nodes;
    bucks = (RefNode **)sq_vm_malloc((size * sizeof(RefNode *)) + (size * sizeof(RefNode)));
    nodes = (RefNode *)&bucks[size];
    RefNode *temp = nodes;
    SQUnsignedInteger n;
    for(n = 0; n < size - 1; n++) {
        bucks[n] = NULL;
        temp->refs = 0;
        new (&temp->obj) SQObjectPtr;
        temp->next = temp+1;
        temp++;
    }
    bucks[n] = NULL;
    temp->refs = 0;
    new (&temp->obj) SQObjectPtr;
    temp->next = NULL;
    _freelist = nodes;
    _nodes = nodes;
    _buckets = bucks;
    _slotused = 0;
    _numofslots = size;
}
