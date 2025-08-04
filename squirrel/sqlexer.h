#pragma once

#include "strbuf.h"
#include "squirrel.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#define TK_IDENTIFIER   258
#define TK_STRING_LITERAL   259
#define TK_INTEGER  260
#define TK_FLOAT    261
#define TK_BASE 262
#define TK_DELETE   263
#define TK_EQ   264
#define TK_NE   265
#define TK_LE   266
#define TK_GE   267
#define TK_SWITCH   268
#define TK_ARROW    269
#define TK_AND  270
#define TK_OR   271
#define TK_IF   272
#define TK_ELSE 273
#define TK_WHILE    274
#define TK_BREAK    275
#define TK_FOR  276
#define TK_DO   277
#define TK_NULL 278
#define TK_FOREACH  279
#define TK_IN   280
#define TK_NEWSLOT  281
#define TK_MODULO   282
#define TK_LOCAL    283
#define TK_CLONE    284
#define TK_FUNCTION 285
#define TK_RETURN   286
#define TK_TYPEOF   287
#define TK_UMINUS   288
#define TK_PLUSEQ   289
#define TK_MINUSEQ  290
#define TK_CONTINUE 291
#define TK_YIELD 292
#define TK_TRY 293
#define TK_CATCH 294
#define TK_THROW 295
#define TK_SHIFTL 296
#define TK_SHIFTR 297
#define TK_RESUME 298
#define TK_DOUBLE_COLON 299
#define TK_CASE 300
#define TK_DEFAULT 301
#define TK_THIS 302
#define TK_PLUSPLUS 303
#define TK_MINUSMINUS 304
#define TK_3WAYSCMP 305
#define TK_USHIFTR 306
#define TK_CLASS 307
#define TK_EXTENDS 308
#define TK_CONSTRUCTOR 310
#define TK_INSTANCEOF 311
#define TK_VARPARAMS 312
#define TK___LINE__ 313
#define TK___FILE__ 314
#define TK_TRUE 315
#define TK_FALSE 316
#define TK_MULEQ 317
#define TK_DIVEQ 318
#define TK_MODEQ 319
#define TK_ATTR_OPEN 320
#define TK_ATTR_CLOSE 321
#define TK_STATIC 322
#define TK_ENUM 323
#define TK_CONST 324
#define TK_RAWCALL 325

extern char const * token_to_string(uint16_t token);

typedef struct {
    uint16_t token;
    uint16_t prev_token;
    uint32_t current_line;
    uint32_t current_column;
    uint32_t last_token_line;

    SQUnsignedInteger uint_value;
    SQFloat float_value;
    char const * string_value;
    Strbuf string_buffer; // replace with length value?
} LexerState;

typedef void(*CompilerErrorFunc)(void *ud, const SQChar *s);

typedef struct {
    LexerState state;

    SQLEXREADFUNC reader_func;
    void * reader_context;
    CompilerErrorFunc error_func;
    void *error_context;

    uint16_t _curtoken;
    bool _reached_eof;
    uint8_t _currdata;
} SQLexer;

extern void lexer_deinit(SQLexer * lexer);

#ifdef __cplusplus
}

extern void lexer_init(SQLexer * lexer, SQLEXREADFUNC rg, void * up, CompilerErrorFunc efunc, void *ed);
extern LexerState * lexer_lex(SQLexer * lexer);

#endif
