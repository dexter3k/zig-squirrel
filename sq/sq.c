/*  see copyright notice in squirrel.h */

#include <stdio.h>
#include <stdarg.h>

#include <squirrel.h>

void printfunc(HSQUIRRELVM vm, const SQChar *s, ...) {
    (void)vm;

    va_list vl;
    va_start(vl, s);
    vfprintf(stdout, s, vl);
    va_end(vl);
}

void errorfunc(HSQUIRRELVM vm, const SQChar *s, ...) {
    (void)vm;

    va_list vl;
    va_start(vl, s);
    vfprintf(stderr, s, vl);
    va_end(vl);
}