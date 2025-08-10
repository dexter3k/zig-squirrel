#include "SQInstance.hpp"

SQInstance::SQInstance(SQSharedState * ss, SQClass * c, size_t memsize)
	: SQDelegable()
	, klass(c)
	, user_data(nullptr)
	, _hook(nullptr)
	, _memsize(memsize)
	, _values{}
{
    size_t const nvalues = klass->_defaultvalues.size();
    for(size_t n = 0; n < nvalues; n++) {
        new (&_values[n]) SQObjectPtr(klass->_defaultvalues[n].val);
    }

    __ObjAddRef(klass);
    _delegate = klass->_members;

    INIT_CHAIN();
    ADD_TO_CHAIN(&_sharedstate->_gc_chain, this);
}

SQInstance::SQInstance(SQSharedState * ss, SQInstance * i, size_t memsize)
    : SQDelegable()
    , klass(i->klass)
    , user_data(nullptr)
    , _hook(nullptr)
    , _memsize(memsize)
    , _values{}
{
    size_t const nvalues = klass->_defaultvalues.size();
    for(size_t n = 0; n < nvalues; n++) {
        new (&_values[n]) SQObjectPtr(i->_values[n]);
    }

    __ObjAddRef(klass);
    _delegate = klass->_members;

    INIT_CHAIN();
    ADD_TO_CHAIN(&_sharedstate->_gc_chain, this);
}

SQInstance::~SQInstance() {
    REMOVE_FROM_CHAIN(&_sharedstate->_gc_chain, this);

    // if klass is null it was already finalized by the GC
    if (klass) {
        Finalize();
    }
}

void SQInstance::Finalize() {
    size_t const nvalues = klass->_defaultvalues.size();

    __ObjRelease(klass);
    klass = nullptr;

    for (size_t i = 0; i < nvalues; i++) {
        _values[i].Null();
    }
}

bool SQInstance::GetMetaMethod(SQVM * v, SQMetaMethod mm, SQObjectPtr & res) {
    (void)v;

    if (sq_type(klass->_metamethods[mm]) != OT_NULL) {
        res = klass->_metamethods[mm];
        return true;
    }
    return false;
}

bool SQInstance::InstanceOf(SQClass *trg) {
    SQClass *parent = klass;
    while(parent != NULL) {
        if(parent == trg) {
            return true;
        }
        parent = parent->_base;
    }
    return false;
}

#ifndef NO_GARBAGE_COLLECTOR
void SQInstance::Mark(SQCollectable **chain)
{
    START_MARK()
        klass->Mark(chain);
        SQUnsignedInteger nvalues = klass->_defaultvalues.size();
        for(SQUnsignedInteger i =0; i< nvalues; i++) {
            SQSharedState::MarkObject(_values[i], chain);
        }
    END_MARK()
}
#endif
