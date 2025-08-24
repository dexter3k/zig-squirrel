/*  see copyright notice in squirrel.h */
#ifndef _SQOBJECT_H_
#define _SQOBJECT_H_

#include <cassert>

#include <squirrel.h>

#include "sqvector.hpp"

enum SQMetaMethod {
    MT_ADD = 0,
    MT_SUB = 1,
    MT_MUL = 2,
    MT_DIV = 3,
    MT_UNM = 4,
    MT_MODULO = 5,
    MT_SET = 6,
    MT_GET = 7,
    MT_TYPEOF = 8,
    MT_NEXTI = 9,
    MT_CMP = 10,
    MT_CALL = 11,
    MT_CLONED = 12,
    MT_NEWSLOT = 13,
    MT_DELSLOT = 14,
    MT_TOSTRING = 15,
    MT_NEWMEMBER = 16,
    MT_INHERITED = 17,
    MT_LAST = 18
};

#define MM_ADD       ("_add")
#define MM_SUB       ("_sub")
#define MM_MUL       ("_mul")
#define MM_DIV       ("_div")
#define MM_UNM       ("_unm")
#define MM_MODULO    ("_modulo")
#define MM_SET       ("_set")
#define MM_GET       ("_get")
#define MM_TYPEOF    ("_typeof")
#define MM_NEXTI     ("_nexti")
#define MM_CMP       ("_cmp")
#define MM_CALL      ("_call")
#define MM_CLONED    ("_cloned")
#define MM_NEWSLOT   ("_newslot")
#define MM_DELSLOT   ("_delslot")
#define MM_TOSTRING  ("_tostring")
#define MM_NEWMEMBER ("_newmember")
#define MM_INHERITED ("_inherited")


#define _CONSTRUCT_VECTOR(type,size,ptr) { \
    for(SQInteger n = 0; n < ((SQInteger)size); n++) { \
            new (&ptr[n]) type(); \
        } \
}

#define _DESTRUCT_VECTOR(type,size,ptr) { \
    for(SQInteger nl = 0; nl < ((SQInteger)size); nl++) { \
            ptr[nl].~type(); \
    } \
}

#define _COPY_VECTOR(dest,src,size) { \
    for(SQInteger _n_ = 0; _n_ < ((SQInteger)size); _n_++) { \
        dest[_n_] = src[_n_]; \
    } \
}

#define _NULL_SQOBJECT_VECTOR(vec,size) { \
    for(SQInteger _n_ = 0; _n_ < ((SQInteger)size); _n_++) { \
        vec[_n_].Null(); \
    } \
}

struct SQRefCounted {
    SQUnsignedInteger _uiRef;
    struct SQWeakRef *_weakref;

    SQRefCounted()
        : _uiRef(0)
        , _weakref(nullptr)
    {}

    SQRefCounted(void *)
        : SQRefCounted()
    {}

    virtual ~SQRefCounted();

    // User must implement release to
    // 1. Call destructor and release others
    //  - this is important, because this invalidates weakrefs!
    // 2. Free memory for itself
    virtual void Release() = 0;

    inline void IncreaseRefCount() {
        _uiRef++;
    }

    inline void DecreaseRefCount() {
        assert(_uiRef > 0);

        _uiRef--;
        if (_uiRef == 0) {
            Release();
        }
    }

    SQWeakRef * GetWeakRef(SQObjectType type);
};

struct SQWeakRef : SQRefCounted {
    // Q: can we replace this with just a pointer to SQRefCounted?
    // A: no, weakref must know the type for proper deref
    SQObject _obj;

    void Release() {
        // Q: how do we have a weakref to non-refcounted?
        // A: it can be OT_NULL
        if (ISREFCOUNTED(_obj._type)) {
            _obj._unVal.pRefCounted->_weakref = nullptr;
        }

        this->~SQWeakRef();
        sq_vm_free(this, sizeof(SQWeakRef));
    }
};

struct SQObjectPtr;

#define is_delegable(t) (sq_type(t)&SQOBJECT_DELEGABLE)
#define raw_type(obj) _RAW_TYPE((obj)._type)

#define _integer(obj) ((obj)._unVal.nInteger)
#define _float(obj) ((obj)._unVal.fFloat)
#define _string(obj) ((obj)._unVal.pString)
#define _table(obj) ((obj)._unVal.pTable)
#define _array(obj) ((obj)._unVal.pArray)
#define _closure(obj) ((obj)._unVal.pClosure)
#define _generator(obj) ((obj)._unVal.pGenerator)
#define _nativeclosure(obj) ((obj)._unVal.pNativeClosure)
#define _userdata(obj) ((obj)._unVal.pUserData)
#define _userpointer(obj) ((obj)._unVal.pUserPointer)
#define _thread(obj) ((obj)._unVal.pThread)
#define _funcproto(obj) ((obj)._unVal.pFunctionProto)
#define _class(obj) ((obj)._unVal.pClass)
#define _instance(obj) ((obj)._unVal.pInstance)
#define _delegable(obj) ((obj)._unVal.pDelegable)
#define _weakref(obj) ((obj)._unVal.pWeakRef)
#define _outer(obj) ((obj)._unVal.pOuter)
#define _refcounted(obj) ((obj)._unVal.pRefCounted)
#define _rawval(obj) ((obj)._unVal.raw)

#define _stringval(obj) (obj)._unVal.pString->_val
#define _userdataval(obj) (SQUserPointer((obj)._unVal.pUserData + 1))

#define tofloat(num) ((sq_type(num)==OT_INTEGER)?(SQFloat)_integer(num):_float(num))
#define tointeger(num) ((sq_type(num)==OT_FLOAT)?(SQInteger)_float(num):_integer(num))

static inline SQObject _realval(SQObject o) {
    if (sq_type(o) == OT_WEAKREF) {
        return _weakref(o)->_obj;
    }

    return o;
}

#define _REF_TYPE_DECL(type,_class,sym) \
    SQObjectPtr(_class * x) \
    { \
        _unVal.raw = 0; \
        _type=type; \
        _unVal.sym = x; \
        assert(_unVal.sym); \
        _unVal.pRefCounted->IncreaseRefCount(); \
    } \
    inline SQObjectPtr& operator=(_class *x) \
    {  \
        SQObjectType tOldType; \
        SQObjectValue unOldVal; \
        tOldType=_type; \
        unOldVal=_unVal; \
        _type = type; \
        _unVal.raw = 0; \
        _unVal.sym = x; \
        _unVal.pRefCounted->IncreaseRefCount(); \
        if (ISREFCOUNTED(tOldType)) { \
            unOldVal.pRefCounted->DecreaseRefCount(); \
        } \
        return *this; \
    }

#define _SCALAR_TYPE_DECL(type,_class,sym) \
    SQObjectPtr(_class x) \
    { \
        _unVal.raw = 0; \
        _type=type; \
        _unVal.sym = x; \
    } \
    inline SQObjectPtr& operator=(_class x) \
    {  \
        if (ISREFCOUNTED(_type)) { \
            _unVal.pRefCounted->DecreaseRefCount(); \
        } \
        _type = type; \
        _unVal.raw = 0; \
        _unVal.sym = x; \
        return *this; \
    }

struct SQObjectPtr : public SQObject {
    /*
        Essentially an automatic reference manager for SQObject
    */

    SQObjectPtr() {
        _type = OT_NULL;
        _unVal.raw = 0;
    }

    SQObjectPtr(SQObjectPtr const & o) {
        _type = o._type;
        _unVal = o._unVal;
        if (ISREFCOUNTED(_type)) {
            _unVal.pRefCounted->IncreaseRefCount();
        }
    }

    SQObjectPtr(SQObject const & o) {
        _type = o._type;
        _unVal = o._unVal;
        if (ISREFCOUNTED(_type)) {
            _unVal.pRefCounted->IncreaseRefCount();
        }
    }

    _REF_TYPE_DECL(OT_TABLE,SQTable,pTable)
    _REF_TYPE_DECL(OT_CLASS,SQClass,pClass)
    _REF_TYPE_DECL(OT_INSTANCE,SQInstance,pInstance)
    _REF_TYPE_DECL(OT_ARRAY,SQArray,pArray)
    _REF_TYPE_DECL(OT_CLOSURE,SQClosure,pClosure)
    _REF_TYPE_DECL(OT_NATIVECLOSURE,SQNativeClosure,pNativeClosure)
    _REF_TYPE_DECL(OT_OUTER,SQOuter,pOuter)
    _REF_TYPE_DECL(OT_GENERATOR,SQGenerator,pGenerator)
    _REF_TYPE_DECL(OT_STRING,SQString,pString)
    _REF_TYPE_DECL(OT_USERDATA,SQUserData,pUserData)
    _REF_TYPE_DECL(OT_WEAKREF,SQWeakRef,pWeakRef)
    _REF_TYPE_DECL(OT_THREAD,SQVM,pThread)
    _REF_TYPE_DECL(OT_FUNCPROTO,SQFunctionProto,pFunctionProto)

    _SCALAR_TYPE_DECL(OT_INTEGER,SQInteger,nInteger)
    _SCALAR_TYPE_DECL(OT_FLOAT,SQFloat,fFloat)
    _SCALAR_TYPE_DECL(OT_USERPOINTER,SQUserPointer,pUserPointer)

    SQObjectPtr(bool bBool) {
        _unVal.raw = 0;

        _type = OT_BOOL;
        _unVal.nInteger = bBool? 1 : 0;
    }

    ~SQObjectPtr() {
        if (ISREFCOUNTED(_type)) {
            _unVal.pRefCounted->DecreaseRefCount();
        }
    }

    inline void Null() {
        if (ISREFCOUNTED(_type)) {
            _unVal.pRefCounted->DecreaseRefCount();
        }

        _type = OT_NULL;
        _unVal.raw = 0;
    }

    inline SQObjectPtr & operator=(bool b) {
        if (ISREFCOUNTED(_type)) {
            _unVal.pRefCounted->DecreaseRefCount();
        }

        _type = OT_BOOL;
        _unVal.nInteger = b ? 1 : 0;

        return *this;
    }

    inline SQObjectPtr& operator=(const SQObjectPtr& obj) {
        SQObjectType tOldType;
        SQObjectValue unOldVal;
        tOldType=_type;
        unOldVal=_unVal;
        _unVal = obj._unVal;
        _type = obj._type;
        if (ISREFCOUNTED(_type)) {
            _unVal.pRefCounted->IncreaseRefCount();
        }
        if (ISREFCOUNTED(tOldType)) {
            unOldVal.pRefCounted->DecreaseRefCount();
        }
        return *this;
    }

    inline SQObjectPtr& operator=(const SQObject& obj) {
        SQObjectType tOldType;
        SQObjectValue unOldVal;
        tOldType=_type;
        unOldVal=_unVal;
        _unVal = obj._unVal;
        _type = obj._type;
        if (ISREFCOUNTED(_type)) {
            _unVal.pRefCounted->IncreaseRefCount();
        }
        if (ISREFCOUNTED(tOldType)) {
            unOldVal.pRefCounted->DecreaseRefCount();
        }
        return *this;
    }

private:
    SQObjectPtr(SQChar const *) = delete;
};

inline void _Swap(SQObject & a, SQObject & b) {
    SQObjectType tOldType = a._type;
    SQObjectValue unOldVal = a._unVal;
    a._type = b._type;
    a._unVal = b._unVal;
    b._type = tOldType;
    b._unVal = unOldVal;
}

/////////////////////////////////////////////////////////////////////////////////////
#ifndef NO_GARBAGE_COLLECTOR

#define MARK_FLAG 0x80000000

#define START_MARK() if (!(_uiRef & MARK_FLAG)) { \
                         _uiRef |= MARK_FLAG;

#define END_MARK()       this->RemoveFromChain(&_sharedstate->gc.chain_root); \
                         this->AddToChain(chain); \
                     }

#define CHAINABLE_OBJ SQCollectable

#else

#define CHAINABLE_OBJ SQRefCounted

#endif

SQUnsignedInteger TranslateIndex(const SQObjectPtr &idx);
const SQChar *GetTypeName(const SQObjectPtr &obj1);
const SQChar *IdType2Name(SQObjectType type);



#endif //_SQOBJECT_H_
