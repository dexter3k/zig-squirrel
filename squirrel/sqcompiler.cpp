/*
    see copyright notice in squirrel.h
*/
#include "sqpcheader.h"
#ifndef NO_COMPILER
#include <stdarg.h>
#include <setjmp.h>
#include "sqopcodes.h"
#include "SQString.hpp"
#include "sqfuncproto.h"
#include "sqcompiler.h"
#include "sqfuncstate.h"
#include "lexer.h"
#include "sqvm.h"
#include "sqtable.h"

enum ExpressionType {
    EXPR   = 1,
    OBJECT = 2,
    BASE   = 3,
    LOCAL  = 4,
    OUTER  = 5
};

struct SQExpState {
  ExpressionType etype;
  SQInteger  epos;        /* expr. location on stack; -1 for OBJECT and BASE */
  bool       donot_get;   /* signal not to deref the next value */
};

#define MAX_COMPILER_ERROR_LEN 256

struct SQScope {
    SQInteger outers;
    SQInteger stacksize;
};

#define BEGIN_SCOPE() SQScope __oldscope__ = _scope; \
                     _scope.outers = _fs->_outers; \
                     _scope.stacksize = _fs->GetStackSize();

#define RESOLVE_OUTERS() if(_fs->GetStackSize() != _scope.stacksize) { \
                            if(_fs->CountOuters(_scope.stacksize)) { \
                                _fs->AddInstruction(_OP_CLOSE,0,_scope.stacksize); \
                            } \
                        }

#define END_SCOPE_NO_CLOSE() {  if(_fs->GetStackSize() != _scope.stacksize) { \
                            _fs->SetStackSize(_scope.stacksize); \
                        } \
                        _scope = __oldscope__; \
                    }

#define END_SCOPE() {   SQInteger oldouters = _fs->_outers;\
                        if(_fs->GetStackSize() != _scope.stacksize) { \
                            _fs->SetStackSize(_scope.stacksize); \
                            if(oldouters != _fs->_outers) { \
                                _fs->AddInstruction(_OP_CLOSE,0,_scope.stacksize); \
                            } \
                        } \
                        _scope = __oldscope__; \
                    }

#define BEGIN_BREAKBLE_BLOCK()  SQInteger __nbreaks__=_fs->_unresolvedbreaks.size(); \
                            SQInteger __ncontinues__=_fs->_unresolvedcontinues.size(); \
                            _fs->_breaktargets.push_back(0);_fs->_continuetargets.push_back(0);

#define END_BREAKBLE_BLOCK(continue_target) {__nbreaks__=_fs->_unresolvedbreaks.size()-__nbreaks__; \
                    __ncontinues__=_fs->_unresolvedcontinues.size()-__ncontinues__; \
                    if(__ncontinues__>0)ResolveContinues(_fs,__ncontinues__,continue_target); \
                    if(__nbreaks__>0)ResolveBreaks(_fs,__nbreaks__); \
                    _fs->_breaktargets.pop_back();_fs->_continuetargets.pop_back();}

class SQCompiler {
    SQFuncState *_fs;
    SQObjectPtr _sourcename;
    SQLexer lexer;
    LexerState * lexer_state;
    bool _lineinfo;
    bool _raiseerror;
    SQExpState   _es;
    SQScope _scope;
    SQChar _compilererror[MAX_COMPILER_ERROR_LEN];
    jmp_buf _errorjmp;
    SQVM *_vm;
public:
    SQCompiler(
        SQVM *v,
        SQLEXREADFUNC rg,
        SQUserPointer up,
        const SQChar* sourcename,
        bool raiseerror,
        bool lineinfo
    )
        : _fs()
        , _sourcename(SQString::Create(v->_sharedstate, sourcename))
        , lexer_state(nullptr)
        , _lineinfo(lineinfo)
        , _raiseerror(raiseerror)

        , _vm(v)
    {
        lexer_init(&lexer, rg, up, ThrowError, this);

        _scope.outers = 0;
        _scope.stacksize = 0;
        _compilererror[0] = '\0';
    }

    ~SQCompiler() {
        lexer_deinit(&lexer);
    }

    static void ThrowError(void *ud, const SQChar *s) {
        SQCompiler *c = (SQCompiler *)ud;
        c->Error(s);
    }

    void Error(const SQChar *s, ...) {
        va_list vl;
        va_start(vl, s);
        scvsprintf(_compilererror, MAX_COMPILER_ERROR_LEN, s, vl);
        va_end(vl);
        longjmp(_errorjmp,1);
    }

    void Lex() {
        lexer_state = lexer_lex(&lexer);
    }

    SQObject Expect(uint16_t tok) {
        if(lexer_state->token != tok) {
            if(lexer_state->token == TK_CONSTRUCTOR && tok == TK_IDENTIFIER) {
                //do nothing
            }
            else {
                const SQChar *etypename;
                if(tok > 255) {
                    switch(tok)
                    {
                    case TK_IDENTIFIER:
                        etypename = "IDENTIFIER";
                        break;
                    case TK_STRING_LITERAL:
                        etypename = "STRING_LITERAL";
                        break;
                    case TK_INTEGER:
                        etypename = "INTEGER";
                        break;
                    case TK_FLOAT:
                        etypename = "FLOAT";
                        break;
                    default:
                        etypename = lexer_token_to_string(tok);
                    }
                    Error("expected '%s'", etypename);
                }
                Error("expected '%c'", tok);
            }
        }

        SQObjectPtr ret;
        switch(tok) {
        case TK_IDENTIFIER:
            ret = _fs->CreateString(lexer_state->string_value);
            break;
        case TK_STRING_LITERAL:
            ret = _fs->CreateString(lexer_state->string_value, lexer_state->string_length);
            break;
        case TK_INTEGER:
            ret = SQObjectPtr(SQInteger(lexer_state->uint_value));
            break;
        case TK_FLOAT:
            ret = SQObjectPtr(SQFloat(lexer_state->float_value));
            break;
        }
        Lex();
        return ret;
    }

    bool IsEndOfStatement() {
        if (lexer_state->prev_token == '\n') {
            return true;
        }
        if (lexer_state->token == 0) {
            return true;
        }
        if (lexer_state->token == '}') {
            return true;
        }
        if (lexer_state->token == ';') {
            return true;
        }
        return false;
    }

    void OptionalSemicolon() {
        if (lexer_state->token == ';') {
            Lex();
            return;
        }

        if (!IsEndOfStatement()) {
            Error("end of statement expected (; or lf)");
        }
    }

    void MoveIfCurrentTargetIsLocal() {
        uint8_t trg = _fs->TopTarget();
        if (_fs->IsLocal(trg)) {
            trg = _fs->PopTarget(); // pops the target and moves it
            _fs->AddInstruction(_OP_MOVE, _fs->PushNewTarget(), trg);
        }
    }

    bool Compile(SQObjectPtr & o) {
        SQFuncState funcstate(_vm->_sharedstate, NULL, ThrowError, this);
        funcstate._name = SQString::Create(_vm->_sharedstate, "main");
        _fs = &funcstate;
        _fs->AddParameter(_fs->CreateString("this"));
        _fs->AddParameter(_fs->CreateString("vargv"));
        _fs->_varparams = true;
        _fs->_sourcename = _sourcename;
        uint16_t const stacksize = _fs->GetStackSize();
        if (setjmp(_errorjmp) == 0) {
            Lex();

            while (lexer_state->token != 0) {
                Statement();

                if (lexer_state->prev_token != '}' && lexer_state->prev_token != ';') {
                    OptionalSemicolon();
                }
            }

            // Shouldn't this be already at same level?
            _fs->SetStackSize(stacksize);
            _fs->AddLineInfos(lexer_state->current_line, _lineinfo, true);
            _fs->AddInstruction(_OP_RETURN, 0xFF);
            _fs->SetStackSize(0);
            o = _fs->BuildProto();
#ifdef _DEBUG_DUMP
            _fs->Dump(_funcproto(o));
#endif
        } else {
            if(_raiseerror && _ss(_vm)->_compilererrorhandler) {
                _ss(_vm)->_compilererrorhandler(
                    _vm,
                    _compilererror,
                    sq_type(_sourcename) == OT_STRING
                        ? _stringval(_sourcename)
                        : "unknown",
                    lexer.state.current_line,
                    lexer.state.current_column
                );
            }
            _vm->_lasterror = SQString::Create(_ss(_vm), _compilererror, -1);
            return false;
        }
        return true;
    }

    void Statements() {
        while (lexer_state->token != '}' && lexer_state->token != TK_DEFAULT && lexer_state->token != TK_CASE) {
            Statement();

            if (lexer_state->prev_token != '}' && lexer_state->prev_token != ';') {
                OptionalSemicolon();
            }
        }
    }

    void Statement(bool closeframe = true) {
        _fs->AddLineInfos(lexer_state->current_line, _lineinfo);

        switch (lexer_state->token) {
            case ';':
                Lex();
                break;
            case TK_IF:
                IfStatement();
                break;
            case TK_WHILE:
                WhileStatement();
                break;
            case TK_DO:
                DoWhileStatement();
                break;
            case TK_FOR:
                ForStatement();
                break;
            case TK_FOREACH:
                ForEachStatement();
                break;
            case TK_SWITCH:
                SwitchStatement();
                break;
            case TK_LOCAL:
                LocalDeclStatement();
                break;
            case TK_RETURN:
            case TK_YIELD: {
                SQOpcode op;
                if (lexer_state->token == TK_RETURN) {
                    op = _OP_RETURN;
                } else {
                    op = _OP_YIELD;
                    _fs->_bgenerator = true;
                }
                Lex();

                /* WARNING:
                    GetStackSize might return 0x100. This will be written as 0.
                    I fixed this in SQVM with assumption there is at least one local
                    in a generator? This is asserted here for that purpose.
                */

                if(!IsEndOfStatement()) {
                    SQInteger retexp = _fs->GetCurrentPos() + 1;
                    CommaExpr();
                    if(op == _OP_RETURN && _fs->_traps > 0) {
                        _fs->AddInstruction(_OP_POPTRAP, _fs->_traps, 0);
                    }
                    _fs->_returnexp = retexp;
                    // GetStackSize might be 0x100?
                    _fs->AddInstruction(op, 1, _fs->PopTarget(), _fs->GetStackSize());
                } else {
                    if(op == _OP_RETURN && _fs->_traps > 0) {
                        _fs->AddInstruction(_OP_POPTRAP, _fs->_traps, 0);
                    }
                    _fs->_returnexp = -1;
                    _fs->AddInstruction(op, 0xFF, 0, _fs->GetStackSize());
                }

                break;
            }
            case TK_BREAK:
                if(_fs->_breaktargets.size() <= 0)Error("'break' has to be in a loop block");
                if(_fs->_breaktargets.top() > 0){
                    _fs->AddInstruction(_OP_POPTRAP, _fs->_breaktargets.top(), 0);
                }
                RESOLVE_OUTERS();
                _fs->AddInstruction(_OP_JMP, 0, -1234);
                _fs->_unresolvedbreaks.push_back(_fs->GetCurrentPos());
                Lex();
                break;
            case TK_CONTINUE:
                if(_fs->_continuetargets.size() <= 0)Error("'continue' has to be in a loop block");
                if(_fs->_continuetargets.top() > 0) {
                    _fs->AddInstruction(_OP_POPTRAP, _fs->_continuetargets.top(), 0);
                }
                RESOLVE_OUTERS();
                _fs->AddInstruction(_OP_JMP, 0, -1234);
                _fs->_unresolvedcontinues.push_back(_fs->GetCurrentPos());
                Lex();
                break;
            case TK_FUNCTION:
                FunctionStatement();
                break;
            case TK_CLASS:
                ClassStatement();
                break;
            case TK_ENUM:
                EnumStatement();
                break;
            case '{':{
                    BEGIN_SCOPE();
                    Lex();
                    Statements();
                    Expect('}');
                    if(closeframe) {
                        END_SCOPE();
                    }
                    else {
                        END_SCOPE_NO_CLOSE();
                    }
                    break;
                }
            case TK_TRY:
                TryCatchStatement();
                break;
            case TK_THROW:
                Lex();
                CommaExpr();
                _fs->AddInstruction(_OP_THROW, _fs->PopTarget());
                break;
            case TK_CONST: {
                Lex();
                SQObject id = Expect(TK_IDENTIFIER);
                Expect('=');
                SQObject val = ExpectScalar();
                OptionalSemicolon();
                SQTable *enums = _table(_ss(_vm)->_consts);
                SQObjectPtr strongid = id;
                enums->NewSlot(strongid,SQObjectPtr(val));
                strongid.Null();
                break;
            }
            default:
                CommaExpr();
                _fs->DiscardTarget();
                break;
        }

        _fs->SnoozeOpt();
    }

    void EmitDerefOp(SQOpcode op) {
        uint8_t const val = _fs->PopTarget();
        uint8_t const key = _fs->PopTarget();
        uint8_t const src = _fs->PopTarget();
        _fs->AddInstruction(op,_fs->PushNewTarget(),src,key,val);
    }

    void Emit2ArgsOP(SQOpcode op, uint8_t p3 = 0) {
        uint8_t const p2 = _fs->PopTarget(); //src in OP_GET
        uint8_t const p1 = _fs->PopTarget(); //key in OP_GET
        _fs->AddInstruction(op, _fs->PushNewTarget(), p1, p2, p3);
    }

    void EmitCompoundArith(uint16_t tok, ExpressionType etype, SQInteger pos) {
        /* Generate code depending on the expression type */
        switch (etype) {
        case LOCAL: {
            uint8_t const p2 = _fs->PopTarget(); //src in OP_GET
            uint8_t const p1 = _fs->PopTarget(); //key in OP_GET
            _fs->PushTarget(p1);
            _fs->AddInstruction(ChooseArithOpByToken(tok),p1, p2, p1, 0);
            _fs->SnoozeOpt();
            break;
        }
        case OBJECT: {
            uint8_t const val = _fs->PopTarget();
            uint8_t const key = _fs->PopTarget();
            uint8_t const src = _fs->PopTarget();
            /* _OP_COMPARITH mixes dest obj and source val in the arg1 */
            _fs->AddInstruction(_OP_COMPARITH,
                _fs->PushNewTarget(),
                (int32_t(src) << 16) | val,
                key,
                ChooseCompArithCharByToken(tok)
            );
            break;
        }
        case OUTER: {
            uint8_t const val = _fs->TopTarget();
            uint8_t const tmp = _fs->PushNewTarget();
            _fs->AddInstruction(_OP_GETOUTER, tmp, pos);
            _fs->AddInstruction(ChooseArithOpByToken(tok), tmp, val, tmp, 0);
            _fs->PopTarget();
            _fs->PopTarget();
            _fs->AddInstruction(_OP_SETOUTER, _fs->PushNewTarget(), pos, tmp);
            break;
        }
        case EXPR:
        case BASE:
            assert(0);
        }
    }

    void CommaExpr() {
        for(Expression();lexer_state->token == ',';_fs->PopTarget(), Lex(), CommaExpr());
    }

    void Expression()
    {
         SQExpState es = _es;
        _es.etype     = EXPR;
        _es.epos      = -1;
        _es.donot_get = false;
        LogicalOrExp();
        switch(lexer_state->token)  {
        case '=':
        case TK_NEWSLOT:
        case TK_MINUSEQ:
        case TK_PLUSEQ:
        case TK_MULEQ:
        case TK_DIVEQ:
        case TK_MODEQ:{
            ExpressionType const ds = _es.etype;
            if (ds == EXPR) {
                Error("can't assign expression");
                // noreturn
            }
            if (ds == BASE) {
                Error("'base' cannot be modified");
                // noreturn
            }

            uint16_t op = lexer_state->token;
            SQInteger pos = _es.epos;

            Lex(); Expression();

            switch (op) {
            case TK_NEWSLOT:
                if (ds != OBJECT) {
                    Error("can't 'create' a local slot");
                }
                EmitDerefOp(_OP_NEWSLOT);
                break;
            case '=': //ASSIGN
                switch (ds) {
                case LOCAL: {
                    uint8_t const src = _fs->PopTarget();
                    uint8_t const dst = _fs->TopTarget();
                    _fs->AddInstruction(_OP_MOVE, dst, src);
                    break;
                }
                case OBJECT:
                    EmitDerefOp(_OP_SET);
                    break;
                case OUTER: {
                    uint8_t const src = _fs->PopTarget();
                    uint8_t const dst = _fs->PushNewTarget();
                    _fs->AddInstruction(_OP_SETOUTER, dst, pos, src);
                }
                case EXPR:
                case BASE:
                    assert(0);
                }
                break;
            case TK_MINUSEQ:
            case TK_PLUSEQ:
            case TK_MULEQ:
            case TK_DIVEQ:
            case TK_MODEQ:
                EmitCompoundArith(op, ds, pos);
                break;
            }
            break;
        }
        case '?': {
            Lex();

            _fs->AddInstruction(_OP_JZ, _fs->PopTarget());
            SQInteger jzpos = _fs->GetCurrentPos();
            uint8_t const trg = _fs->PushNewTarget();

            Expression();

            uint8_t const first_exp = _fs->PopTarget();
            if (trg != first_exp) {
                _fs->AddInstruction(_OP_MOVE, trg, first_exp);
            }
            SQInteger endfirstexp = _fs->GetCurrentPos();
            _fs->AddInstruction(_OP_JMP, 0, 0);

            Expect(':');

            SQInteger jmppos = _fs->GetCurrentPos();

            Expression();

            uint8_t const second_exp = _fs->PopTarget();
            if(trg != second_exp) _fs->AddInstruction(_OP_MOVE, trg, second_exp);
            _fs->SetInstructionParam(jmppos, 1, _fs->GetCurrentPos() - jmppos);
            _fs->SetInstructionParam(jzpos, 1, endfirstexp - jzpos + 1);
            _fs->SnoozeOpt();
            }
            break;
        default:
            break;
        }
        _es = es;
    }

    template<typename T> void INVOKE_EXP(T f)
    {
        SQExpState es = _es;
        _es.etype     = EXPR;
        _es.epos      = -1;
        _es.donot_get = false;
        (this->*f)();
        _es = es;
    }
    template<typename T> void BIN_EXP(SQOpcode op, T f,SQInteger op3 = 0)
    {
        Lex();
        INVOKE_EXP(f);
        uint8_t const op1 = _fs->PopTarget();
        uint8_t const op2 = _fs->PopTarget();
        _fs->AddInstruction(op, _fs->PushNewTarget(), op1, op2, op3);
        _es.etype = EXPR;
    }
    void LogicalOrExp()
    {
        LogicalAndExp();
        for(;;) if(lexer_state->token == TK_OR) {
            uint8_t const first_exp = _fs->PopTarget();
            uint8_t const trg = _fs->PushNewTarget();
            _fs->AddInstruction(_OP_OR, trg, 0, first_exp, 0);
            SQInteger jpos = _fs->GetCurrentPos();
            if(trg != first_exp) _fs->AddInstruction(_OP_MOVE, trg, first_exp);
            Lex(); INVOKE_EXP(&SQCompiler::LogicalOrExp);
            _fs->SnoozeOpt();
            uint8_t const second_exp = _fs->PopTarget();
            if(trg != second_exp) _fs->AddInstruction(_OP_MOVE, trg, second_exp);
            _fs->SnoozeOpt();
            _fs->SetInstructionParam(jpos, 1, (_fs->GetCurrentPos() - jpos));
            _es.etype = EXPR;
            break;
        }else return;
    }
    void LogicalAndExp()
    {
        BitwiseOrExp();
        for(;;) switch(lexer_state->token) {
        case TK_AND: {
            uint8_t const first_exp = _fs->PopTarget();
            uint8_t const trg = _fs->PushNewTarget();
            _fs->AddInstruction(_OP_AND, trg, 0, first_exp, 0);
            SQInteger jpos = _fs->GetCurrentPos();
            if(trg != first_exp) _fs->AddInstruction(_OP_MOVE, trg, first_exp);
            Lex(); INVOKE_EXP(&SQCompiler::LogicalAndExp);
            _fs->SnoozeOpt();
            uint8_t const second_exp = _fs->PopTarget();
            if(trg != second_exp) _fs->AddInstruction(_OP_MOVE, trg, second_exp);
            _fs->SnoozeOpt();
            _fs->SetInstructionParam(jpos, 1, (_fs->GetCurrentPos() - jpos));
            _es.etype = EXPR;
            break;
            }

        default:
            return;
        }
    }
    void BitwiseOrExp()
    {
        BitwiseXorExp();
        for(;;) if(lexer_state->token == '|')
        {BIN_EXP(_OP_BITW, &SQCompiler::BitwiseXorExp,BW_OR);
        }else return;
    }
    void BitwiseXorExp()
    {
        BitwiseAndExp();
        for(;;) if(lexer_state->token == '^')
        {BIN_EXP(_OP_BITW, &SQCompiler::BitwiseAndExp,BW_XOR);
        }else return;
    }
    void BitwiseAndExp()
    {
        EqExp();
        for(;;) if(lexer_state->token == '&')
        {BIN_EXP(_OP_BITW, &SQCompiler::EqExp,BW_AND);
        }else return;
    }
    void EqExp()
    {
        CompExp();
        for(;;) switch(lexer_state->token) {
        case TK_EQ: BIN_EXP(_OP_EQ, &SQCompiler::CompExp); break;
        case TK_NE: BIN_EXP(_OP_NE, &SQCompiler::CompExp); break;
        case TK_3WAYSCMP: BIN_EXP(_OP_CMP, &SQCompiler::CompExp,CMP_3W); break;
        default: return;
        }
    }
    void CompExp()
    {
        ShiftExp();
        for(;;) switch(lexer_state->token) {
        case '>': BIN_EXP(_OP_CMP, &SQCompiler::ShiftExp,CMP_G); break;
        case '<': BIN_EXP(_OP_CMP, &SQCompiler::ShiftExp,CMP_L); break;
        case TK_GE: BIN_EXP(_OP_CMP, &SQCompiler::ShiftExp,CMP_GE); break;
        case TK_LE: BIN_EXP(_OP_CMP, &SQCompiler::ShiftExp,CMP_LE); break;
        case TK_IN: BIN_EXP(_OP_EXISTS, &SQCompiler::ShiftExp); break;
        case TK_INSTANCEOF: BIN_EXP(_OP_INSTANCEOF, &SQCompiler::ShiftExp); break;
        default: return;
        }
    }

    void ShiftExp() {
        PlusExp();
        for(;;) switch(lexer_state->token) {
        case TK_USHIFTR: BIN_EXP(_OP_BITW, &SQCompiler::PlusExp,BW_USHIFTR); break;
        case TK_SHIFTL: BIN_EXP(_OP_BITW, &SQCompiler::PlusExp,BW_SHIFTL); break;
        case TK_SHIFTR: BIN_EXP(_OP_BITW, &SQCompiler::PlusExp,BW_SHIFTR); break;
        default: return;
        }
    }

    SQOpcode ChooseArithOpByToken(uint16_t tok) {
        switch (tok) {
            case TK_PLUSEQ: case '+': return _OP_ADD;
            case TK_MINUSEQ: case '-': return _OP_SUB;
            case TK_MULEQ: case '*': return _OP_MUL;
            case TK_DIVEQ: case '/': return _OP_DIV;
            case TK_MODEQ: case '%': return _OP_MOD;
            default: assert(0);
        }
        return _OP_ADD;
    }

    uint8_t ChooseCompArithCharByToken(uint16_t tok) {
        uint8_t oper;
        switch(tok){
        case TK_MINUSEQ: oper = '-'; break;
        case TK_PLUSEQ: oper = '+'; break;
        case TK_MULEQ: oper = '*'; break;
        case TK_DIVEQ: oper = '/'; break;
        case TK_MODEQ: oper = '%'; break;
        default: oper = 0; //shut up compiler
            assert(0); break;
        };
        return oper;
    }

    void PlusExp()
    {
        MultExp();
        for(;;) switch(lexer_state->token) {
        case '+': case '-':
            BIN_EXP(ChooseArithOpByToken(lexer_state->token), &SQCompiler::MultExp); break;
        default: return;
        }
    }

    void MultExp()
    {
        PrefixedExpr();
        for(;;) switch(lexer_state->token) {
        case '*': case '/': case '%':
            BIN_EXP(ChooseArithOpByToken(lexer_state->token), &SQCompiler::PrefixedExpr); break;
        default: return;
        }
    }
    //if 'pos' != -1 the previous variable is a local variable
    void PrefixedExpr()
    {
        SQInteger pos = Factor();
        for(;;) {
            switch(lexer_state->token) {
            case '.':
                pos = -1;
                Lex();

                _fs->AddInstruction(_OP_LOAD, _fs->PushNewTarget(), _fs->GetConstant(Expect(TK_IDENTIFIER)));
                if(_es.etype==BASE) {
                    Emit2ArgsOP(_OP_GET);
                    pos = _fs->TopTarget();
                    _es.etype = EXPR;
                    _es.epos   = pos;
                }
                else {
                    if(NeedGet()) {
                        Emit2ArgsOP(_OP_GET);
                    }
                    _es.etype = OBJECT;
                }
                break;
            case '[':
                if (lexer_state->prev_token == '\n') {
                    Error("cannot break deref/or comma needed after [exp]=exp slot declaration");
                }

                Lex(); Expression(); Expect(']');
                pos = -1;
                if(_es.etype==BASE) {
                    Emit2ArgsOP(_OP_GET);
                    pos = _fs->TopTarget();
                    _es.etype = EXPR;
                    _es.epos   = pos;
                }
                else {
                    if(NeedGet()) {
                        Emit2ArgsOP(_OP_GET);
                    }
                    _es.etype = OBJECT;
                }
                break;
            case TK_MINUSMINUS:
            case TK_PLUSPLUS:
                {
                    if(IsEndOfStatement()) return;
                    SQInteger diff = (lexer_state->token==TK_MINUSMINUS) ? -1 : 1;
                    Lex();
                    switch(_es.etype)
                    {
                        case EXPR: Error("can't '++' or '--' an expression"); break;
                        case BASE: Error("'base' cannot be modified"); break;
                        case OBJECT:
                            if (_es.donot_get == true)  {
                                // mmh dor this make sense?
                                Error("can't '++' or '--' an expression");
                                // noreturn
                            }
                            Emit2ArgsOP(_OP_PINC, diff);
                            break;
                        case LOCAL: {
                            uint8_t const src = _fs->PopTarget();
                            _fs->AddInstruction(_OP_PINCL, _fs->PushNewTarget(), src, 0, diff);
                                    }
                            break;
                        case OUTER: {
                            uint8_t const tmp1 = _fs->PushNewTarget();
                            uint8_t const tmp2 = _fs->PushNewTarget();
                            _fs->AddInstruction(_OP_GETOUTER, tmp2, _es.epos);
                            _fs->AddInstruction(_OP_PINCL,    tmp1, tmp2, 0, diff);
                            _fs->AddInstruction(_OP_SETOUTER, tmp2, _es.epos, tmp2);
                            _fs->PopTarget();
                        }
                    }
                    _es.etype = EXPR;
                }
                return;
                break;
            case '(':
                switch(_es.etype) {
                    case OBJECT: {
                        uint8_t const key     = _fs->PopTarget();  /* location of the key */
                        uint8_t const table   = _fs->PopTarget();  /* location of the object */
                        uint8_t const closure = _fs->PushNewTarget(); /* location for the closure */
                        uint8_t const ttarget = _fs->PushNewTarget(); /* location for 'this' pointer */
                        _fs->AddInstruction(_OP_PREPCALL, closure, key, table, ttarget);
                        }
                        break;
                    case BASE:
                        _fs->AddInstruction(_OP_MOVE, _fs->PushNewTarget(), 0);
                        break;
                    case OUTER:
                        _fs->AddInstruction(_OP_GETOUTER, _fs->PushNewTarget(), _es.epos);
                        _fs->AddInstruction(_OP_MOVE,     _fs->PushNewTarget(), 0);
                        break;
                    default:
                        _fs->AddInstruction(_OP_MOVE, _fs->PushNewTarget(), 0);
                }
                _es.etype = EXPR;
                Lex();
                FunctionCallArgs();
                break;
            default:
                return;
            }
        }
    }

    SQInteger Factor() {
        switch(lexer_state->token) {
        case TK_STRING_LITERAL:
            _fs->AddInstruction(
                _OP_LOAD,
                _fs->PushNewTarget(),
                _fs->GetConstant(_fs->CreateString(
                    lexer_state->string_value,
                    lexer_state->string_length)));
            Lex();
            break;
        case TK_BASE:
            Lex();
            _fs->AddInstruction(_OP_GETBASE, _fs->PushNewTarget());
            _es.etype  = BASE;
            _es.epos   = _fs->TopTarget();
            return (_es.epos);
            break;
        case TK_IDENTIFIER:
        case TK_CONSTRUCTOR:
        case TK_THIS:
            {
                SQObject id;
                switch(lexer_state->token) {
                    case TK_IDENTIFIER:
                        id = _fs->CreateString(lexer_state->string_value);
                        break;
                    case TK_THIS:
                        id = _fs->CreateString("this", 4);
                        break;
                    case TK_CONSTRUCTOR:
                        id = _fs->CreateString("constructor", 11);
                        break;
                }

                SQObject constant;
                SQInteger pos = -1;
                Lex();
                if ((pos = _fs->GetLocalVariable(id)) != -1) {
                    /* Handle a local variable (includes 'this') */
                    _fs->PushTarget(pos);
                    _es.etype  = LOCAL;
                    _es.epos   = pos;
                } else if ((pos = _fs->GetOuterVariable(id)) != -1) {
                    /* Handle a free var */
                    if (NeedGet()) {
                        _es.epos  = _fs->PushNewTarget();
                        _fs->AddInstruction(_OP_GETOUTER, _es.epos, pos);
                    } else {
                        _es.etype = OUTER;
                        _es.epos  = pos;
                    }
                } else if (_fs->IsConstant(id, constant)) {
                    /* Handle named constant */
                    SQObjectPtr constval;
                    SQObject    constid;
                    if(sq_type(constant) == OT_TABLE) {
                        Expect('.');
                        constid = Expect(TK_IDENTIFIER);
                        if(!_table(constant)->Get(constid, constval)) {
                            constval.Null();
                            Error("invalid constant [%s.%s]", _stringval(id), _stringval(constid));
                        }
                    }
                    else {
                        constval = constant;
                    }
                    _es.epos = _fs->PushNewTarget();

                    /* generate direct or literal function depending on size */
                    SQObjectType ctype = sq_type(constval);
                    switch(ctype) {
                        case OT_INTEGER: EmitLoadConstInt(_integer(constval),_es.epos); break;
                        case OT_FLOAT: EmitLoadConstFloat(_float(constval),_es.epos); break;
                        case OT_BOOL: _fs->AddInstruction(_OP_LOADBOOL, _es.epos, _integer(constval)); break;
                        default: _fs->AddInstruction(_OP_LOAD,_es.epos,_fs->GetConstant(constval)); break;
                    }
                    _es.etype = EXPR;
                } else {
                    /* Handle a non-local variable, aka a field. Push the 'this' pointer on
                    * the virtual stack (always found in offset 0, so no instruction needs to
                    * be generated), and push the key next. Generate an _OP_LOAD instruction
                    * for the latter. If we are not using the variable as a dref expr, generate
                    * the _OP_GET instruction.
                    */
                    _fs->PushTarget(0);
                    _fs->AddInstruction(_OP_LOAD, _fs->PushNewTarget(), _fs->GetConstant(id));
                    if(NeedGet()) {
                        Emit2ArgsOP(_OP_GET);
                    }
                    _es.etype = OBJECT;
                }
                return _es.epos;
            }
            break;
        case TK_DOUBLE_COLON:  // "::"
            _fs->AddInstruction(_OP_LOADROOT, _fs->PushNewTarget());
            _es.etype = OBJECT;
            lexer_state->token = '.'; /* hack: drop into PrefixExpr, case '.'*/
            _es.epos = -1;
            return _es.epos;
            break;
        case TK_NULL:
            _fs->AddInstruction(_OP_LOADNULLS, _fs->PushNewTarget(),1);
            Lex();
            break;
        case TK_INTEGER: EmitLoadConstInt(lexer_state->uint_value,-1); Lex();  break;
        case TK_FLOAT: EmitLoadConstFloat(lexer_state->float_value,-1); Lex(); break;
        case TK_TRUE: case TK_FALSE:
            _fs->AddInstruction(_OP_LOADBOOL, _fs->PushNewTarget(),lexer_state->token == TK_TRUE?1:0);
            Lex();
            break;
        case '[': {
                _fs->AddInstruction(_OP_NEWOBJ, _fs->PushNewTarget(),0,0,NOT_ARRAY);
                SQInteger apos = _fs->GetCurrentPos();
                SQInteger key = 0;
                Lex();
                while(lexer_state->token != ']') {
                    Expression();
                    if(lexer_state->token == ',') Lex();
                    uint8_t const val = _fs->PopTarget();
                    uint8_t const array = _fs->TopTarget();
                    _fs->AddInstruction(_OP_APPENDARRAY, array, val, AAT_STACK);
                    key++;
                }
                _fs->SetInstructionParam(apos, 1, key);
                Lex();
            }
            break;
        case '{':
            _fs->AddInstruction(_OP_NEWOBJ, _fs->PushNewTarget(),0,0,NOT_TABLE);
            Lex();ParseTableOrClass(',','}');
            break;
        case TK_FUNCTION: FunctionExp();break;
        case '@': FunctionExp(true);break;
        case TK_CLASS: Lex(); ClassExp();break;
        case '-':
            Lex();
            switch(lexer_state->token) {
            case TK_INTEGER: EmitLoadConstInt(-lexer_state->uint_value,-1); Lex(); break;
            case TK_FLOAT: EmitLoadConstFloat(-lexer_state->float_value,-1); Lex(); break;
            default: UnaryOP(_OP_NEG);
            }
            break;
        case '!': Lex(); UnaryOP(_OP_NOT); break;
        case '~':
            Lex();
            if(lexer_state->token == TK_INTEGER)  { EmitLoadConstInt(~lexer_state->uint_value,-1); Lex(); break; }
            UnaryOP(_OP_BWNOT);
            break;
        case TK_TYPEOF : Lex() ;UnaryOP(_OP_TYPEOF); break;
        case TK_RESUME : Lex(); UnaryOP(_OP_RESUME); break;
        case TK_CLONE : Lex(); UnaryOP(_OP_CLONE); break;
        case TK_RAWCALL: Lex(); Expect('('); FunctionCallArgs(true); break;
        case TK_MINUSMINUS :
        case TK_PLUSPLUS :PrefixIncDec(lexer_state->token); break;
        case TK_DELETE : DeleteExpr(); break;
        case '(': Lex(); CommaExpr(); Expect(')');
            break;
        case TK___LINE__: EmitLoadConstInt(lexer_state->current_line,-1); Lex(); break;
        case TK___FILE__: _fs->AddInstruction(_OP_LOAD, _fs->PushNewTarget(), _fs->GetConstant(_sourcename)); Lex(); break;
        default: Error("expression expected");
        }
        _es.etype = EXPR;
        return -1;
    }

    void EmitLoadConstInt(SQInteger value, SQInteger target) {
        if (target < 0) {
            target = _fs->PushNewTarget();
        }
        if (value <= INT_MAX && value > INT_MIN) { //does it fit in 32 bits?
            _fs->AddInstruction(_OP_LOADINT, target, value);
        } else {
            _fs->AddInstruction(_OP_LOAD, target, _fs->GetNumericConstant(value));
        }
    }

    void EmitLoadConstFloat(SQFloat value,SQInteger target)
    {
        if(target < 0) {
            target = _fs->PushNewTarget();
        }
        if(sizeof(SQFloat) == sizeof(SQInt32)) {
            _fs->AddInstruction(_OP_LOADFLOAT, target,*((SQInt32 *)&value));
        }
        else {
            _fs->AddInstruction(_OP_LOAD, target, _fs->GetNumericConstant(value));
        }
    }
    void UnaryOP(SQOpcode op)
    {
        PrefixedExpr();
        if (_fs->_targetstack.size() == 0) {
            Error("cannot evaluate unary operator");
        }
        uint8_t const src = _fs->PopTarget();
        _fs->AddInstruction(op, _fs->PushNewTarget(), src);
    }
    bool NeedGet()
    {
        switch(lexer_state->token) {
        case '=': case '(': case TK_NEWSLOT: case TK_MODEQ: case TK_MULEQ:
        case TK_DIVEQ: case TK_MINUSEQ: case TK_PLUSEQ:
            return false;
        case TK_PLUSPLUS: case TK_MINUSMINUS:
            if (!IsEndOfStatement()) {
                return false;
            }
        break;
        }
        return (!_es.donot_get || ( _es.donot_get && (lexer_state->token == '.' || lexer_state->token == '[')));
    }
    void FunctionCallArgs(bool rawcall = false)
    {
        SQInteger nargs = 1;//this
         while(lexer_state->token != ')') {
             Expression();
             MoveIfCurrentTargetIsLocal();
             nargs++;
             if(lexer_state->token == ','){
                 Lex();
                 if(lexer_state->token == ')') Error("expression expected, found ')'");
             }
         }
         Lex();
         if (rawcall) {
             if (nargs < 3) Error("rawcall requires at least 2 parameters (callee and this)");
             nargs -= 2; //removes callee and this from count
         }
         for(SQInteger i = 0; i < (nargs - 1); i++) _fs->PopTarget();
         uint8_t const stackbase = _fs->PopTarget();
         uint8_t const closure = _fs->PopTarget();
         _fs->AddInstruction(_OP_CALL, _fs->PushNewTarget(), closure, stackbase, nargs);
		 if (lexer_state->token == '{') {
			 uint8_t const retval = _fs->TopTarget();
			 Lex();
			 while (lexer_state->token != '}') {
				 switch (lexer_state->token) {
				 case '[':
					 Lex(); CommaExpr(); Expect(']');
					 Expect('='); Expression();
					 break;
				 default:
					 _fs->AddInstruction(_OP_LOAD, _fs->PushNewTarget(), _fs->GetConstant(Expect(TK_IDENTIFIER)));
					 Expect('='); Expression();
					 break;
				 }
				 if (lexer_state->token == ',') Lex();
				 // nkeys++;
				 uint8_t const val = _fs->PopTarget();
				 uint8_t const key = _fs->PopTarget();
				 _fs->AddInstruction(_OP_SET, 0xFF, retval, key, val);
			 }
			 Lex();
		 }
    }

    void ParseTableOrClass(SQInteger separator, SQInteger terminator) {
        SQInteger tpos = _fs->GetCurrentPos();
        SQInteger nkeys = 0;
        while(lexer_state->token != terminator) {
            bool hasattrs = false;
            bool isstatic = false;
            //check if is an attribute
            if(separator == ';') {
                if(lexer_state->token == TK_ATTR_OPEN) {
                    _fs->AddInstruction(_OP_NEWOBJ, _fs->PushNewTarget(),0,0,NOT_TABLE); Lex();
                    ParseTableOrClass(',',TK_ATTR_CLOSE);
                    hasattrs = true;
                }
                if(lexer_state->token == TK_STATIC) {
                    isstatic = true;
                    Lex();
                }
            }
            switch(lexer_state->token) {
            case TK_FUNCTION:
            case TK_CONSTRUCTOR:{
                SQInteger tk = lexer_state->token;
                Lex();
                SQObject id = tk == TK_FUNCTION ? Expect(TK_IDENTIFIER) : _fs->CreateString("constructor");
				_fs->AddInstruction(_OP_LOAD, _fs->PushNewTarget(), _fs->GetConstant(id));
				SQInteger boundtarget = 0xFF;
				if (lexer_state->token == '[') {
					boundtarget = ParseBindEnv();
				}
                Expect('(');
                
                CreateFunction(id, boundtarget);
                _fs->AddInstruction(_OP_CLOSURE, _fs->PushNewTarget(), _fs->_functions.size() - 1, boundtarget);
                                }
                                break;
            case '[':
                Lex(); CommaExpr(); Expect(']');
                Expect('='); Expression();
                break;
            case TK_STRING_LITERAL: //JSON
                if(separator == ',') { //only works for tables
                    _fs->AddInstruction(_OP_LOAD, _fs->PushNewTarget(), _fs->GetConstant(Expect(TK_STRING_LITERAL)));
                    Expect(':'); Expression();
                    break;
                }
            default :
                _fs->AddInstruction(_OP_LOAD, _fs->PushNewTarget(), _fs->GetConstant(Expect(TK_IDENTIFIER)));
                Expect('='); Expression();
            }
            if(lexer_state->token == separator) Lex();//optional comma/semicolon
            nkeys++;
            uint8_t const val = _fs->PopTarget();
            uint8_t const key = _fs->PopTarget();
            SQInteger attrs = hasattrs ? _fs->PopTarget():-1;
            ((void)attrs);
            assert((hasattrs && (attrs == key-1)) || !hasattrs);
            unsigned char flags = (hasattrs?NEW_SLOT_ATTRIBUTES_FLAG:0)|(isstatic?NEW_SLOT_STATIC_FLAG:0);
            uint8_t const table = _fs->TopTarget(); //<<BECAUSE OF THIS NO COMMON EMIT FUNC IS POSSIBLE
            if(separator == ',') { //hack recognizes a table from the separator
                _fs->AddInstruction(_OP_NEWSLOT, 0xFF, table, key, val);
            }
            else {
                _fs->AddInstruction(_OP_NEWSLOTA, flags, table, key, val); //this for classes only as it invokes _newmember
            }
        }
        if(separator == ',') //hack recognizes a table from the separator
            _fs->SetInstructionParam(tpos, 1, nkeys);
        Lex();
    }
    void LocalDeclStatement()
    {
        SQObject varname;
        Lex();
        if( lexer_state->token == TK_FUNCTION) {
			SQInteger boundtarget = 0xFF;
            Lex();
			varname = Expect(TK_IDENTIFIER);
			if (lexer_state->token == '[') {
				boundtarget = ParseBindEnv();
			}
            Expect('(');
            CreateFunction(varname,0xFF,false);
            _fs->AddInstruction(_OP_CLOSURE, _fs->PushNewTarget(), _fs->_functions.size() - 1, boundtarget);
            _fs->PopTarget();
            _fs->PushLocalVariable(varname);
            return;
        }

        do {
            varname = Expect(TK_IDENTIFIER);
            if(lexer_state->token == '=') {
                Lex(); Expression();
                uint8_t const src = _fs->PopTarget();
                uint8_t const dest = _fs->PushNewTarget();
                if (dest != src) {
                    if (_fs->IsLocal(src)) {
                        _fs->SnoozeOpt();
                    }
                    _fs->AddInstruction(_OP_MOVE, dest, src);
                }
            }
            else{
                _fs->AddInstruction(_OP_LOADNULLS, _fs->PushNewTarget(),1);
            }
            _fs->PopTarget();
            _fs->PushLocalVariable(varname);
            if(lexer_state->token == ',') Lex(); else break;
        } while(1);
    }

    void IfBlock() {
        if (lexer_state->token == '{') {
            BEGIN_SCOPE();
            Lex();
            Statements();
            Expect('}');
            if (true) {
                END_SCOPE();
            } else {
                END_SCOPE_NO_CLOSE();
            }
        } else {
            Statement();
            if (lexer_state->prev_token != '}' && lexer_state->prev_token != ';') {
                OptionalSemicolon();
            }
        }
    }

    void IfStatement() {
        Lex();
        Expect('(');
        CommaExpr();
        Expect(')');

        _fs->AddInstruction(_OP_JZ, _fs->PopTarget());
        SQInteger jnepos = _fs->GetCurrentPos();

        IfBlock();
      
        SQInteger endifblock = _fs->GetCurrentPos();
        bool haselse = false;
        if (lexer_state->token == TK_ELSE) {
            haselse = true;
            _fs->AddInstruction(_OP_JMP);
            SQInteger jmppos = _fs->GetCurrentPos();
            Lex();
            IfBlock();
            _fs->SetInstructionParam(jmppos, 1, _fs->GetCurrentPos() - jmppos);
        }
        _fs->SetInstructionParam(jnepos, 1, endifblock - jnepos + (haselse?1:0));
    }
    void WhileStatement()
    {
        SQInteger jzpos, jmppos;
        jmppos = _fs->GetCurrentPos();
        Lex(); Expect('('); CommaExpr(); Expect(')');

        BEGIN_BREAKBLE_BLOCK();
        _fs->AddInstruction(_OP_JZ, _fs->PopTarget());
        jzpos = _fs->GetCurrentPos();
        BEGIN_SCOPE();

        Statement();

        END_SCOPE();
        _fs->AddInstruction(_OP_JMP, 0, jmppos - _fs->GetCurrentPos() - 1);
        _fs->SetInstructionParam(jzpos, 1, _fs->GetCurrentPos() - jzpos);

        END_BREAKBLE_BLOCK(jmppos);
    }
    void DoWhileStatement()
    {
        Lex();
        SQInteger jmptrg = _fs->GetCurrentPos();
        BEGIN_BREAKBLE_BLOCK()
        BEGIN_SCOPE();
        Statement();
        END_SCOPE();
        Expect(TK_WHILE);
        SQInteger continuetrg = _fs->GetCurrentPos();
        Expect('('); CommaExpr(); Expect(')');
        _fs->AddInstruction(_OP_JZ, _fs->PopTarget(), 1);
        _fs->AddInstruction(_OP_JMP, 0, jmptrg - _fs->GetCurrentPos() - 1);
        END_BREAKBLE_BLOCK(continuetrg);
    }
    void ForStatement()
    {
        Lex();
        BEGIN_SCOPE();
        Expect('(');
        if(lexer_state->token == TK_LOCAL) LocalDeclStatement();
        else if(lexer_state->token != ';'){
            CommaExpr();
            _fs->PopTarget();
        }
        Expect(';');
        _fs->SnoozeOpt();
        SQInteger jmppos = _fs->GetCurrentPos();
        SQInteger jzpos = -1;
        if(lexer_state->token != ';') { CommaExpr(); _fs->AddInstruction(_OP_JZ, _fs->PopTarget()); jzpos = _fs->GetCurrentPos(); }
        Expect(';');
        _fs->SnoozeOpt();
        SQInteger expstart = _fs->GetCurrentPos() + 1;
        if(lexer_state->token != ')') {
            CommaExpr();
            _fs->PopTarget();
        }
        Expect(')');
        _fs->SnoozeOpt();
        SQInteger expend = _fs->GetCurrentPos();
        SQInteger expsize = (expend - expstart) + 1;
        SQInstructionVec exp;
        if(expsize > 0) {
            for(SQInteger i = 0; i < expsize; i++)
                exp.push_back(_fs->GetInstruction(expstart + i));
            _fs->PopInstructions(expsize);
        }
        BEGIN_BREAKBLE_BLOCK()
        Statement();
        SQInteger continuetrg = _fs->GetCurrentPos();
        if(expsize > 0) {
            for(SQInteger i = 0; i < expsize; i++)
                _fs->AddInstruction(exp[i]);
        }
        _fs->AddInstruction(_OP_JMP, 0, jmppos - _fs->GetCurrentPos() - 1, 0);
        if(jzpos>  0) _fs->SetInstructionParam(jzpos, 1, _fs->GetCurrentPos() - jzpos);
        
        END_BREAKBLE_BLOCK(continuetrg);

		END_SCOPE();
    }
    void ForEachStatement()
    {
        SQObject idxname, valname;
        Lex(); Expect('('); valname = Expect(TK_IDENTIFIER);
        if(lexer_state->token == ',') {
            idxname = valname;
            Lex(); valname = Expect(TK_IDENTIFIER);
        }
        else{
            idxname = _fs->CreateString("@INDEX@");
        }
        Expect(TK_IN);

        //save the stack size
        BEGIN_SCOPE();
        //put the table in the stack(evaluate the table expression)
        Expression(); Expect(')');
        uint8_t const container = _fs->TopTarget();
        //push the index local var
        uint8_t const indexpos = _fs->PushLocalVariable(idxname);
        _fs->AddInstruction(_OP_LOADNULLS, indexpos, 1);
        //push the value local var
        uint8_t const valuepos = _fs->PushLocalVariable(valname);
        _fs->AddInstruction(_OP_LOADNULLS, valuepos, 1);
        //push reference index
        uint8_t const itrpos = _fs->PushLocalVariable(_fs->CreateString("@ITERATOR@")); //use invalid id to make it inaccessible
        _fs->AddInstruction(_OP_LOADNULLS, itrpos, 1);
        SQInteger jmppos = _fs->GetCurrentPos();
        _fs->AddInstruction(_OP_FOREACH, container, 0, indexpos);
        SQInteger foreachpos = _fs->GetCurrentPos();
        _fs->AddInstruction(_OP_POSTFOREACH, container, 0, indexpos);
        //generate the statement code
        BEGIN_BREAKBLE_BLOCK()
        Statement();
        _fs->AddInstruction(_OP_JMP, 0, jmppos - _fs->GetCurrentPos() - 1);
        _fs->SetInstructionParam(foreachpos, 1, _fs->GetCurrentPos() - foreachpos);
        _fs->SetInstructionParam(foreachpos + 1, 1, _fs->GetCurrentPos() - foreachpos);
        END_BREAKBLE_BLOCK(foreachpos - 1);
        //restore the local variable stack(remove index,val and ref idx)
        _fs->PopTarget();
        END_SCOPE();
    }
    void SwitchStatement()
    {
        Lex(); Expect('('); CommaExpr(); Expect(')');
        Expect('{');
        uint8_t const expr = _fs->TopTarget();
        bool bfirst = true;
        SQInteger tonextcondjmp = -1;
        SQInteger skipcondjmp = -1;
        SQInteger __nbreaks__ = _fs->_unresolvedbreaks.size();
        _fs->_breaktargets.push_back(0);
        while(lexer_state->token == TK_CASE) {
            if(!bfirst) {
                _fs->AddInstruction(_OP_JMP, 0, 0);
                skipcondjmp = _fs->GetCurrentPos();
                _fs->SetInstructionParam(tonextcondjmp, 1, _fs->GetCurrentPos() - tonextcondjmp);
            }
            //condition
            Lex(); Expression(); Expect(':');
            uint8_t const trg = _fs->PopTarget();
            uint8_t eqtarget = trg;
            bool local = _fs->IsLocal(trg);
            if(local) {
                eqtarget = _fs->PushNewTarget(); //we need to allocate a extra reg
            }
            _fs->AddInstruction(_OP_EQ, eqtarget, trg, expr);
            _fs->AddInstruction(_OP_JZ, eqtarget, 0);
            if(local) {
                _fs->PopTarget();
            }

            //end condition
            if(skipcondjmp != -1) {
                _fs->SetInstructionParam(skipcondjmp, 1, (_fs->GetCurrentPos() - skipcondjmp));
            }
            tonextcondjmp = _fs->GetCurrentPos();
            BEGIN_SCOPE();
            Statements();
            END_SCOPE();
            bfirst = false;
        }
        if(tonextcondjmp != -1)
            _fs->SetInstructionParam(tonextcondjmp, 1, _fs->GetCurrentPos() - tonextcondjmp);
        if(lexer_state->token == TK_DEFAULT) {
            Lex(); Expect(':');
            BEGIN_SCOPE();
            Statements();
            END_SCOPE();
        }
        Expect('}');
        _fs->PopTarget();
        __nbreaks__ = _fs->_unresolvedbreaks.size() - __nbreaks__;
        if(__nbreaks__ > 0)ResolveBreaks(_fs, __nbreaks__);
        _fs->_breaktargets.pop_back();
    }
    void FunctionStatement()
    {
        SQObject id;
        Lex(); id = Expect(TK_IDENTIFIER);
        _fs->PushTarget(0);
        _fs->AddInstruction(_OP_LOAD, _fs->PushNewTarget(), _fs->GetConstant(id));
        if(lexer_state->token == TK_DOUBLE_COLON) Emit2ArgsOP(_OP_GET);

        while(lexer_state->token == TK_DOUBLE_COLON) {
            Lex();
            id = Expect(TK_IDENTIFIER);
            _fs->AddInstruction(_OP_LOAD, _fs->PushNewTarget(), _fs->GetConstant(id));
            if(lexer_state->token == TK_DOUBLE_COLON) Emit2ArgsOP(_OP_GET);
        }
		SQInteger boundtarget = 0xFF;
		if (lexer_state->token == '[') {
			boundtarget = ParseBindEnv();
		}
        Expect('(');
        CreateFunction(id, boundtarget);
        _fs->AddInstruction(_OP_CLOSURE, _fs->PushNewTarget(), _fs->_functions.size() - 1, boundtarget);
        EmitDerefOp(_OP_NEWSLOT);
        _fs->PopTarget();
    }
    void ClassStatement()
    {
        SQExpState es;
        Lex();
        es = _es;
        _es.donot_get = true;
        PrefixedExpr();
        if(_es.etype == EXPR) {
            Error("invalid class name");
        }
        else if(_es.etype == OBJECT || _es.etype == BASE) {
            ClassExp();
            EmitDerefOp(_OP_NEWSLOT);
            _fs->PopTarget();
        }
        else {
            Error("cannot create a class in a local with the syntax(class <local>)");
        }
        _es = es;
    }

    SQObject ExpectScalar() {
        SQObject val;
        val._type = OT_NULL; val._unVal.nInteger = 0; //shut up GCC 4.x
        switch(lexer_state->token) {
            case TK_INTEGER:
                val._type = OT_INTEGER;
                val._unVal.nInteger = lexer_state->uint_value;
                break;
            case TK_FLOAT:
                val._type = OT_FLOAT;
                val._unVal.fFloat = lexer_state->float_value;
                break;
            case TK_STRING_LITERAL:
                val = _fs->CreateString(lexer_state->string_value,lexer_state->string_length);
                break;
            case TK_TRUE:
            case TK_FALSE:
                val._type = OT_BOOL;
                val._unVal.nInteger = lexer_state->token == TK_TRUE ? 1 : 0;
                break;
            case '-':
                Lex();
                switch(lexer_state->token)
                {
                case TK_INTEGER:
                    val._type = OT_INTEGER;
                    val._unVal.nInteger = -lexer_state->uint_value;
                break;
                case TK_FLOAT:
                    val._type = OT_FLOAT;
                    val._unVal.fFloat = -lexer_state->float_value;
                break;
                default:
                    Error("scalar expected : integer, float");
                }
                break;
            default:
                Error("scalar expected : integer, float, or string");
        }
        Lex();
        return val;
    }
    void EnumStatement()
    {
        Lex();
        SQObject id = Expect(TK_IDENTIFIER);
        Expect('{');

        SQObject table = _fs->CreateTable();
        SQInteger nval = 0;
        while(lexer_state->token != '}') {
            SQObject key = Expect(TK_IDENTIFIER);
            SQObject val;
            if(lexer_state->token == '=') {
                Lex();
                val = ExpectScalar();
            }
            else {
                val._type = OT_INTEGER;
                val._unVal.nInteger = nval++;
            }
            _table(table)->NewSlot(SQObjectPtr(key),SQObjectPtr(val));
            if(lexer_state->token == ',') Lex();
        }
        SQTable *enums = _table(_ss(_vm)->_consts);
        SQObjectPtr strongid = id;
        enums->NewSlot(SQObjectPtr(strongid),SQObjectPtr(table));
        strongid.Null();
        Lex();
    }

    void TryCatchStatement() {
        Lex();
        _fs->AddInstruction(_OP_PUSHTRAP,0,0);
        _fs->_traps++;
        if(_fs->_breaktargets.size()) _fs->_breaktargets.top()++;
        if(_fs->_continuetargets.size()) _fs->_continuetargets.top()++;
        SQInteger trappos = _fs->GetCurrentPos();
        {
            BEGIN_SCOPE();
            Statement();
            END_SCOPE();
        }
        _fs->_traps--;
        _fs->AddInstruction(_OP_POPTRAP, 1, 0);
        if(_fs->_breaktargets.size()) _fs->_breaktargets.top()--;
        if(_fs->_continuetargets.size()) _fs->_continuetargets.top()--;
        _fs->AddInstruction(_OP_JMP, 0, 0);
        SQInteger jmppos = _fs->GetCurrentPos();
        _fs->SetInstructionParam(trappos, 1, (_fs->GetCurrentPos() - trappos));

        Expect(TK_CATCH);
        Expect('(');
        SQObject exid = Expect(TK_IDENTIFIER);
        Expect(')');
        {
            BEGIN_SCOPE();
            uint8_t const ex_target = _fs->PushLocalVariable(exid);
            _fs->SetInstructionParam(trappos, 0, ex_target);
            Statement();
            _fs->SetInstructionParams(jmppos, 0, (_fs->GetCurrentPos() - jmppos), 0);
            END_SCOPE();
        }
    }

	uint8_t ParseBindEnv() {
		Lex();
		Expression();
		uint8_t const boundtarget = _fs->TopTarget();
		Expect(']');
		return boundtarget;
	}

    void FunctionExp(bool lambda = false) {
        Lex(); 
		uint8_t boundtarget = 0xFF;
		if (lexer_state->token == '[') {
			boundtarget = ParseBindEnv();
		}
		Expect('(');
        SQObjectPtr dummy;
        CreateFunction(dummy, boundtarget, lambda);
        _fs->AddInstruction(_OP_CLOSURE, _fs->PushNewTarget(), _fs->_functions.size() - 1, boundtarget);
    }

    void ClassExp() {
        // Ok, so base can be negative (reg1)
        // However, attrs are not differentiated from 255
        // Is this a problem? Let's add a panic here and try
        // to construct a panic case
        SQInteger base = -1;
        SQInteger attrs = -1;
        if (lexer_state->token == TK_EXTENDS) {
            Lex();
            Expression();
            base = _fs->TopTarget();
        }

        if (lexer_state->token == TK_ATTR_OPEN) {
            Lex();
            _fs->AddInstruction(_OP_NEWOBJ, _fs->PushNewTarget(),0,0,NOT_TABLE);
            ParseTableOrClass(',',TK_ATTR_CLOSE);
            attrs = _fs->TopTarget();
            if (attrs == 0xff) {
                Error("internal compiler error: too many locals for attribute definition");
            }
        }

        Expect('{');

        if (attrs != -1) {
            _fs->PopTarget();
        }
        if (base != -1) {
            _fs->PopTarget();
        }
        _fs->AddInstruction(_OP_NEWOBJ, _fs->PushNewTarget(), base, attrs, NOT_CLASS);

        ParseTableOrClass(';','}');
    }

    void DeleteExpr()
    {
        SQExpState es;
        Lex();
        es = _es;
        _es.donot_get = true;
        PrefixedExpr();
        if(_es.etype==EXPR) Error("can't delete an expression");
        if(_es.etype==BASE) Error("can't delete 'base'");
        if(_es.etype==OBJECT) {
            Emit2ArgsOP(_OP_DELETE);
        }
        else {
            Error("cannot delete an (outer) local");
        }
        _es = es;
    }
    void PrefixIncDec(SQInteger token)
    {
        SQExpState  es;
        SQInteger diff = (token==TK_MINUSMINUS) ? -1 : 1;
        Lex();
        es = _es;
        _es.donot_get = true;
        PrefixedExpr();
        if(_es.etype==EXPR) {
            Error("can't '++' or '--' an expression");
        }
        else if (_es.etype == BASE) {
            Error("can't '++' or '--' a base");
        }
        else if(_es.etype==OBJECT) {
            Emit2ArgsOP(_OP_INC, diff);
        }
        else if(_es.etype==LOCAL) {
            uint8_t const src = _fs->TopTarget();
            _fs->AddInstruction(_OP_INCL, src, src, 0, diff);

        }
        else if(_es.etype==OUTER) {
            uint8_t tmp = _fs->PushNewTarget();
            _fs->AddInstruction(_OP_GETOUTER, tmp, _es.epos);
            _fs->AddInstruction(_OP_INCL,     tmp, tmp, 0, diff);
            _fs->AddInstruction(_OP_SETOUTER, tmp, _es.epos, tmp);
        }
        _es = es;
    }
    void CreateFunction(SQObject &name,SQInteger boundtarget,bool lambda = false)
    {
        SQFuncState *funcstate = _fs->PushChildState(_ss(_vm));
        funcstate->_name = name;
        SQObject paramname;
        funcstate->AddParameter(_fs->CreateString("this"));
        funcstate->_sourcename = _sourcename;
        SQInteger defparams = 0;
        while(lexer_state->token!=')') {
            if(lexer_state->token == TK_VARPARAMS) {
                if(defparams > 0) Error("function with default parameters cannot have variable number of parameters");
                funcstate->AddParameter(_fs->CreateString("vargv"));
                funcstate->_varparams = true;
                Lex();
                if(lexer_state->token != ')') Error("expected ')'");
                break;
            }
            else {
                paramname = Expect(TK_IDENTIFIER);
                funcstate->AddParameter(paramname);
                if(lexer_state->token == '=') {
                    Lex();
                    Expression();
                    funcstate->AddDefaultParam(_fs->TopTarget());
                    defparams++;
                }
                else {
                    if(defparams > 0) Error("expected '='");
                }
                if(lexer_state->token == ',') Lex();
                else if(lexer_state->token != ')') Error("expected ')' or ','");
            }
        }
        Expect(')');
		if (boundtarget != 0xFF) {
			_fs->PopTarget();
		}
        for(SQInteger n = 0; n < defparams; n++) {
            _fs->PopTarget();
        }

        SQFuncState *currchunk = _fs;
        _fs = funcstate;
        if(lambda) {
            Expression();
            _fs->AddInstruction(_OP_RETURN, 1, _fs->PopTarget());}
        else {
            Statement(false);
        }
        funcstate->AddLineInfos(
            lexer_state->prev_token == '\n'
                ? lexer_state->last_token_line
                : lexer_state->current_line,
            _lineinfo,
            true);
        funcstate->AddInstruction(_OP_RETURN, -1);
        funcstate->SetStackSize(0);

        SQFunctionProto *func = funcstate->BuildProto();
#ifdef _DEBUG_DUMP
        funcstate->Dump(func);
#endif
        _fs = currchunk;
        _fs->_functions.push_back(func);
        _fs->PopChildState();
    }
    void ResolveBreaks(SQFuncState *funcstate, SQInteger ntoresolve)
    {
        while(ntoresolve > 0) {
            SQInteger pos = funcstate->_unresolvedbreaks.back();
            funcstate->_unresolvedbreaks.pop_back();
            //set the jmp instruction
            funcstate->SetInstructionParams(pos, 0, funcstate->GetCurrentPos() - pos, 0);
            ntoresolve--;
        }
    }
    void ResolveContinues(SQFuncState *funcstate, SQInteger ntoresolve, SQInteger targetpos)
    {
        while(ntoresolve > 0) {
            SQInteger pos = funcstate->_unresolvedcontinues.back();
            funcstate->_unresolvedcontinues.pop_back();
            //set the jmp instruction
            funcstate->SetInstructionParams(pos, 0, targetpos - pos, 0);
            ntoresolve--;
        }
    }
};

bool Compile(SQVM *vm,SQLEXREADFUNC rg, SQUserPointer up, const SQChar *sourcename, SQObjectPtr &out, bool raiseerror, bool lineinfo)
{
    SQCompiler p(vm, rg, up, sourcename, raiseerror, lineinfo);
    return p.Compile(out);
}

#endif
