#pragma once

#include <cstring>

#include "squirrel.h"
#include "squtils.h"

struct SQString;
struct SQSharedState;

class SQStringTable {
public:
    SQString ** strings;
    SQUnsignedInteger _numofslots;
    SQUnsignedInteger _slotused;
    SQSharedState * _sharedstate;

    SQStringTable(SQSharedState * ss)
        : strings(nullptr)
        , _numofslots(0)
        , _slotused(0)
        , _sharedstate(ss)
    {
        AllocNodes(128);
    }

    ~SQStringTable() {
        sq_vm_free(strings, sizeof(SQString *) * _numofslots);
    }

    SQString *Add(const SQChar *,SQInteger len);
    SQString* Concat(const SQChar* a, SQInteger alen, const SQChar* b, SQInteger blen);

    void Remove(SQString const *);
private:
    void Resize(SQInteger size);

    void AllocNodes(SQInteger size) {
        _numofslots = size;
        strings = (SQString **)sq_vm_malloc(sizeof(SQString *) * _numofslots);
        memset(strings, 0, sizeof(SQString *) * _numofslots);
    }
};
