#pragma once

typedef unsigned char LexChar;

struct SQLexer {
    SQLexer(
        SQLEXREADFUNC rg,
        SQUserPointer up,
        CompilerErrorFunc efunc,
        void *ed
    )
        : _curtoken(0)
        , _reached_eof(SQFalse)
        , _prevtoken(-1)
        , _currentline(1)
        , _lasttokenline(1)
        , _currentcolumn(0)
        , _svalue(nullptr)
        , _nvalue(0)
        , _fvalue(0)
        , _readf(rg)
        , _up(up)
        , _currdata(0)
        , _longstr()
        , _errfunc(efunc)
        , _errtarget(ed)
    {}

    void Init() {
        Next();
        _currentcolumn--;
    }

    void Error(const SQChar *err);

    SQInteger Lex();

    SQChar const * Tok2Str(SQInteger tok);
private:
    SQInteger GetIDType(const SQChar *s,SQInteger len);
    SQInteger ReadString(SQInteger ndelim,bool verbatim);
    SQInteger ReadNumber();
    void LexBlockComment();
    void LexLineComment();
    SQInteger ReadID();
    void Next();
    SQInteger AddUTF8(SQUnsignedInteger ch);
    SQInteger ProcessStringHexEscape(SQChar *dest, SQInteger maxdigits);

    SQInteger _curtoken;
    SQBool _reached_eof;
public:
    SQInteger _prevtoken;
    SQInteger _currentline;
    SQInteger _lasttokenline;
    SQInteger _currentcolumn;
    SQChar const * _svalue;
    SQUnsignedInteger _nvalue;
    SQFloat _fvalue;
    SQLEXREADFUNC _readf;
    SQUserPointer _up;
    LexChar _currdata;
    sqvector<SQChar> _longstr;
    CompilerErrorFunc _errfunc;
    void *_errtarget;
};
