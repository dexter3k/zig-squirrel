#include "sqpcheader.h"
#include <ctype.h>
#include <stdlib.h>

#include "sqtable.h"
#include "sqstring.h"
#include "sqcompiler.h"
#include "sqlexer.h"

#define RETURN_TOKEN(t) { _prevtoken = _curtoken; _curtoken = t; return t;}
#define INIT_TEMP_STRING() { _longstr.resize(0);}
#define APPEND_CHAR(c) { _longstr.push_back(c);}

void SQLexer::Error(SQChar const * err) {
    _errfunc(_errtarget,err);
}

void SQLexer::Next() {
    SQInteger t = _readf(_up);
    if (t > MAX_CHAR) {
        Error("Invalid character");
    }

    _currentcolumn++;

    if (t == 0) {
        _currdata = SQUIRREL_EOB;
        _reached_eof = SQTrue;
        return;
    }

    _currdata = LexChar(t);
}

const SQChar * SQLexer::Tok2Str(SQInteger tok) {
    switch (tok) {
        case TK_WHILE: return "while";
        case TK_DO: return "do";
        case TK_IF: return "if";
        case TK_ELSE: return "else";
        case TK_BREAK: return "break";
        case TK_CONTINUE: return "continue";
        case TK_RETURN: return "return";
        case TK_NULL: return "null";
        case TK_FUNCTION: return "function";
        case TK_LOCAL: return "local";
        case TK_FOR: return "for";
        case TK_FOREACH: return "foreach";
        case TK_IN: return "in";
        case TK_TYPEOF: return "typeof";
        case TK_BASE: return "base";
        case TK_DELETE: return "delete";
        case TK_TRY: return "try";
        case TK_CATCH: return "catch";
        case TK_THROW: return "throw";
        case TK_CLONE: return "clone";
        case TK_YIELD: return "yield";
        case TK_RESUME: return "resume";
        case TK_SWITCH: return "switch";
        case TK_CASE: return "case";
        case TK_DEFAULT: return "default";
        case TK_THIS: return "this";
        case TK_CLASS: return "class";
        case TK_EXTENDS: return "extends";
        case TK_CONSTRUCTOR: return "constructor";
        case TK_INSTANCEOF: return "instanceof";
        case TK_TRUE: return "true";
        case TK_FALSE: return "false";
        case TK_STATIC: return "static";
        case TK_ENUM: return "enum";
        case TK_CONST: return "const";
        case TK___LINE__: return "__LINE__";
        case TK___FILE__: return "__FILE__";
        case TK_RAWCALL: return "rawcall";
        default:
            return nullptr;
    }
}

void SQLexer::LexBlockComment()
{
    bool done = false;
    while(!done) {
        switch(_currdata) {
            case _SC('*'): { Next(); if(_currdata == _SC('/')) { done = true; Next(); }}; continue;
            case _SC('\n'): _currentline++; Next(); continue;
            case SQUIRREL_EOB: Error(_SC("missing \"*/\" in comment"));
            default: Next();
        }
    }
}
void SQLexer::LexLineComment() {
    do {
        Next();
    } while (_currdata != '\n' && _currdata > SQUIRREL_EOB);
}

SQInteger SQLexer::Lex() {
    _lasttokenline = _currentline;
    while(_currdata != SQUIRREL_EOB) {
        switch(_currdata){
        case '\t':
        case '\r':
        case ' ':
            Next();
            continue;
        case '\n':
            _currentline++;
            _prevtoken=_curtoken;
            _curtoken=_SC('\n');
            Next();
            _currentcolumn=1;
            continue;
        case _SC('#'): LexLineComment(); continue;
        case _SC('/'):
            Next();
            switch(_currdata){
            case _SC('*'):
                Next();
                LexBlockComment();
                continue;
            case _SC('/'):
                LexLineComment();
                continue;
            case _SC('='):
                Next();
                RETURN_TOKEN(TK_DIVEQ);
                continue;
            case _SC('>'):
                Next();
                RETURN_TOKEN(TK_ATTR_CLOSE);
                continue;
            default:
                RETURN_TOKEN('/');
            }
        case _SC('='):
            Next();
            if (_currdata != _SC('=')){ RETURN_TOKEN('=') }
            else { Next(); RETURN_TOKEN(TK_EQ); }
        case _SC('<'):
            Next();
            switch(_currdata) {
            case _SC('='):
                Next();
                if(_currdata == _SC('>')) {
                    Next();
                    RETURN_TOKEN(TK_3WAYSCMP);
                }
                RETURN_TOKEN(TK_LE)
                break;
            case _SC('-'): Next(); RETURN_TOKEN(TK_NEWSLOT); break;
            case _SC('<'): Next(); RETURN_TOKEN(TK_SHIFTL); break;
            case _SC('/'): Next(); RETURN_TOKEN(TK_ATTR_OPEN); break;
            }
            RETURN_TOKEN('<');
        case _SC('>'):
            Next();
            if (_currdata == _SC('=')){ Next(); RETURN_TOKEN(TK_GE);}
            else if(_currdata == _SC('>')){
                Next();
                if(_currdata == _SC('>')){
                    Next();
                    RETURN_TOKEN(TK_USHIFTR);
                }
                RETURN_TOKEN(TK_SHIFTR);
            }
            else { RETURN_TOKEN('>') }
        case _SC('!'):
            Next();
            if (_currdata != _SC('=')){ RETURN_TOKEN('!')}
            else { Next(); RETURN_TOKEN(TK_NE); }
        case _SC('@'): {
            SQInteger stype;
            Next();
            if(_currdata != _SC('"')) {
                RETURN_TOKEN('@');
            }
            if((stype=ReadString('"',true))!=-1) {
                RETURN_TOKEN(stype);
            }
            Error(_SC("error parsing the string"));
                       }
        case _SC('"'):
        case _SC('\''): {
            SQInteger stype;
            if((stype=ReadString(_currdata,false))!=-1){
                RETURN_TOKEN(stype);
            }
            Error(_SC("error parsing the string"));
            }
        case _SC('{'): case _SC('}'): case _SC('('): case _SC(')'): case _SC('['): case _SC(']'):
        case _SC(';'): case _SC(','): case _SC('?'): case _SC('^'): case _SC('~'):
            {SQInteger ret = _currdata;
            Next(); RETURN_TOKEN(ret); }
        case _SC('.'):
            Next();
            if (_currdata != _SC('.')){ RETURN_TOKEN('.') }
            Next();
            if (_currdata != _SC('.')){ Error(_SC("invalid token '..'")); }
            Next();
            RETURN_TOKEN(TK_VARPARAMS);
        case _SC('&'):
            Next();
            if (_currdata != _SC('&')){ RETURN_TOKEN('&') }
            else { Next(); RETURN_TOKEN(TK_AND); }
        case _SC('|'):
            Next();
            if (_currdata != _SC('|')){ RETURN_TOKEN('|') }
            else { Next(); RETURN_TOKEN(TK_OR); }
        case _SC(':'):
            Next();
            if (_currdata != _SC(':')){ RETURN_TOKEN(':') }
            else { Next(); RETURN_TOKEN(TK_DOUBLE_COLON); }
        case _SC('*'):
            Next();
            if (_currdata == _SC('=')){ Next(); RETURN_TOKEN(TK_MULEQ);}
            else RETURN_TOKEN('*');
        case _SC('%'):
            Next();
            if (_currdata == _SC('=')){ Next(); RETURN_TOKEN(TK_MODEQ);}
            else RETURN_TOKEN('%');
        case _SC('-'):
            Next();
            if (_currdata == _SC('=')){ Next(); RETURN_TOKEN(TK_MINUSEQ);}
            else if  (_currdata == _SC('-')){ Next(); RETURN_TOKEN(TK_MINUSMINUS);}
            else RETURN_TOKEN('-');
        case _SC('+'):
            Next();
            if (_currdata == _SC('=')){ Next(); RETURN_TOKEN(TK_PLUSEQ);}
            else if (_currdata == _SC('+')){ Next(); RETURN_TOKEN(TK_PLUSPLUS);}
            else RETURN_TOKEN('+');
        case SQUIRREL_EOB:
            return 0;
        default:{
                if (scisdigit(_currdata)) {
                    SQInteger ret = ReadNumber();
                    RETURN_TOKEN(ret);
                }
                else if (scisalpha(_currdata) || _currdata == _SC('_')) {
                    SQInteger t = ReadID();
                    RETURN_TOKEN(t);
                }
                else {
                    SQInteger c = _currdata;
                    if (sciscntrl((int)c)) Error(_SC("unexpected character(control)"));
                    Next();
                    RETURN_TOKEN(c);
                }
                RETURN_TOKEN(0);
            }
        }
    }
    return 0;
}

SQInteger SQLexer::GetIDType(SQChar const * s, SQInteger len) {
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

#ifdef SQUNICODE
#if WCHAR_SIZE == 2
SQInteger SQLexer::AddUTF16(SQUnsignedInteger ch)
{
    if (ch >= 0x10000)
    {
        SQUnsignedInteger code = (ch - 0x10000);
        APPEND_CHAR((SQChar)(0xD800 | (code >> 10)));
        APPEND_CHAR((SQChar)(0xDC00 | (code & 0x3FF)));
        return 2;
    }
    else {
        APPEND_CHAR((SQChar)ch);
        return 1;
    }
}
#endif
#else
SQInteger SQLexer::AddUTF8(SQUnsignedInteger ch)
{
    if (ch < 0x80) {
        APPEND_CHAR((char)ch);
        return 1;
    }
    if (ch < 0x800) {
        APPEND_CHAR((SQChar)((ch >> 6) | 0xC0));
        APPEND_CHAR((SQChar)((ch & 0x3F) | 0x80));
        return 2;
    }
    if (ch < 0x10000) {
        APPEND_CHAR((SQChar)((ch >> 12) | 0xE0));
        APPEND_CHAR((SQChar)(((ch >> 6) & 0x3F) | 0x80));
        APPEND_CHAR((SQChar)((ch & 0x3F) | 0x80));
        return 3;
    }
    if (ch < 0x110000) {
        APPEND_CHAR((SQChar)((ch >> 18) | 0xF0));
        APPEND_CHAR((SQChar)(((ch >> 12) & 0x3F) | 0x80));
        APPEND_CHAR((SQChar)(((ch >> 6) & 0x3F) | 0x80));
        APPEND_CHAR((SQChar)((ch & 0x3F) | 0x80));
        return 4;
    }
    return 0;
}
#endif

SQInteger SQLexer::ProcessStringHexEscape(SQChar *dest, SQInteger maxdigits)
{
    Next();
    if (!isxdigit(_currdata)) Error(_SC("hexadecimal number expected"));
    SQInteger n = 0;
    while (isxdigit(_currdata) && n < maxdigits) {
        dest[n] = _currdata;
        n++;
        Next();
    }
    dest[n] = 0;
    return n;
}

SQInteger SQLexer::ReadString(SQInteger ndelim,bool verbatim)
{
    INIT_TEMP_STRING();
    Next();

    if (_currdata <= SQUIRREL_EOB) {
        return -1;
    }

    for(;;) {
        while(_currdata != ndelim) {
            SQInteger x = _currdata;
            switch (x) {
            case SQUIRREL_EOB:
                Error(_SC("unfinished string"));
                return -1;
            case _SC('\n'):
                if(!verbatim) Error(_SC("newline in a constant"));
                APPEND_CHAR(_currdata); Next();
                _currentline++;
                break;
            case _SC('\\'):
                if(verbatim) {
                    APPEND_CHAR('\\'); Next();
                }
                else {
                    Next();
                    switch(_currdata) {
                    case _SC('x'):  {
                        const SQInteger maxdigits = sizeof(SQChar) * 2;
                        SQChar temp[maxdigits + 1];
                        ProcessStringHexEscape(temp, maxdigits);
                        SQChar *stemp;
                        APPEND_CHAR((SQChar)scstrtoul(temp, &stemp, 16));
                    }
                    break;
                    case _SC('U'):
                    case _SC('u'):  {
                        const SQInteger maxdigits = _currdata == 'u' ? 4 : 8;
                        SQChar temp[8 + 1];
                        ProcessStringHexEscape(temp, maxdigits);
                        SQChar *stemp;
#ifdef SQUNICODE
#if WCHAR_SIZE == 2
                        AddUTF16(scstrtoul(temp, &stemp, 16));
#else
                        APPEND_CHAR((SQChar)scstrtoul(temp, &stemp, 16));
#endif
#else
                        AddUTF8(scstrtoul(temp, &stemp, 16));
#endif
                    }
                    break;
                    case _SC('t'): APPEND_CHAR(_SC('\t')); Next(); break;
                    case _SC('a'): APPEND_CHAR(_SC('\a')); Next(); break;
                    case _SC('b'): APPEND_CHAR(_SC('\b')); Next(); break;
                    case _SC('n'): APPEND_CHAR(_SC('\n')); Next(); break;
                    case _SC('r'): APPEND_CHAR(_SC('\r')); Next(); break;
                    case _SC('v'): APPEND_CHAR(_SC('\v')); Next(); break;
                    case _SC('f'): APPEND_CHAR(_SC('\f')); Next(); break;
                    case _SC('0'): APPEND_CHAR(_SC('\0')); Next(); break;
                    case _SC('\\'): APPEND_CHAR(_SC('\\')); Next(); break;
                    case _SC('"'): APPEND_CHAR(_SC('"')); Next(); break;
                    case _SC('\''): APPEND_CHAR(_SC('\'')); Next(); break;
                    default:
                        Error(_SC("unrecognised escaper char"));
                    break;
                    }
                }
                break;
            default:
                APPEND_CHAR(_currdata);
                Next();
            }
        }
        Next();
        if(verbatim && _currdata == '"') { //double quotation
            APPEND_CHAR(_currdata);
            Next();
        }
        else {
            break;
        }
    }
    _longstr.push_back('\0');
    SQInteger len = _longstr.size()-1;
    if(ndelim == _SC('\'')) {
        if(len == 0) Error(_SC("empty constant"));
        if(len > 1) Error(_SC("constant too long"));
        _nvalue = SQUnsignedInteger(SQInteger(_longstr[0])); // sign-extend the value!
        return TK_INTEGER;
    }
    _svalue = &_longstr[0];
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

SQInteger scisodigit(SQInteger c) { return c >= _SC('0') && c <= _SC('7'); }

void LexOctal(const SQChar *s,SQUnsignedInteger *res)
{
    *res = 0;
    while(*s != 0)
    {
        if(scisodigit(*s)) *res = (*res)*8+((*s++)-'0');
        else { assert(0); }
    }
}

SQInteger isexponent(SQInteger c) { return c == 'e' || c=='E'; }


#define MAX_HEX_DIGITS (sizeof(SQInteger)*2)
SQInteger SQLexer::ReadNumber()
{
#define TINT 1
#define TFLOAT 2
#define THEX 3
#define TSCIENTIFIC 4
#define TOCTAL 5
    SQInteger type = TINT, firstchar = _currdata;
    SQChar *sTemp;
    INIT_TEMP_STRING();
    Next();
    if(firstchar == _SC('0') && (toupper(_currdata) == _SC('X') || scisodigit(_currdata)) ) {
        if(scisodigit(_currdata)) {
            type = TOCTAL;
            while(scisodigit(_currdata)) {
                APPEND_CHAR(_currdata);
                Next();
            }
            if(scisdigit(_currdata)) Error(_SC("invalid octal number"));
        }
        else {
            Next();
            type = THEX;
            while(isxdigit(_currdata)) {
                APPEND_CHAR(_currdata);
                Next();
            }
            if(_longstr.size() > MAX_HEX_DIGITS) Error(_SC("too many digits for an Hex number"));
        }
    }
    else {
        APPEND_CHAR((int)firstchar);
        while (_currdata == _SC('.') || scisdigit(_currdata) || isexponent(_currdata)) {
            if(_currdata == _SC('.') || isexponent(_currdata)) type = TFLOAT;
            if(isexponent(_currdata)) {
                if(type != TFLOAT) Error(_SC("invalid numeric format"));
                type = TSCIENTIFIC;
                APPEND_CHAR(_currdata);
                Next();
                if(_currdata == '+' || _currdata == '-'){
                    APPEND_CHAR(_currdata);
                    Next();
                }
                if(!scisdigit(_currdata)) Error(_SC("exponent expected"));
            }

            APPEND_CHAR(_currdata);
            Next();
        }
    }
    _longstr.push_back('\0');
    switch(type) {
    case TSCIENTIFIC:
    case TFLOAT:
        _fvalue = (SQFloat)scstrtod(&_longstr[0],&sTemp);
        return TK_FLOAT;
    case TINT:
        LexInteger(&_longstr[0], &_nvalue);
        return TK_INTEGER;
    case THEX:
        LexHexadecimal(&_longstr[0], &_nvalue);
        return TK_INTEGER;
    case TOCTAL:
        LexOctal(&_longstr[0], &_nvalue);
        return TK_INTEGER;
    }
    return 0;
}

SQInteger SQLexer::ReadID()
{
    SQInteger res;
    INIT_TEMP_STRING();
    do {
        APPEND_CHAR(_currdata);
        Next();
    } while(scisalnum(_currdata) || _currdata == _SC('_'));
    _longstr.push_back('\0');
    res = GetIDType(&_longstr[0],_longstr.size() - 1);
    if(res == TK_IDENTIFIER || res == TK_CONSTRUCTOR) {
        _svalue = &_longstr[0];
    }
    return res;
}
