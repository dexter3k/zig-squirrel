#pragma once

#include <stdint.h>

#ifdef _SQ64
typedef int64_t SQInteger;
typedef uint64_t SQUnsignedInteger;
#else
typedef int32_t SQInteger;
typedef uint32_t SQUnsignedInteger;
#endif

typedef uintptr_t SQHash; // todo: _SQ64 on 32-bit machines?
typedef int32_t SQInt32;
typedef uint32_t SQUnsignedInteger32;

#ifdef SQUSEDOUBLE
typedef double SQFloat;
#else
typedef float SQFloat;
#endif

#if defined(SQUSEDOUBLE) && !defined(_SQ64) || !defined(SQUSEDOUBLE) && defined(_SQ64)
typedef int64_t SQRawObjectVal;
#define SQ_OBJECT_RAWINIT() { _unVal.raw = 0; }
#else
typedef SQUnsignedInteger SQRawObjectVal; //is 32 bits on 32 bits builds and 64 bits otherwise
#define SQ_OBJECT_RAWINIT()
#endif

// This is likely wrong?
#ifndef SQ_ALIGNMENT // SQ_ALIGNMENT shall be less than or equal to SQ_MALLOC alignments, and its value shall be power of 2.
#if defined(SQUSEDOUBLE) || defined(_SQ64)
#define SQ_ALIGNMENT 8
#else
#define SQ_ALIGNMENT 4
#endif
#endif

typedef void* SQUserPointer;
typedef SQUnsignedInteger SQBool;
typedef SQInteger SQRESULT;

typedef char SQChar;

#define _SC(a) a
#define scstrcmp    strcmp
#define scsprintf   snprintf
#define scstrlen    strlen
#define scstrtod    strtod

#ifdef _SQ64
#define scstrtol    strtoll
#else
#define scstrtol    strtol
#endif

#define scstrtoul   strtoul
#define scvsprintf  vsnprintf
#define scstrstr    strstr
#define scisspace   isspace
#define scisdigit   isdigit
#define scisprint   isprint
#define scisxdigit  isxdigit
#define sciscntrl   iscntrl
#define scisalpha   isalpha
#define scisalnum   isalnum
#define scprintf    printf
#define MAX_CHAR 0xFF
#define sq_rsl(l) (l)

#ifdef _SQ64
#define _PRINT_INT_PREC _SC("ll")
#define _PRINT_INT_FMT _SC("%lld")
#else
#define _PRINT_INT_FMT _SC("%d")
#endif
