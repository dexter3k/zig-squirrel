#include "builtins.hpp"

#include "SQVM.hpp"

SQInteger default_delegate_len(HSQUIRRELVM vm) {
    vm->Push(sq_getsize(vm, 1));
    return 1;
}

SQInteger container_rawget(HSQUIRRELVM vm) {
    return SQ_SUCCEEDED(sq_rawget(vm, -2))
        ? 1
        : SQ_ERROR;
}

SQInteger container_rawset(HSQUIRRELVM vm) {
    return SQ_SUCCEEDED(sq_rawset(vm, -3))
        ? 1
        : SQ_ERROR;
}

SQInteger container_rawexists(HSQUIRRELVM vm) {
    if (SQ_SUCCEEDED(sq_rawget(vm, -2))) {
        sq_pushbool(vm, SQTrue);
        return 1;
    }
    sq_pushbool(vm, SQFalse);
    return 1;
}

SQInteger obj_delegate_weakref(HSQUIRRELVM vm) {
    sq_weakref(vm, 1);
    return 1;
}

SQInteger default_delegate_tostring(HSQUIRRELVM vm) {
    return SQ_SUCCEEDED(sq_tostring(vm, 1))
        ? 1
        : SQ_ERROR;
}

SQInteger obj_clear(HSQUIRRELVM vm) {
    return SQ_SUCCEEDED(sq_clear(vm, -1))
        ? 1
        : SQ_ERROR;
}
