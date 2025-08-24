#pragma once

#include <squirrel.h>

#include "sqobject.h"

struct SQVM;

typedef void(*CompilerErrorFunc)(void *ud, const SQChar *s);

bool Compile(
	SQVM *vm,
	SQLEXREADFUNC rg,
	SQUserPointer up,
	const SQChar *sourcename,
	SQObjectPtr &out,
	bool raiseerror,
	bool lineinfo
);
