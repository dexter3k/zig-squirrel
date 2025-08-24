#pragma once

#include <squirrel.h>
#include "sqstate.h"

SQInteger default_delegate_len(HSQUIRRELVM vm);
SQInteger container_rawget(HSQUIRRELVM vm);
SQInteger container_rawset(HSQUIRRELVM vm);
SQInteger container_rawexists(HSQUIRRELVM vm);
SQInteger obj_delegate_weakref(HSQUIRRELVM vm);
SQInteger default_delegate_tostring(HSQUIRRELVM vm);
SQInteger obj_clear(HSQUIRRELVM vm);
