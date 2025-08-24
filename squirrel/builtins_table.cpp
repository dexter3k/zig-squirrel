#include "builtins.hpp"

#include "SQArray.hpp"
#include "SQTable.hpp"
#include "SQVM.hpp"

static SQInteger table_rawdelete(HSQUIRRELVM vm) {
    if (SQ_FAILED(sq_rawdeleteslot(vm, 1, SQTrue))) {
        return SQ_ERROR;
    }
    return 1;
}

static SQInteger table_setdelegate(HSQUIRRELVM vm) {
    if (SQ_FAILED(sq_setdelegate(vm, -2))) {
        return SQ_ERROR;
    }
    sq_push(vm, -1); // -1 because sq_setdelegate pops 1
    return 1;
}

static SQInteger table_getdelegate(HSQUIRRELVM vm) {
    return SQ_SUCCEEDED(sq_getdelegate(vm, -1))
        ? 1
        : SQ_ERROR;
}

static SQInteger table_filter(HSQUIRRELVM vm) {
    SQObject & o = stack_get(vm, 1);
    SQTable * tbl = _table(o);
    SQObjectPtr ret = SQTable::Create(vm->_sharedstate, 0);

    SQObjectPtr itr, key, val;
    SQInteger nitr;
    while((nitr = tbl->Next(false, itr, key, val)) != -1) {
        itr = (SQInteger)nitr;

        vm->Push(o);
        vm->Push(key);
        vm->Push(val);
        if (SQ_FAILED(sq_call(vm, 3, SQTrue, SQFalse))) {
            return SQ_ERROR;
        }

        if (!SQVM::IsFalse(vm->GetUp(-1))) {
            _table(ret)->NewSlot(key, val);
        }
        vm->Pop();
    }

    vm->Push(ret);
    return 1;
}

static SQInteger table_map(HSQUIRRELVM v)
{
    SQObject &o = stack_get(v, 1);
    SQTable *tbl = _table(o);
    SQInteger nitr, n = 0;
    SQObjectPtr ret = SQArray::Create(_ss(v), tbl->CountUsed());
    SQObjectPtr itr, key, val;
    while ((nitr = tbl->Next(false, itr, key, val)) != -1) {
        itr = (SQInteger)nitr;

        v->Push(o);
        v->Push(key);
        v->Push(val);
        if (SQ_FAILED(sq_call(v, 3, SQTrue, SQFalse))) {
            return SQ_ERROR;
        }
        _array(ret)->Set(n, v->GetUp(-1));
        v->Pop();
        n++;
    }

    v->Push(ret);
    return 1;
}

#define TABLE_TO_ARRAY_FUNC(_funcname_,_valname_) static SQInteger _funcname_(HSQUIRRELVM v) \
{ \
    SQObject &o = stack_get(v, 1); \
    SQTable *t = _table(o); \
    SQObjectPtr itr, key, val; \
    SQObjectPtr _null; \
    SQInteger nitr, n = 0; \
    size_t nitems = t->CountUsed(); \
    SQArray *a = SQArray::Create(_ss(v), nitems); \
    a->Resize(nitems, _null); \
    if (nitems) { \
        while ((nitr = t->Next(false, itr, key, val)) != -1) { \
            itr = (SQInteger)nitr; \
            a->Set(n, _valname_); \
            n++; \
        } \
    } \
    v->Push(a); \
    return 1; \
}

TABLE_TO_ARRAY_FUNC(table_keys, key)
TABLE_TO_ARRAY_FUNC(table_values, val)

const SQRegFunction SQSharedState::_table_default_delegate_funcz[] = {
    {"len",         default_delegate_len,      1, "t"},
    {"rawget",      container_rawget,          2, "t"},
    {"rawset",      container_rawset,          3, "t"},
    {"rawdelete",   table_rawdelete,           2, "t"},
    {"rawin",       container_rawexists,       2, "t"},
    {"weakref",     obj_delegate_weakref,      1, nullptr},
    {"tostring",    default_delegate_tostring, 1, "."},
    {"clear",       obj_clear,                 1, "."},
    {"setdelegate", table_setdelegate,         2, ".t|o"},
    {"getdelegate", table_getdelegate,         1, "."},
    {"filter",      table_filter,              2, "tc"},
	{"map",         table_map,                 2, "tc"},
	{"keys",        table_keys,                1, "t"},
	{"values",      table_values,              1, "t"},
    {nullptr, nullptr, 0, nullptr}
};
