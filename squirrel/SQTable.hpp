/*  see copyright notice in squirrel.h */
#ifndef _SQTABLE_H_
#define _SQTABLE_H_
/*
* The following code is based on Lua 4.0 (Copyright 1994-2002 Tecgraf, PUC-Rio.)
* http://www.lua.org/copyright.html#4
* http://www.lua.org/source/4.0.1/src_ltable.c.html
*/

#include <algorithm>

#include "SQString.hpp"
#include "sqstate.h"
#include "SQDelegable.hpp"

#define hashptr(p)  ((SQHash)(((SQInteger)p) >> 3))

inline SQHash HashObj(SQObject const & key) {
    switch (sq_type(key)) {
    case OT_STRING:
        return _string(key)->_hash;
    case OT_FLOAT:
        return (SQHash)((SQInteger)_float(key));
    case OT_BOOL:
    case OT_INTEGER:
        return (SQHash)((SQInteger)_integer(key));
    default:
        return hashptr(key._unVal.pRefCounted);
    }
}

struct SQTable : public SQDelegable {
private:
    struct _HashNode {
        SQObjectPtr val;
        SQObjectPtr key;
        _HashNode * next;

        _HashNode()
            : val()
            , key()
            , next(nullptr)
        {}
    };

    _HashNode * _firstfree;
    _HashNode * _nodes;
    size_t _numofnodes;
    size_t _usednodes;

    SQTable(SQSharedState * ss, size_t nInitialSize);

    ~SQTable() {
        SetDelegate(nullptr);
        for (size_t i = 0; i < _numofnodes; i++) {
            _nodes[i].~_HashNode();
        }
        sq_vm_free(_nodes, _numofnodes * sizeof(_HashNode));
    }

    void AllocNodes(size_t nSize);
    void Rehash(bool force);
    void _ClearNodes();
public:
    static SQTable * Create(SQSharedState * ss, SQInteger nInitialSize) {
        auto table = (SQTable *)sq_vm_malloc(sizeof(SQTable));
        new (table) SQTable(ss, std::max(0ll, nInitialSize));
        return table;
    }

    void Release() {
        this->~SQTable();
        sq_vm_free(this, sizeof(*this));
    }

    void Finalize();

    SQTable *Clone();

#ifndef NO_GARBAGE_COLLECTOR
    void Mark(SQCollectable **chain);
    SQObjectType GetType() { return OT_TABLE; }
#endif

    bool Get(const SQObjectPtr &key,SQObjectPtr &val);
    void Remove(const SQObjectPtr &key);
    bool Set(const SQObjectPtr &key, const SQObjectPtr &val);
    //returns true if a new slot has been created false if it was already present
    bool NewSlot(const SQObjectPtr &key,const SQObjectPtr &val);
    SQInteger Next(bool getweakrefs,const SQObjectPtr &refpos, SQObjectPtr &outkey, SQObjectPtr &outval);

    size_t CountUsed() {
        return _usednodes;
    }
    void Clear();
private:
    inline _HashNode * _Get(SQObjectPtr const & key, SQHash hash) {
        _HashNode * n = &_nodes[hash];

        do {
            if (_rawval(n->key) != _rawval(key)) {
                continue;
            }

            if (sq_type(n->key) == sq_type(key)) {
                return n;
            }
        } while ((n = n->next));

        return NULL;
    }
};

#endif //_SQTABLE_H_
