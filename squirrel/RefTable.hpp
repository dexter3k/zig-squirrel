#pragma once

#include "SQCollectable.hpp"
#include "sqobject.h"

struct RefTable {
private:
    struct RefNode {
        SQObjectPtr obj;
        SQUnsignedInteger refs;
        struct RefNode * next;
    };

    SQUnsignedInteger _numofslots;
    SQUnsignedInteger _slotused;
    RefNode *_nodes;
    RefNode *_freelist;
    RefNode **_buckets;
public:
    RefTable();
    ~RefTable();

    void AddRef(SQObject &obj);

    SQBool Release(SQObject &obj);

    SQUnsignedInteger GetRefCount(SQObject &obj);

#ifndef NO_GARBAGE_COLLECTOR
    void Mark(SQCollectable **chain);
#endif

    void Finalize();
private:
    RefNode *Get(SQObject &obj,SQHash &mainpos,RefNode **prev,bool add);
    RefNode *Add(SQHash mainpos,SQObject &obj);
    void Resize(SQUnsignedInteger size);
    void AllocNodes(SQUnsignedInteger size);
};
