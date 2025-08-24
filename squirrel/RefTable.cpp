#include "RefTable.hpp"

#include "GC.hpp"
#include "sqstate.h"
#include "SQTable.hpp"

RefTable::RefTable()
    : _numofslots(0)
    , _slotused(0)
    , _nodes(nullptr)
    , _freelist(nullptr)
    , _buckets(nullptr)
{
    AllocNodes(4);
}

RefTable::~RefTable() {
    sq_vm_free(_buckets, (_numofslots * sizeof(RefNode *)) + (_numofslots * sizeof(RefNode)));
}

void RefTable::Finalize() {
    RefNode * nodes = _nodes;

    for (SQUnsignedInteger n = 0; n < _numofslots; n++) {
        nodes->obj.Null();
        nodes++;
    }
}

SQBool RefTable::Release(SQObject & obj) {
    SQHash mainpos;
    RefNode *prev;
    RefNode *ref = Get(obj,mainpos,&prev,false);
    if (ref) {
        if (--ref->refs == 0) {
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

#ifndef NO_GARBAGE_COLLECTOR
void RefTable::Mark(SQCollectable **chain) {
    RefNode *nodes = (RefNode *)_nodes;
    for(SQUnsignedInteger n = 0; n < _numofslots; n++) {
        if(sq_type(nodes->obj) != OT_NULL) {
            GC::MarkObject(nodes->obj,chain);
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

void RefTable::AllocNodes(SQUnsignedInteger size) {
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
