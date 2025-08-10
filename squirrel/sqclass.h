/*  see copyright notice in squirrel.h */
#ifndef _SQCLASS_H_
#define _SQCLASS_H_

#include <new>

#include "sqtable.h"
#include "SQDelegable.hpp"

struct SQInstance;

struct SQClassMember {
    SQObjectPtr val;
    SQObjectPtr attrs;
    void Null() {
        val.Null();
        attrs.Null();
    }
};

typedef sqvector<SQClassMember> SQClassMemberVec;

#define MEMBER_TYPE_METHOD 0x01000000
#define MEMBER_TYPE_FIELD 0x02000000
#define MEMBER_MAX_COUNT 0x00FFFFFF

#define _ismethod(o) (_integer(o)&MEMBER_TYPE_METHOD)
#define _isfield(o) (_integer(o)&MEMBER_TYPE_FIELD)
#define _make_method_idx(i) ((SQInteger)(MEMBER_TYPE_METHOD|i))
#define _make_field_idx(i) ((SQInteger)(MEMBER_TYPE_FIELD|i))
#define _member_type(o) (_integer(o)&0xFF000000)
#define _member_idx(o) (_integer(o)&0x00FFFFFF)

struct SQClass : public CHAINABLE_OBJ
{
    SQClass(SQSharedState *ss,SQClass *base);
public:
    static SQClass* Create(SQSharedState *ss,SQClass *base) {
        SQClass *newclass = (SQClass *)sq_vm_malloc(sizeof(SQClass));
        new (newclass) SQClass(ss, base);
        return newclass;
    }
    ~SQClass();
    bool NewSlot(SQSharedState *ss, const SQObjectPtr &key,const SQObjectPtr &val,bool bstatic);
    bool Get(const SQObjectPtr &key,SQObjectPtr &val) {
        if(_members->Get(key,val)) {
            if(_isfield(val)) {
                SQObjectPtr &o = _defaultvalues[_member_idx(val)].val;
                val = _realval(o);
            }
            else {
                val = _methods[_member_idx(val)].val;
            }
            return true;
        }
        return false;
    }
    bool GetConstructor(SQObjectPtr &ctor)
    {
        if(_constructoridx != -1) {
            ctor = _methods[_constructoridx].val;
            return true;
        }
        return false;
    }
    bool SetAttributes(const SQObjectPtr &key,const SQObjectPtr &val);
    bool GetAttributes(const SQObjectPtr &key,SQObjectPtr &outval);
    void Lock() { _locked = true; if(_base) _base->Lock(); }

    void Release() override {
        if (_hook) {
            _hook(_typetag,0);
        }
        this->~SQClass();
        sq_vm_free(this, sizeof(*this));
    }

    void Finalize() override;
#ifndef NO_GARBAGE_COLLECTOR
    void Mark(SQCollectable ** ) override;
    SQObjectType GetType() override {
        return OT_CLASS;
    }
#endif
    SQInteger Next(const SQObjectPtr &refpos, SQObjectPtr &outkey, SQObjectPtr &outval);
    SQInstance *CreateInstance();
    SQTable *_members;
    SQClass *_base;
    SQClassMemberVec _defaultvalues;
    SQClassMemberVec _methods;
    SQObjectPtr _metamethods[MT_LAST];
    SQObjectPtr _attributes;
    SQUserPointer _typetag;
    SQRELEASEHOOK _hook;
    bool _locked;
    SQInteger _constructoridx;
    SQInteger _udsize;
};

#endif //_SQCLASS_H_
