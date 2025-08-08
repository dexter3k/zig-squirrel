#include "SQDelegable.hpp"

#include <new>

#include "sqtable.h"
#include "sqvm.h"

bool SQDelegable::SetDelegate(SQTable * mt) {
    SQTable * temp = mt;
    if (temp == this) {
    	return false;
    }
    while (temp) {
        if (temp->_delegate == this) {
        	return false; // cycle detected
        }
        temp = temp->_delegate;
    }
    if (mt) {
    	__ObjAddRef(mt);
    }
    __ObjRelease(_delegate);
    _delegate = mt;
    return true;
}

bool SQDelegable::GetMetaMethod(SQVM * v, SQMetaMethod mm, SQObjectPtr & res) {
    if (_delegate) {
        return _delegate->Get(v->_sharedstate->_metamethods[mm], res);
    }
    return false;
}
