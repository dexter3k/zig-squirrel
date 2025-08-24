#include "SQClass.hpp"

#include "SQClosure.hpp"
#include "SQInstance.hpp"

SQClass::SQClass(SQSharedState * ss, SQClass * base)
    : CHAINABLE_OBJ(ss)
{
    _base = base;
    _typetag = 0;
    _hook = NULL;
    _udsize = 0;
    _locked = false;
    _constructoridx = -1;
    if(_base) {
        _constructoridx = _base->_constructoridx;
        _udsize = _base->_udsize;
        _defaultvalues.copy(base->_defaultvalues);
        _methods.copy(base->_methods);
        _COPY_VECTOR(_metamethods,base->_metamethods,MT_LAST);
        _base->IncreaseRefCount();
    }
    _members = base
        ? base->_members->Clone()
        : SQTable::Create(ss, 0);
    _members->IncreaseRefCount();
}

void SQClass::Finalize() {
    _attributes.Null();
    _NULL_SQOBJECT_VECTOR(_defaultvalues,_defaultvalues.size());
    _methods.resize(0);
    _NULL_SQOBJECT_VECTOR(_metamethods,MT_LAST);

    _members->DecreaseRefCount();
    _members = nullptr;

    if(_base) {
        _base->DecreaseRefCount();
        _base = nullptr;
    }
}

SQClass::~SQClass() {
    Finalize();
}

bool SQClass::NewSlot(SQSharedState *ss,const SQObjectPtr &key,const SQObjectPtr &val,bool bstatic)
{
    SQObjectPtr temp;
    bool belongs_to_static_table = sq_type(val) == OT_CLOSURE || sq_type(val) == OT_NATIVECLOSURE || bstatic;
    if(_locked && !belongs_to_static_table)
        return false; //the class already has an instance so cannot be modified
    if(_members->Get(key,temp) && _isfield(temp)) //overrides the default value
    {
        _defaultvalues[_member_idx(temp)].val = val;
        return true;
    }
	if (_members->CountUsed() >= MEMBER_MAX_COUNT) {
		return false;
	}
    if(belongs_to_static_table) {
        SQInteger mmidx;
        if((sq_type(val) == OT_CLOSURE || sq_type(val) == OT_NATIVECLOSURE) &&
            (mmidx = ss->GetMetaMethodIdxByName(key)) != -1) {
            _metamethods[mmidx] = val;
        }
        else {
            SQObjectPtr theval = val;
            if(_base && sq_type(val) == OT_CLOSURE) {
                theval = _closure(val)->Clone();
                _closure(theval)->_base = _base;
                _base->IncreaseRefCount(); // for the closure
            }
            if(sq_type(temp) == OT_NULL) {
                bool isconstructor;
                SQVM::IsEqual(ss->_constructoridx, key, isconstructor);
                if(isconstructor) {
                    _constructoridx = (SQInteger)_methods.size();
                }
                SQClassMember m;
                m.val = theval;
                _members->NewSlot(key,SQObjectPtr(_make_method_idx(_methods.size())));
                _methods.push_back(m);
            }
            else {
                _methods[_member_idx(temp)].val = theval;
            }
        }
        return true;
    }
    SQClassMember m;
    m.val = val;
    _members->NewSlot(key,SQObjectPtr(_make_field_idx(_defaultvalues.size())));
    _defaultvalues.push_back(m);
    return true;
}

SQInstance *SQClass::CreateInstance()
{
    if(!_locked) Lock();
    return SQInstance::Create(_opt_ss(this),this);
}

SQInteger SQClass::Next(const SQObjectPtr &refpos, SQObjectPtr &outkey, SQObjectPtr &outval)
{
    SQObjectPtr oval;
    SQInteger idx = _members->Next(false,refpos,outkey,oval);
    if(idx != -1) {
        if(_ismethod(oval)) {
            outval = _methods[_member_idx(oval)].val;
        }
        else {
            SQObjectPtr &o = _defaultvalues[_member_idx(oval)].val;
            outval = _realval(o);
        }
    }
    return idx;
}

bool SQClass::SetAttributes(const SQObjectPtr &key,const SQObjectPtr &val)
{
    SQObjectPtr idx;
    if(_members->Get(key,idx)) {
        if(_isfield(idx))
            _defaultvalues[_member_idx(idx)].attrs = val;
        else
            _methods[_member_idx(idx)].attrs = val;
        return true;
    }
    return false;
}

bool SQClass::GetAttributes(const SQObjectPtr &key,SQObjectPtr &outval)
{
    SQObjectPtr idx;
    if(_members->Get(key,idx)) {
        outval = (_isfield(idx)?_defaultvalues[_member_idx(idx)].attrs:_methods[_member_idx(idx)].attrs);
        return true;
    }
    return false;
}
