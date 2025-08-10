/*  see copyright notice in squirrel.h */
#ifndef _SQOBJECT_H_
#define _SQOBJECT_H_

#include <cassert>

#include <squirrel.h>

#include "squtils.h"

#ifdef _SQ64
#define UINT_MINUS_ONE (0xFFFFFFFFFFFFFFFF)
#else
#define UINT_MINUS_ONE (0xFFFFFFFF)
#endif

#define SQ_CLOSURESTREAM_HEAD (('S'<<24)|('Q'<<16)|('I'<<8)|('R'))
#define SQ_CLOSURESTREAM_PART (('P'<<24)|('A'<<16)|('R'<<8)|('T'))
#define SQ_CLOSURESTREAM_TAIL (('T'<<24)|('A'<<16)|('I'<<8)|('L'))

struct SQSharedState;

enum SQMetaMethod{
    MT_ADD=0,
    MT_SUB=1,
    MT_MUL=2,
    MT_DIV=3,
    MT_UNM=4,
    MT_MODULO=5,
    MT_SET=6,
    MT_GET=7,
    MT_TYPEOF=8,
    MT_NEXTI=9,
    MT_CMP=10,
    MT_CALL=11,
    MT_CLONED=12,
    MT_NEWSLOT=13,
    MT_DELSLOT=14,
    MT_TOSTRING=15,
    MT_NEWMEMBER=16,
    MT_INHERITED=17,
    MT_LAST = 18
};

#define MM_ADD      _SC("_add")
#define MM_SUB      _SC("_sub")
#define MM_MUL      _SC("_mul")
#define MM_DIV      _SC("_div")
#define MM_UNM      _SC("_unm")
#define MM_MODULO   _SC("_modulo")
#define MM_SET      _SC("_set")
#define MM_GET      _SC("_get")
#define MM_TYPEOF   _SC("_typeof")
#define MM_NEXTI    _SC("_nexti")
#define MM_CMP      _SC("_cmp")
#define MM_CALL     _SC("_call")
#define MM_CLONED   _SC("_cloned")
#define MM_NEWSLOT  _SC("_newslot")
#define MM_DELSLOT  _SC("_delslot")
#define MM_TOSTRING _SC("_tostring")
#define MM_NEWMEMBER _SC("_newmember")
#define MM_INHERITED _SC("_inherited")


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

    virtual ~SQRefCounted();

    SQWeakRef * GetWeakRef(SQObjectType type);

    // User must implement release to
    // 1. Call destructor and release others
    //  - this is important, because this invalidates weakrefs!
    // 2. Free memory for itself
    virtual void Release() = 0;
};

struct SQWeakRef : SQRefCounted {
    SQObject _obj;

    void Release() {
        // I mean yeah, but also..
        // how do we have a weakref to non-refcounted?
        // * it can become nullptr
        if (ISREFCOUNTED(_obj._type)) {
            _obj._unVal.pRefCounted->_weakref = nullptr;
        }

        this->~SQWeakRef();
        sq_vm_free(this, sizeof(SQWeakRef));
    }
};

#define _realval(o) (sq_type((o)) != OT_WEAKREF?(SQObject)o:_weakref(o)->_obj)

struct SQObjectPtr;

static inline void __Release(SQObjectType type, SQObjectValue unval) {
    if (ISREFCOUNTED(type)) {
        unval.pRefCounted->_uiRef--;
        if (unval.pRefCounted->_uiRef == 0) {
            unval.pRefCounted->Release();
        }
    }
}

static inline void __ObjRelease(SQRefCounted * rc) {
    if (rc) {
        rc->_uiRef--;
        if (rc->_uiRef == 0) {
            rc->Release();
        }
    }
}

static inline void __ObjAddRef(SQRefCounted * rc) {
    rc->_uiRef++;
}

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

#define _REF_TYPE_DECL(type,_class,sym) \
    SQObjectPtr(_class * x) \
    { \
        _unVal.raw = 0; \
        _type=type; \
        _unVal.sym = x; \
        assert(_unVal.sym); \
        _unVal.pRefCounted->_uiRef++; \
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
        _unVal.pRefCounted->_uiRef++; \
        __Release(tOldType,unOldVal); \
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
        __Release(_type,_unVal); \
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
            _unVal.pRefCounted->_uiRef++;
        }
    }

    SQObjectPtr(SQObject const & o) {
        _type = o._type;
        _unVal = o._unVal;
        if (ISREFCOUNTED(_type)) {
            _unVal.pRefCounted->_uiRef++;
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
        _unVal.nInteger = bBool?1:0;
    }

    ~SQObjectPtr() {
        __Release(_type, _unVal);
    }

    inline void Null() {
        if (ISREFCOUNTED(_type)) {
            _unVal.pRefCounted->_uiRef--;
            if (_unVal.pRefCounted->_uiRef == 0) {
                _unVal.pRefCounted->Release();
            }
        }
        _type = OT_NULL;
        _unVal.raw = 0;
    }

    inline SQObjectPtr & operator=(bool b) {
        if (ISREFCOUNTED(_type)) {
            _unVal.pRefCounted->_uiRef--;
            if (_unVal.pRefCounted->_uiRef == 0) {
                _unVal.pRefCounted->Release();
            }
        }

        _type = OT_BOOL;
        _unVal.nInteger = b?1:0;

        return *this;
    }

    inline SQObjectPtr& operator=(const SQObjectPtr& obj)
    {
        SQObjectType tOldType;
        SQObjectValue unOldVal;
        tOldType=_type;
        unOldVal=_unVal;
        _unVal = obj._unVal;
        _type = obj._type;
        if (ISREFCOUNTED(_type)) {
            _unVal.pRefCounted->_uiRef++;
        }
        if (ISREFCOUNTED(tOldType)) {
            unOldVal.pRefCounted->_uiRef--;
            if (unOldVal.pRefCounted->_uiRef == 0) {
                unOldVal.pRefCounted->Release();
            }
        }
        return *this;
    }

    inline SQObjectPtr& operator=(const SQObject& obj)
    {
        SQObjectType tOldType;
        SQObjectValue unOldVal;
        tOldType=_type;
        unOldVal=_unVal;
        _unVal = obj._unVal;
        _type = obj._type;
        if (ISREFCOUNTED(_type)) {
            _unVal.pRefCounted->_uiRef++;
        }
        if (ISREFCOUNTED(tOldType)) {
            unOldVal.pRefCounted->_uiRef--;
            if (unOldVal.pRefCounted->_uiRef == 0) {
                unOldVal.pRefCounted->Release();
            }
        }
        return *this;
    }

    private:
        SQObjectPtr(const SQChar *) = delete;
};


inline void _Swap(SQObject &a,SQObject &b)
{
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

#define END_MARK()       RemoveFromChain(&_sharedstate->_gc_chain, this); \
                         AddToChain(chain, this); \
                     }

struct SQCollectable : public SQRefCounted {
    SQCollectable *_next;
    SQCollectable *_prev;
    SQSharedState *_sharedstate;
    virtual SQObjectType GetType() = 0;
    virtual void Release() = 0;
    virtual void Mark(SQCollectable **chain) = 0;
    void UnMark();
    virtual void Finalize()=0;
    static void AddToChain(SQCollectable **chain,SQCollectable *c);
    static void RemoveFromChain(SQCollectable **chain,SQCollectable *c);
};


#define ADD_TO_CHAIN(chain,obj) AddToChain(chain,obj)
#define REMOVE_FROM_CHAIN(chain,obj) {if(!(_uiRef&MARK_FLAG))RemoveFromChain(chain,obj);}
#define CHAINABLE_OBJ SQCollectable
#define INIT_CHAIN() {_next=nullptr;_prev=nullptr;_sharedstate=ss;}
#else

#define ADD_TO_CHAIN(chain,obj) ((void)0)
#define REMOVE_FROM_CHAIN(chain, obj) ((void)0)
#define CHAINABLE_OBJ SQRefCounted
#define INIT_CHAIN() ((void)0)
#endif

SQUnsignedInteger TranslateIndex(const SQObjectPtr &idx);
const SQChar *GetTypeName(const SQObjectPtr &obj1);
const SQChar *IdType2Name(SQObjectType type);



#endif //_SQOBJECT_H_
