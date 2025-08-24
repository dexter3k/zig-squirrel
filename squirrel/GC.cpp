#include "GC.hpp"

#include "SQArray.hpp"
#include "SQClass.hpp"
#include "SQClosure.hpp"
#include "SQFunctionProto.hpp"
#include "SQGenerator.hpp"
#include "SQInstance.hpp"
#include "SQNativeClosure.hpp"
#include "SQOuter.hpp"
#include "SQTable.hpp"
#include "SQUserData.hpp"
#include "SQVM.hpp"

#ifndef NO_GARBAGE_COLLECTOR
void GC::MarkObject(SQObjectPtr & o, SQCollectable ** chain) {
    switch (sq_type(o)) {
    case OT_TABLE:
        _table(o)->Mark(chain);
        break;
    case OT_ARRAY:
        _array(o)->Mark(chain);
        break;
    case OT_USERDATA:
        _userdata(o)->Mark(chain);
        break;
    case OT_CLOSURE:
        _closure(o)->Mark(chain);
        break;
    case OT_NATIVECLOSURE:
        _nativeclosure(o)->Mark(chain);
        break;
    case OT_GENERATOR:
        _generator(o)->Mark(chain);
        break;
    case OT_THREAD:
        _thread(o)->Mark(chain);
        break;
    case OT_CLASS:
        _class(o)->Mark(chain);
        break;
    case OT_INSTANCE:
        _instance(o)->Mark(chain);
        break;
    case OT_OUTER:
        _outer(o)->Mark(chain);
        break;
    case OT_FUNCPROTO:
        _funcproto(o)->Mark(chain);
        break;
    case OT_NULL:
    case OT_BOOL:
    case OT_INTEGER:
    case OT_FLOAT:
    case OT_STRING:
    case OT_WEAKREF:
    case OT_USERPOINTER:
        break;
    }
}
#endif

GC::GC()
    : string_table()
#ifndef NO_GARBAGE_COLLECTOR
    , chain_root(nullptr)
#endif
{}

GC::~GC() {
}

SQString * GC::AddString(char const * string, size_t length) {
    return this->string_table.Add(string, length);
}

SQString * GC::ConcatStrings(char const * string_a, size_t length_a, char const * string_b, size_t length_b) {
    return this->string_table.Concat(string_a, length_a, string_b, length_b);
}
