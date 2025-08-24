#include "SQCollectable.hpp"

#include <cassert>

#include "sqstate.h"

SQCollectable::SQCollectable(SQSharedState * ss)
    : SQRefCounted(nullptr)
    , _next(nullptr)
    , _prev(nullptr)
    , _sharedstate(ss)
{
	AddToChain(&ss->gc.chain_root);
}

SQCollectable::~SQCollectable() {
    // TODO: investigate
    assert(!(_uiRef & MARK_FLAG));
    RemoveFromChain(&_sharedstate->gc.chain_root);
}
