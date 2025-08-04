#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <squirrel.h>

#include "sqlexer.h"

SQInteger lexer_get_id_type(char const * s, SQInteger len) {
    switch (len) {
    case 2:
        if (0 == strcmp(s, "do")) return TK_DO;
        if (0 == strcmp(s, "if")) return TK_IF;
        if (0 == strcmp(s, "in")) return TK_IN;
        break;
    case 3:
        if (0 == strcmp(s, "for")) return TK_FOR;
        if (0 == strcmp(s, "try")) return TK_TRY;
        break;
    case 4:
        if (0 == strcmp(s, "base")) return TK_BASE;
        if (0 == strcmp(s, "case")) return TK_CASE;
        if (0 == strcmp(s, "else")) return TK_ELSE;
        if (0 == strcmp(s, "enum")) return TK_ENUM;
        if (0 == strcmp(s, "null")) return TK_NULL;
        if (0 == strcmp(s, "this")) return TK_THIS;
        if (0 == strcmp(s, "true")) return TK_TRUE;
        break;
    case 5:
        if (0 == strcmp(s, "break")) return TK_BREAK;
        if (0 == strcmp(s, "catch")) return TK_CATCH;
        if (0 == strcmp(s, "class")) return TK_CLASS;
        if (0 == strcmp(s, "clone")) return TK_CLONE;
        if (0 == strcmp(s, "const")) return TK_CONST;
        if (0 == strcmp(s, "false")) return TK_FALSE;
        if (0 == strcmp(s, "local")) return TK_LOCAL;
        if (0 == strcmp(s, "throw")) return TK_THROW;
        if (0 == strcmp(s, "while")) return TK_WHILE;
        if (0 == strcmp(s, "yield")) return TK_YIELD;
        break;
    case 6:
        if (0 == strcmp(s, "delete")) return TK_DELETE;
        if (0 == strcmp(s, "resume")) return TK_RESUME;
        if (0 == strcmp(s, "return")) return TK_RETURN;
        if (0 == strcmp(s, "static")) return TK_STATIC;
        if (0 == strcmp(s, "switch")) return TK_SWITCH;
        if (0 == strcmp(s, "typeof")) return TK_TYPEOF;
        break;
    case 7:
        if (0 == strcmp(s, "default")) return TK_DEFAULT;
        if (0 == strcmp(s, "extends")) return TK_EXTENDS;
        if (0 == strcmp(s, "foreach")) return TK_FOREACH;
        if (0 == strcmp(s, "rawcall")) return TK_RAWCALL;
        break;
    case 8:
        if (0 == strcmp(s, "__FILE__")) return TK___FILE__;
        if (0 == strcmp(s, "__LINE__")) return TK___LINE__;
        if (0 == strcmp(s, "continue")) return TK_CONTINUE;
        if (0 == strcmp(s, "function")) return TK_FUNCTION;
        break;
    case 10:
        if (0 == strcmp(s, "instanceof")) return TK_INSTANCEOF;
        break;
    case 11:
        if (0 == strcmp(s, "constructor")) return TK_CONSTRUCTOR;
        break;
    default:
        break;
    }

    return TK_IDENTIFIER;
}

void lexer_error(SQLexer * lexer, char const * err) {
    lexer->error_func(lexer->error_context, err);
}

void lexer_next(SQLexer * lexer) {
    SQInteger t = lexer->reader_func(lexer->reader_context);
    if (t > MAX_CHAR) {
        lexer_error(lexer, "Invalid character");
    }

    lexer->state.current_column++;

    if (t == 0) {
        lexer->_currdata = SQUIRREL_EOB;
        lexer->_reached_eof = SQTrue;
        return;
    }

    lexer->_currdata = t;
}

void lexer_add_utf8(SQLexer * lexer, uint32_t ch) {
    if (ch < 0x80) {
        strbuf_push_back(&lexer->state.string_buffer, char(ch));
    } else if (ch < 0x800) {
        strbuf_push_back(&lexer->state.string_buffer, (SQChar)((ch >> 6) | 0xC0));
        strbuf_push_back(&lexer->state.string_buffer, (SQChar)((ch & 0x3F) | 0x80));
    } else if (ch < 0x10000) {
        strbuf_push_back(&lexer->state.string_buffer, (SQChar)((ch >> 12) | 0xE0));
        strbuf_push_back(&lexer->state.string_buffer, (SQChar)(((ch >> 6) & 0x3F) | 0x80));
        strbuf_push_back(&lexer->state.string_buffer, (SQChar)((ch & 0x3F) | 0x80));
    } else if (ch < 0x110000) {
        strbuf_push_back(&lexer->state.string_buffer, (SQChar)((ch >> 18) | 0xF0));
        strbuf_push_back(&lexer->state.string_buffer, (SQChar)(((ch >> 12) & 0x3F) | 0x80));
        strbuf_push_back(&lexer->state.string_buffer, (SQChar)(((ch >> 6) & 0x3F) | 0x80));
        strbuf_push_back(&lexer->state.string_buffer, (SQChar)((ch & 0x3F) | 0x80));
    }
}

void lexer_process_string_hex_escape(SQLexer * lexer, char * dest, SQInteger maxdigits) {
    lexer_next(lexer);

    if (!isxdigit(lexer->_currdata)) {
        lexer_error(lexer, "hexadecimal number expected");
        // noreturn
    }

    SQInteger n = 0;
    while (isxdigit(lexer->_currdata) && n < maxdigits) {
        dest[n] = lexer->_currdata;
        n++;
        lexer_next(lexer);
    }
    dest[n] = 0;
}

SQInteger lexer_read_string(SQLexer * lexer, SQInteger ndelim, bool verbatim) {
    strbuf_reset(&lexer->state.string_buffer);
    lexer_next(lexer);

    if (lexer->_currdata <= SQUIRREL_EOB) {
        return -1;
    }

    for(;;) {
        while(lexer->_currdata != ndelim) {
            SQInteger x = lexer->_currdata;
            switch (x) {
            case SQUIRREL_EOB:
                lexer_error(lexer, "unfinished string");
                // noreturn
                return -1;
            case '\n':
                if(!verbatim) {
                    lexer_error(lexer, "newline in a constant");
                    // noreturn
                }
                strbuf_push_back(&lexer->state.string_buffer, lexer->_currdata);
                lexer_next(lexer);
                lexer->state.current_line++;
                break;
            case '\\':
                if(verbatim) {
                    strbuf_push_back(&lexer->state.string_buffer, '\\');
                    lexer_next(lexer);
                }
                else {
                    lexer_next(lexer);
                    switch(lexer->_currdata) {
                    case _SC('x'):  {
                        const SQInteger maxdigits = sizeof(SQChar) * 2;
                        SQChar temp[maxdigits + 1];
                        lexer_process_string_hex_escape(lexer, temp, maxdigits);
                        strbuf_push_back(&lexer->state.string_buffer, (SQChar)strtoul(temp, nullptr, 16));
                    }
                    break;
                    case _SC('U'):
                    case _SC('u'):  {
                        const SQInteger maxdigits = lexer->_currdata == 'u' ? 4 : 8;
                        SQChar temp[8 + 1];
                        lexer_process_string_hex_escape(lexer, temp, maxdigits);
                        lexer_add_utf8(lexer, strtoul(temp, nullptr, 16));
                    }
                    break;
                    case _SC('t'): strbuf_push_back(&lexer->state.string_buffer, _SC('\t')); lexer_next(lexer); break;
                    case _SC('a'): strbuf_push_back(&lexer->state.string_buffer, _SC('\a')); lexer_next(lexer); break;
                    case _SC('b'): strbuf_push_back(&lexer->state.string_buffer, _SC('\b')); lexer_next(lexer); break;
                    case _SC('n'): strbuf_push_back(&lexer->state.string_buffer, _SC('\n')); lexer_next(lexer); break;
                    case _SC('r'): strbuf_push_back(&lexer->state.string_buffer, _SC('\r')); lexer_next(lexer); break;
                    case _SC('v'): strbuf_push_back(&lexer->state.string_buffer, _SC('\v')); lexer_next(lexer); break;
                    case _SC('f'): strbuf_push_back(&lexer->state.string_buffer, _SC('\f')); lexer_next(lexer); break;
                    case _SC('0'): strbuf_push_back(&lexer->state.string_buffer, _SC('\0')); lexer_next(lexer); break;
                    case _SC('\\'): strbuf_push_back(&lexer->state.string_buffer, _SC('\\')); lexer_next(lexer); break;
                    case _SC('"'): strbuf_push_back(&lexer->state.string_buffer, _SC('"')); lexer_next(lexer); break;
                    case _SC('\''): strbuf_push_back(&lexer->state.string_buffer, _SC('\'')); lexer_next(lexer); break;
                    default:
                        lexer_error(lexer, _SC("unrecognised escaper char"));
                        break;
                    }
                }
                break;
            default:
                strbuf_push_back(&lexer->state.string_buffer, lexer->_currdata);
                lexer_next(lexer);
                break;
            }
        }
        lexer_next(lexer);
        if(verbatim && lexer->_currdata == '"') { //double quotation
            strbuf_push_back(&lexer->state.string_buffer, lexer->_currdata);
            lexer_next(lexer);
        }
        else {
            break;
        }
    }
    strbuf_push_back(&lexer->state.string_buffer, '\0');
    SQInteger len = lexer->state.string_buffer.len-1;
    if(ndelim == _SC('\'')) {
        if(len == 0) lexer_error(lexer, _SC("empty constant"));
        if(len > 1) lexer_error(lexer, _SC("constant too long"));
        lexer->state.uint_value = SQUnsignedInteger(SQInteger(lexer->state.string_buffer.data[0])); // sign-extend the value!
        return TK_INTEGER;
    }
    lexer->state.string_value = reinterpret_cast<char const *>(&lexer->state.string_buffer.data[0]);
    return TK_STRING_LITERAL;
}

void LexHexadecimal(const SQChar *s,SQUnsignedInteger *res)
{
    *res = 0;
    while(*s != 0)
    {
        if(scisdigit(*s)) *res = (*res)*16+((*s++)-'0');
        else if(scisxdigit(*s)) *res = (*res)*16+(toupper(*s++)-'A'+10);
        else { assert(0); }
    }
}

void LexInteger(SQChar const * s, SQUnsignedInteger * res) {
    *res = 0;

    while(*s != 0) {
        *res = (*res)*10+((*s++)-'0');
    }
}

SQInteger scisodigit(SQInteger c) {
    return c >= _SC('0') && c <= _SC('7');
}

void LexOctal(const SQChar *s,SQUnsignedInteger *res) {
    *res = 0;
    while(*s != 0) {
        if(scisodigit(*s)) *res = (*res)*8+((*s++)-'0');
        else { assert(0); }
    }
}

SQInteger isexponent(SQInteger c) {
    return c == 'e' || c=='E';
}

#define MAX_HEX_DIGITS (sizeof(SQInteger)*2)
SQInteger lexer_read_number(SQLexer * lexer)
{
#define TINT 1
#define TFLOAT 2
#define THEX 3
#define TSCIENTIFIC 4
#define TOCTAL 5
    SQInteger type = TINT, firstchar = lexer->_currdata;
    SQChar *sTemp;
    
    strbuf_reset(&lexer->state.string_buffer);
    lexer_next(lexer);
    if(firstchar == _SC('0') && (toupper(lexer->_currdata) == _SC('X') || scisodigit(lexer->_currdata)) ) {
        if(scisodigit(lexer->_currdata)) {
            type = TOCTAL;

            while(scisodigit(lexer->_currdata)) {
                strbuf_push_back(&lexer->state.string_buffer, lexer->_currdata);
                lexer_next(lexer);
            }

            if (scisdigit(lexer->_currdata)) {
                lexer_error(lexer, _SC("invalid octal number"));
            }
        } else {
            lexer_next(lexer);
            type = THEX;

            while (isxdigit(lexer->_currdata)) {
                strbuf_push_back(&lexer->state.string_buffer, lexer->_currdata);
                lexer_next(lexer);
            }

            if (lexer->state.string_buffer.len > MAX_HEX_DIGITS) {
                lexer_error(lexer, _SC("too many digits for an Hex number"));
                // noreturn
            }
        }
    }
    else {
        strbuf_push_back(&lexer->state.string_buffer, (int)firstchar);
        while (lexer->_currdata == _SC('.') || scisdigit(lexer->_currdata) || isexponent(lexer->_currdata)) {
            if(lexer->_currdata == _SC('.') || isexponent(lexer->_currdata)) type = TFLOAT;
            if(isexponent(lexer->_currdata)) {
                if(type != TFLOAT) lexer_error(lexer, _SC("invalid numeric format"));
                type = TSCIENTIFIC;
                strbuf_push_back(&lexer->state.string_buffer, lexer->_currdata);
                lexer_next(lexer);
                if(lexer->_currdata == '+' || lexer->_currdata == '-'){
                    strbuf_push_back(&lexer->state.string_buffer, lexer->_currdata);
                    lexer_next(lexer);
                }
                if(!scisdigit(lexer->_currdata)) lexer_error(lexer, _SC("exponent expected"));
            }

            strbuf_push_back(&lexer->state.string_buffer, lexer->_currdata);
            lexer_next(lexer);
        }
    }
    strbuf_push_back(&lexer->state.string_buffer, '\0');
    switch(type) {
    case TSCIENTIFIC:
    case TFLOAT:
        lexer->state.float_value = (SQFloat)scstrtod(reinterpret_cast<char const *>(&lexer->state.string_buffer.data[0]),&sTemp);
        return TK_FLOAT;
    case TINT:
        LexInteger(reinterpret_cast<char const *>(&lexer->state.string_buffer.data[0]), &lexer->state.uint_value);
        return TK_INTEGER;
    case THEX:
        LexHexadecimal(reinterpret_cast<char const *>(&lexer->state.string_buffer.data[0]), &lexer->state.uint_value);
        return TK_INTEGER;
    case TOCTAL:
        LexOctal(reinterpret_cast<char const *>(&lexer->state.string_buffer.data[0]), &lexer->state.uint_value);
        return TK_INTEGER;
    }
    return 0;
}

SQInteger lexer_read_id(SQLexer * lexer) {
    SQInteger res;
    strbuf_reset(&lexer->state.string_buffer);
    do {
        strbuf_push_back(&lexer->state.string_buffer, lexer->_currdata);
        lexer_next(lexer);
    } while(scisalnum(lexer->_currdata) || lexer->_currdata == _SC('_'));

    strbuf_push_back(&lexer->state.string_buffer, '\0');

    res = lexer_get_id_type(reinterpret_cast<char const *>(&lexer->state.string_buffer.data[0]), lexer->state.string_buffer.len - 1);
    if(res == TK_IDENTIFIER || res == TK_CONSTRUCTOR) {
        lexer->state.string_value = reinterpret_cast<char const *>(&lexer->state.string_buffer.data[0]);
    }

    return res;
}

void lexer_block_comment(SQLexer * lexer) {
    while (true) {
        switch(lexer->_currdata) {
        case '*':
            lexer_next(lexer);
            if (lexer->_currdata == '/') {
                lexer_next(lexer);
                return;
            }
            continue;
        case '\n':
            lexer->state.current_line++;
            lexer_next(lexer);
            continue;
        case SQUIRREL_EOB:
            lexer_error(lexer, "missing \"*/\" in comment");
            // bug? lexer_error is noreturn probably.. because longjmp in compiler
        default:
            lexer_next(lexer);
        }
    }
}

void lexer_line_comment(SQLexer * lexer) {
    do {
        lexer_next(lexer);
    } while (lexer->_currdata != '\n' && lexer->_currdata > SQUIRREL_EOB);
}

void lexer_init(SQLexer * lexer, SQLEXREADFUNC rg, void * up, CompilerErrorFunc efunc, void *ed) {
    lexer->state.token = 0;
    lexer->state.prev_token = 0;

    lexer->state.current_line = 1;
    lexer->state.current_column = 0;
    lexer->state.last_token_line = 1;
    lexer->state.uint_value = 0;
    lexer->state.float_value = 0;
    lexer->state.string_value = nullptr;
    strbuf_init(&lexer->state.string_buffer);

    lexer->reader_func = rg;
    lexer->reader_context = up;
    lexer->error_func = efunc;
    lexer->error_context = ed;

    lexer->_curtoken = 0;
    lexer->_reached_eof = SQFalse;
    lexer->_currdata = 0;

    lexer_next(lexer);
    lexer->state.current_column--;
}

LexerState * lexer_lex(SQLexer * lexer) {

#define RETURN_TOKEN(t) { lexer->state.prev_token = lexer->_curtoken; lexer->_curtoken = t; lexer->state.token = t; return &lexer->state;}

    lexer->state.last_token_line = lexer->state.current_line;

    while(lexer->_currdata != SQUIRREL_EOB) {
        switch(lexer->_currdata){
        case '\t':
        case '\r':
        case ' ':
            lexer_next(lexer);
            continue;
        case '\n':
            lexer->state.current_line++;
            lexer->state.prev_token = lexer->_curtoken;
            lexer->_curtoken = '\n';
            lexer_next(lexer);
            lexer->state.current_column=1;
            continue;
        case '#':
            lexer_line_comment(lexer);
            continue;
        case '/':
            lexer_next(lexer);

            switch (lexer->_currdata) {
            case '*':
                lexer_next(lexer);
                lexer_block_comment(lexer);
                continue;
            case '/':
                lexer_line_comment(lexer);
                continue;
            case '=':
                lexer_next(lexer);
                RETURN_TOKEN(TK_DIVEQ);
            case '>':
                lexer_next(lexer);
                RETURN_TOKEN(TK_ATTR_CLOSE);
            default:
                RETURN_TOKEN('/');
            }
        case '=':
            lexer_next(lexer);
            if (lexer->_currdata == '=') {
                lexer_next(lexer);
                RETURN_TOKEN(TK_EQ);
            } else {
                RETURN_TOKEN('=');
            }
        case '<':
            lexer_next(lexer);
            switch(lexer->_currdata) {
            case _SC('='):
                lexer_next(lexer);
                if(lexer->_currdata == _SC('>')) {
                    lexer_next(lexer);
                    RETURN_TOKEN(TK_3WAYSCMP);
                }
                RETURN_TOKEN(TK_LE)
                break;
            case _SC('-'): lexer_next(lexer); RETURN_TOKEN(TK_NEWSLOT); break;
            case _SC('<'): lexer_next(lexer); RETURN_TOKEN(TK_SHIFTL); break;
            case _SC('/'): lexer_next(lexer); RETURN_TOKEN(TK_ATTR_OPEN); break;
            }
            RETURN_TOKEN('<');
        case _SC('>'):
            lexer_next(lexer);
            if (lexer->_currdata == _SC('=')){ lexer_next(lexer); RETURN_TOKEN(TK_GE);}
            else if(lexer->_currdata == _SC('>')){
                lexer_next(lexer);
                if(lexer->_currdata == _SC('>')){
                    lexer_next(lexer);
                    RETURN_TOKEN(TK_USHIFTR);
                }
                RETURN_TOKEN(TK_SHIFTR);
            }
            else { RETURN_TOKEN('>') }
        case _SC('!'):
            lexer_next(lexer);
            if (lexer->_currdata != _SC('=')){ RETURN_TOKEN('!')}
            else { lexer_next(lexer); RETURN_TOKEN(TK_NE); }
        case _SC('@'): {
            SQInteger stype;
            lexer_next(lexer);
            if(lexer->_currdata != _SC('"')) {
                RETURN_TOKEN('@');
            }
            if((stype=lexer_read_string(lexer, '"', true))!=-1) {
                RETURN_TOKEN(stype);
            }
            lexer_error(lexer, _SC("error parsing the string"));
                       }
        case _SC('"'):
        case _SC('\''): {
            SQInteger stype;
            if((stype=lexer_read_string(lexer, lexer->_currdata, false))!=-1){
                RETURN_TOKEN(stype);
            }
            lexer_error(lexer, _SC("error parsing the string"));
            }
        case _SC('{'): case _SC('}'): case _SC('('): case _SC(')'): case _SC('['): case _SC(']'):
        case _SC(';'): case _SC(','): case _SC('?'): case _SC('^'): case _SC('~'):
            {SQInteger ret = lexer->_currdata;
            lexer_next(lexer); RETURN_TOKEN(ret); }
        case _SC('.'):
            lexer_next(lexer);
            if (lexer->_currdata != _SC('.')){ RETURN_TOKEN('.') }
            lexer_next(lexer);
            if (lexer->_currdata != _SC('.')){ lexer_error(lexer, _SC("invalid token '..'")); }
            lexer_next(lexer);
            RETURN_TOKEN(TK_VARPARAMS);
        case _SC('&'):
            lexer_next(lexer);
            if (lexer->_currdata != _SC('&')){ RETURN_TOKEN('&') }
            else { lexer_next(lexer); RETURN_TOKEN(TK_AND); }
        case _SC('|'):
            lexer_next(lexer);
            if (lexer->_currdata != _SC('|')){ RETURN_TOKEN('|') }
            else { lexer_next(lexer); RETURN_TOKEN(TK_OR); }
        case _SC(':'):
            lexer_next(lexer);
            if (lexer->_currdata != _SC(':')){ RETURN_TOKEN(':') }
            else { lexer_next(lexer); RETURN_TOKEN(TK_DOUBLE_COLON); }
        case _SC('*'):
            lexer_next(lexer);
            if (lexer->_currdata == _SC('=')){ lexer_next(lexer); RETURN_TOKEN(TK_MULEQ);}
            else RETURN_TOKEN('*');
        case _SC('%'):
            lexer_next(lexer);
            if (lexer->_currdata == _SC('=')){ lexer_next(lexer); RETURN_TOKEN(TK_MODEQ);}
            else RETURN_TOKEN('%');
        case _SC('-'):
            lexer_next(lexer);
            if (lexer->_currdata == _SC('=')){ lexer_next(lexer); RETURN_TOKEN(TK_MINUSEQ);}
            else if  (lexer->_currdata == _SC('-')){ lexer_next(lexer); RETURN_TOKEN(TK_MINUSMINUS);}
            else RETURN_TOKEN('-');
        case _SC('+'):
            lexer_next(lexer);
            if (lexer->_currdata == _SC('=')){ lexer_next(lexer); RETURN_TOKEN(TK_PLUSEQ);}
            else if (lexer->_currdata == _SC('+')){ lexer_next(lexer); RETURN_TOKEN(TK_PLUSPLUS);}
            else RETURN_TOKEN('+');
        case SQUIRREL_EOB:
            lexer->state.token = 0;
            return &lexer->state;
        default:{
                if (scisdigit(lexer->_currdata)) {
                    SQInteger ret = lexer_read_number(lexer);
                    RETURN_TOKEN(ret);
                }
                else if (scisalpha(lexer->_currdata) || lexer->_currdata == _SC('_')) {
                    SQInteger t = lexer_read_id(lexer);
                    RETURN_TOKEN(t);
                }
                else {
                    SQInteger c = lexer->_currdata;
                    if (sciscntrl((int)c)) lexer_error(lexer, _SC("unexpected character(control)"));
                    lexer_next(lexer);
                    RETURN_TOKEN(c);
                }
                RETURN_TOKEN(0);
            }
        }
    }

    lexer->state.token = 0;
    return &lexer->state;
}
