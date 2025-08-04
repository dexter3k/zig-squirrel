/*  see copyright notice in squirrel.h */
#ifndef _SQCOMPILER_H_
#define _SQCOMPILER_H_

struct SQVM;

typedef void(*CompilerErrorFunc)(void *ud, const SQChar *s);
bool Compile(SQVM *vm, SQLEXREADFUNC rg, SQUserPointer up, const SQChar *sourcename, SQObjectPtr &out, bool raiseerror, bool lineinfo);
#endif //_SQCOMPILER_H_
