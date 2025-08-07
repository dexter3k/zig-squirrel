#pragma once

#include <cstdint>

#include "squtils.h"

class SQFuncState {
    CompilerErrorFunc _errfunc;
    void *_errtarget;
public:
    SQInteger _returnexp;
    sqvector<SQLocalVarInfo> _vlocals;
    sqvector<uint8_t> _targetstack;
    uint16_t _stacksize;
    bool _varparams;
    bool _bgenerator;
    sqvector<SQInteger> _unresolvedbreaks;
    sqvector<SQInteger> _unresolvedcontinues;
    sqvector<SQObjectPtr> _functions;
    sqvector<SQObjectPtr> _parameters;
    SQOuterVarVec _outervalues;
    SQInstructionVec _instructions;
    sqvector<SQLocalVarInfo> _localvarinfos;
    SQObjectPtr _literals;
    SQObjectPtr _strings;
    SQObjectPtr _name;
    SQObjectPtr _sourcename;
    SQInteger _nliterals;
    SQLineInfoVec _lineinfos;
    SQFuncState *_parent;
    sqvector<SQInteger> _scope_blocks;
    sqvector<SQInteger> _breaktargets;
    sqvector<SQInteger> _continuetargets;
    sqvector<SQInteger> _defaultparams;
    SQInteger _lastline;
    SQInteger _traps; //contains number of nested exception traps
    SQInteger _outers;
    bool _optimization;
    SQSharedState *_sharedstate;
    sqvector<SQFuncState*> _childstates;

    SQFuncState(SQSharedState *ss,SQFuncState *parent,CompilerErrorFunc efunc,void *ed);
    ~SQFuncState();
#ifdef _DEBUG_DUMP
    void Dump(SQFunctionProto *func);
#endif
    void Error(const SQChar *err);
    SQFuncState *PushChildState(SQSharedState *ss);
    void PopChildState();
    void AddInstruction(SQOpcode _op, uint8_t arg0 = 0, int32_t arg1=0, uint8_t arg2=0, uint8_t arg3=0) {
        SQInstruction i(_op, arg0, arg1, arg2, arg3);
        AddInstruction(i);
    }
    void AddInstruction(SQInstruction &i);
    void SetInstructionParams(SQInteger pos,SQInteger arg0,SQInteger arg1,SQInteger arg2=0,SQInteger arg3=0);
    void SetInstructionParam(SQInteger pos,SQInteger arg,SQInteger val);
    SQInstruction &GetInstruction(SQInteger pos){return _instructions[pos];}
    void PopInstructions(SQInteger size){for(SQInteger i=0;i<size;i++)_instructions.pop_back();}
    void SetStackSize(SQInteger n);
    SQInteger CountOuters(SQInteger stacksize);
    void SnoozeOpt(){_optimization=false;}
    void AddDefaultParam(SQInteger trg) { _defaultparams.push_back(trg); }
    SQInteger GetDefaultParamCount() { return _defaultparams.size(); }
    SQInteger GetCurrentPos() { return _instructions.size() - 1; }
    SQInteger GetNumericConstant(const SQInteger cons);
    SQInteger GetNumericConstant(const SQFloat cons);
    uint8_t PushLocalVariable(const SQObject &name);
    void AddParameter(const SQObject &name);
    SQInteger GetLocalVariable(const SQObject &name);
    void MarkLocalAsOuter(SQInteger pos);
    SQInteger GetOuterVariable(const SQObject &name);
    SQInteger GenerateCode();
    uint16_t GetStackSize();
    SQInteger CalcStackFrameSize();
    void AddLineInfos(SQInteger line,bool lineop,bool force=false);
    SQFunctionProto *BuildProto();
    uint8_t PushNewTarget();
    void PushTarget(uint8_t n);
    uint8_t PopTarget();
    uint8_t TopTarget();
    void DiscardTarget();
    bool IsLocal(SQUnsignedInteger stkpos);
    SQObject CreateString(const SQChar *s,SQInteger len = -1);
    SQObject CreateTable();
    bool IsConstant(const SQObject &name,SQObject &e);
    SQInteger GetConstant(const SQObject &cons);
private:
    uint8_t AllocStackPos();
};
