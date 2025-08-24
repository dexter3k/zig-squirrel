#pragma once

#include "strtab.h"

#include "SQCollectable.hpp"

class GC {
public:
#ifndef NO_GARBAGE_COLLECTOR
    static void MarkObject(SQObjectPtr & o, SQCollectable ** chain);
#endif

private:
    SQStringTable string_table;

public:
#ifndef NO_GARBAGE_COLLECTOR
    SQCollectable * chain_root;
#endif

    GC();
    ~GC();

    SQString * AddString(char const * string, size_t length);
    SQString * ConcatStrings(char const * string_a, size_t length_a, char const * string_b, size_t length_b);
};
