#include "SQString.hpp"
#include "SQGenerator.hpp"
#include "SQArray.hpp"
#include "SQTable.hpp"
#include "SQInstance.hpp"
#include "SQUserData.hpp"
#include "SQFunctionProto.hpp"
#include "SQClass.hpp"
#include "SQClosure.hpp"

const SQChar *IdType2Name(SQObjectType type)
{
    switch(_RAW_TYPE(type))
    {
    case _RT_NULL:return _SC("null");
    case _RT_INTEGER:return _SC("integer");
    case _RT_FLOAT:return _SC("float");
    case _RT_BOOL:return _SC("bool");
    case _RT_STRING:return _SC("string");
    case _RT_TABLE:return _SC("table");
    case _RT_ARRAY:return _SC("array");
    case _RT_GENERATOR:return _SC("generator");
    case _RT_CLOSURE:
    case _RT_NATIVECLOSURE:
        return _SC("function");
    case _RT_USERDATA:
    case _RT_USERPOINTER:
        return _SC("userdata");
    case _RT_THREAD: return _SC("thread");
    case _RT_FUNCPROTO: return _SC("function");
    case _RT_CLASS: return _SC("class");
    case _RT_INSTANCE: return _SC("instance");
    case _RT_WEAKREF: return _SC("weakref");
    case _RT_OUTER: return _SC("outer");
    default:
        return NULL;
    }
}

const SQChar *GetTypeName(const SQObjectPtr &obj1) {
    return IdType2Name(sq_type(obj1));
}

SQUnsignedInteger TranslateIndex(SQObjectPtr const & idx) {
    switch(sq_type(idx)) {
    case OT_NULL:
        return 0;
    case OT_INTEGER:
        return (SQUnsignedInteger)_integer(idx);
    default:
        assert(0);
        break;
    }
    return 0;
}

SQWeakRef *SQRefCounted::GetWeakRef(SQObjectType type) {
    if(!_weakref) {
        _weakref = (SQWeakRef *)sq_vm_malloc(sizeof(SQWeakRef));
        new (_weakref) SQWeakRef;

        // Why, though? Leftover bits will never be accessed anyways...?
        // Because comparison by value compares entire region
#if defined(SQUSEDOUBLE) && !defined(_SQ64)
        _weakref->_obj._unVal.raw = 0; //clean the whole union on 32 bits with double
#endif

        _weakref->_obj._type = type;
        _weakref->_obj._unVal.pRefCounted = this;
    }
    return _weakref;
}

SQRefCounted::~SQRefCounted() {
    if (_weakref) {
        _weakref->_obj._type = OT_NULL;
        _weakref->_obj._unVal.pRefCounted = NULL;
    }
}

const SQChar* SQFunctionProto::GetLocal(SQVM *vm,SQUnsignedInteger stackbase,SQUnsignedInteger nseq,SQUnsignedInteger nop)
{
    SQUnsignedInteger nvars=_nlocalvarinfos;
    const SQChar *res=NULL;
    if(nvars>=nseq){
        for(SQUnsignedInteger i=0;i<nvars;i++){
            if(_localvarinfos[i]._start_op<=nop && _localvarinfos[i]._end_op>=nop)
            {
                if(nseq==0){
                    vm->Push(vm->_stack[stackbase+_localvarinfos[i]._pos]);
                    res=_stringval(_localvarinfos[i]._name);
                    break;
                }
                nseq--;
            }
        }
    }
    return res;
}

SQInteger SQFunctionProto::GetLine(SQInstruction *curr)
{
    SQInteger op = (SQInteger)(curr-_instructions);
    SQInteger line=_lineinfos[0]._line;
    SQInteger low = 0;
    SQInteger high = _nlineinfos - 1;
    size_t mid = 0;
    while(low <= high)
    {
        mid = low + ((high - low) >> 1);
        SQInteger curop = _lineinfos[mid]._op;
        if(curop > op)
        {
            high = mid - 1;
        }
        else if(curop < op) {
            if(mid < (_nlineinfos - 1)
                && _lineinfos[mid + 1]._op >= op) {
                break;
            }
            low = mid + 1;
        }
        else { //equal
            break;
        }
    }

    while(mid > 0 && _lineinfos[mid]._op >= op) mid--;

    line = _lineinfos[mid]._line;

    return line;
}

SQClosure::~SQClosure() {
    assert(_root);
    _root->DecreaseRefCount();

    if (_env) {
        _env->DecreaseRefCount();
    }

    if (_base) {
        _base->DecreaseRefCount();
    }
}

#define _CHECK_IO(exp)  { if(!exp)return false; }
bool SafeWrite(HSQUIRRELVM v,SQWRITEFUNC write,SQUserPointer up,SQUserPointer dest,SQInteger size)
{
    if(write(up,dest,size) != size) {
        v->Raise_Error(_SC("io error (write function failure)"));
        return false;
    }
    return true;
}

bool SafeRead(HSQUIRRELVM v,SQREADFUNC read,SQUserPointer up,SQUserPointer dest,SQInteger size)
{
    if(size && read(up,dest,size) != size) {
        v->Raise_Error(_SC("io error, read function failure, the origin stream could be corrupted/trucated"));
        return false;
    }
    return true;
}

bool WriteTag(HSQUIRRELVM v,SQWRITEFUNC write,SQUserPointer up,SQUnsignedInteger32 tag)
{
    return SafeWrite(v,write,up,&tag,sizeof(tag));
}

bool CheckTag(HSQUIRRELVM v,SQREADFUNC read,SQUserPointer up,SQUnsignedInteger32 tag)
{
    SQUnsignedInteger32 t;
    _CHECK_IO(SafeRead(v,read,up,&t,sizeof(t)));
    if(t != tag){
        v->Raise_Error(_SC("invalid or corrupted closure stream"));
        return false;
    }
    return true;
}

bool WriteObject(HSQUIRRELVM v,SQUserPointer up,SQWRITEFUNC write,SQObjectPtr &o)
{
    SQUnsignedInteger32 _type = (SQUnsignedInteger32)sq_type(o);
    _CHECK_IO(SafeWrite(v,write,up,&_type,sizeof(_type)));
    switch(sq_type(o)){
    case OT_STRING:
        _CHECK_IO(SafeWrite(v,write,up,&_string(o)->_len,sizeof(SQInteger)));
        _CHECK_IO(SafeWrite(v,write,up,_stringval(o),sq_rsl(_string(o)->_len)));
        break;
    case OT_BOOL:
    case OT_INTEGER:
        _CHECK_IO(SafeWrite(v,write,up,&_integer(o),sizeof(SQInteger)));break;
    case OT_FLOAT:
        _CHECK_IO(SafeWrite(v,write,up,&_float(o),sizeof(SQFloat)));break;
    case OT_NULL:
        break;
    default:
        v->Raise_Error(_SC("cannot serialize a %s"),GetTypeName(o));
        return false;
    }
    return true;
}

bool ReadObject(HSQUIRRELVM v, SQUserPointer up, SQREADFUNC read, SQObjectPtr & o) {
    SQUnsignedInteger32 _type;
    _CHECK_IO(SafeRead(v, read, up, &_type, sizeof(_type)));

    SQObjectType t = (SQObjectType)_type;
    switch (t) {
    case OT_STRING:{
        SQInteger len;
        _CHECK_IO(SafeRead(v,read,up,&len,sizeof(SQInteger)));
        _CHECK_IO(SafeRead(v,read,up,_ss(v)->GetScratchPad(sq_rsl(len)),sq_rsl(len)));
        o = v->_sharedstate->gc.AddString(v->_sharedstate->GetScratchPad(0), len);
        break;
    }
    case OT_INTEGER:{
        SQInteger i;
        _CHECK_IO(SafeRead(v,read,up,&i,sizeof(SQInteger))); o = i; break;
                    }
    case OT_BOOL:{
        SQInteger i;
        _CHECK_IO(SafeRead(v,read,up,&i,sizeof(SQInteger))); o._type = OT_BOOL; o._unVal.nInteger = i; break;
                    }
    case OT_FLOAT:{
        SQFloat f;
        _CHECK_IO(SafeRead(v,read,up,&f,sizeof(SQFloat))); o = f; break;
                  }
    case OT_NULL:
        o.Null();
        break;
    default:
        v->Raise_Error("cannot serialize a %s",IdType2Name(t));
        return false;
    }
    return true;
}

#define SQ_CLOSURESTREAM_HEAD (('S'<<24)|('Q'<<16)|('I'<<8)|('R'))
#define SQ_CLOSURESTREAM_PART (('P'<<24)|('A'<<16)|('R'<<8)|('T'))
#define SQ_CLOSURESTREAM_TAIL (('T'<<24)|('A'<<16)|('I'<<8)|('L'))

bool SQClosure::Save(SQVM *v,SQUserPointer up,SQWRITEFUNC write)
{
    _CHECK_IO(WriteTag(v,write,up,SQ_CLOSURESTREAM_HEAD));
    _CHECK_IO(WriteTag(v,write,up,sizeof(SQChar)));
    _CHECK_IO(WriteTag(v,write,up,sizeof(SQInteger)));
    _CHECK_IO(WriteTag(v,write,up,sizeof(SQFloat)));
    _CHECK_IO(_function->Save(v,up,write));
    _CHECK_IO(WriteTag(v,write,up,SQ_CLOSURESTREAM_TAIL));
    return true;
}

bool SQClosure::Load(SQVM *v,SQUserPointer up,SQREADFUNC read,SQObjectPtr &ret)
{
    _CHECK_IO(CheckTag(v,read,up,SQ_CLOSURESTREAM_HEAD));
    _CHECK_IO(CheckTag(v,read,up,sizeof(SQChar)));
    _CHECK_IO(CheckTag(v,read,up,sizeof(SQInteger)));
    _CHECK_IO(CheckTag(v,read,up,sizeof(SQFloat)));
    SQObjectPtr func;
    _CHECK_IO(SQFunctionProto::Load(v,up,read,func));
    _CHECK_IO(CheckTag(v,read,up,SQ_CLOSURESTREAM_TAIL));
    ret = SQClosure::Create(_ss(v),_funcproto(func),_table(v->_roottable)->GetWeakRef(OT_TABLE));
    //FIXME: load an root for this closure
    return true;
}

SQFunctionProto::SQFunctionProto(SQSharedState *ss)
    : CHAINABLE_OBJ(ss)
{
    _stacksize=0;
    _bgenerator=false;
}

bool SQFunctionProto::Save(SQVM *v,SQUserPointer up,SQWRITEFUNC write)
{
    SQInteger i,nliterals = _nliterals,nparameters = _nparameters;
    SQInteger noutervalues = _noutervalues,nlocalvarinfos = _nlocalvarinfos;
    SQInteger nlineinfos=_nlineinfos,ninstructions = _ninstructions,nfunctions=_nfunctions;
    SQInteger ndefaultparams = _ndefaultparams;
    _CHECK_IO(WriteTag(v,write,up,SQ_CLOSURESTREAM_PART));
    _CHECK_IO(WriteObject(v,up,write,_sourcename));
    _CHECK_IO(WriteObject(v,up,write,_name));
    _CHECK_IO(WriteTag(v,write,up,SQ_CLOSURESTREAM_PART));
    _CHECK_IO(SafeWrite(v,write,up,&nliterals,sizeof(nliterals)));
    _CHECK_IO(SafeWrite(v,write,up,&nparameters,sizeof(nparameters)));
    _CHECK_IO(SafeWrite(v,write,up,&noutervalues,sizeof(noutervalues)));
    _CHECK_IO(SafeWrite(v,write,up,&nlocalvarinfos,sizeof(nlocalvarinfos)));
    _CHECK_IO(SafeWrite(v,write,up,&nlineinfos,sizeof(nlineinfos)));
    _CHECK_IO(SafeWrite(v,write,up,&ndefaultparams,sizeof(ndefaultparams)));
    _CHECK_IO(SafeWrite(v,write,up,&ninstructions,sizeof(ninstructions)));
    _CHECK_IO(SafeWrite(v,write,up,&nfunctions,sizeof(nfunctions)));
    _CHECK_IO(WriteTag(v,write,up,SQ_CLOSURESTREAM_PART));
    for(i=0;i<nliterals;i++){
        _CHECK_IO(WriteObject(v,up,write,_literals[i]));
    }

    _CHECK_IO(WriteTag(v,write,up,SQ_CLOSURESTREAM_PART));
    for(i=0;i<nparameters;i++){
        _CHECK_IO(WriteObject(v,up,write,_parameters[i]));
    }

    _CHECK_IO(WriteTag(v,write,up,SQ_CLOSURESTREAM_PART));
    for(i=0;i<noutervalues;i++){
        _CHECK_IO(SafeWrite(v,write,up,&_outervalues[i]._type,sizeof(SQUnsignedInteger)));
        _CHECK_IO(WriteObject(v,up,write,_outervalues[i]._src));
        _CHECK_IO(WriteObject(v,up,write,_outervalues[i]._name));
    }

    _CHECK_IO(WriteTag(v,write,up,SQ_CLOSURESTREAM_PART));
    for(i=0;i<nlocalvarinfos;i++){
        SQLocalVarInfo &lvi=_localvarinfos[i];
        _CHECK_IO(WriteObject(v,up,write,lvi._name));
        _CHECK_IO(SafeWrite(v,write,up,&lvi._pos,sizeof(SQUnsignedInteger)));
        _CHECK_IO(SafeWrite(v,write,up,&lvi._start_op,sizeof(SQUnsignedInteger)));
        _CHECK_IO(SafeWrite(v,write,up,&lvi._end_op,sizeof(SQUnsignedInteger)));
    }

    _CHECK_IO(WriteTag(v,write,up,SQ_CLOSURESTREAM_PART));
    _CHECK_IO(SafeWrite(v,write,up,_lineinfos,sizeof(SQLineInfo)*nlineinfos));

    _CHECK_IO(WriteTag(v,write,up,SQ_CLOSURESTREAM_PART));
    _CHECK_IO(SafeWrite(v,write,up,_defaultparams,sizeof(SQInteger)*ndefaultparams));

    _CHECK_IO(WriteTag(v,write,up,SQ_CLOSURESTREAM_PART));
    _CHECK_IO(SafeWrite(v,write,up,_instructions,sizeof(SQInstruction)*ninstructions));

    _CHECK_IO(WriteTag(v,write,up,SQ_CLOSURESTREAM_PART));
    for(i=0;i<nfunctions;i++){
        _CHECK_IO(_funcproto(_functions[i])->Save(v,up,write));
    }
    _CHECK_IO(SafeWrite(v,write,up,&_stacksize,sizeof(_stacksize)));
    _CHECK_IO(SafeWrite(v,write,up,&_bgenerator,sizeof(_bgenerator)));
    _CHECK_IO(SafeWrite(v,write,up,&_varparams,sizeof(_varparams)));
    return true;
}

bool SQFunctionProto::Load(SQVM *v,SQUserPointer up,SQREADFUNC read,SQObjectPtr &ret)
{
    SQInteger i, nliterals,nparameters;
    SQInteger noutervalues ,nlocalvarinfos ;
    SQInteger nlineinfos,ninstructions ,nfunctions,ndefaultparams ;
    SQObjectPtr sourcename, name;
    SQObjectPtr o;
    _CHECK_IO(CheckTag(v,read,up,SQ_CLOSURESTREAM_PART));
    _CHECK_IO(ReadObject(v, up, read, sourcename));
    _CHECK_IO(ReadObject(v, up, read, name));

    _CHECK_IO(CheckTag(v,read,up,SQ_CLOSURESTREAM_PART));
    _CHECK_IO(SafeRead(v,read,up, &nliterals, sizeof(nliterals)));
    _CHECK_IO(SafeRead(v,read,up, &nparameters, sizeof(nparameters)));
    _CHECK_IO(SafeRead(v,read,up, &noutervalues, sizeof(noutervalues)));
    _CHECK_IO(SafeRead(v,read,up, &nlocalvarinfos, sizeof(nlocalvarinfos)));
    _CHECK_IO(SafeRead(v,read,up, &nlineinfos, sizeof(nlineinfos)));
    _CHECK_IO(SafeRead(v,read,up, &ndefaultparams, sizeof(ndefaultparams)));
    _CHECK_IO(SafeRead(v,read,up, &ninstructions, sizeof(ninstructions)));
    _CHECK_IO(SafeRead(v,read,up, &nfunctions, sizeof(nfunctions)));


    SQFunctionProto *f = SQFunctionProto::Create(_opt_ss(v),ninstructions,nliterals,nparameters,
            nfunctions,noutervalues,nlineinfos,nlocalvarinfos,ndefaultparams);
    SQObjectPtr proto = f; //gets a ref in case of failure
    f->_sourcename = sourcename;
    f->_name = name;

    _CHECK_IO(CheckTag(v,read,up,SQ_CLOSURESTREAM_PART));

    for(i = 0;i < nliterals; i++){
        _CHECK_IO(ReadObject(v, up, read, o));
        f->_literals[i] = o;
    }
    _CHECK_IO(CheckTag(v,read,up,SQ_CLOSURESTREAM_PART));

    for(i = 0; i < nparameters; i++){
        _CHECK_IO(ReadObject(v, up, read, o));
        f->_parameters[i] = o;
    }
    _CHECK_IO(CheckTag(v,read,up,SQ_CLOSURESTREAM_PART));

    for(i = 0; i < noutervalues; i++){
        SQUnsignedInteger type;
        SQObjectPtr name;
        _CHECK_IO(SafeRead(v,read,up, &type, sizeof(SQUnsignedInteger)));
        _CHECK_IO(ReadObject(v, up, read, o));
        _CHECK_IO(ReadObject(v, up, read, name));
        f->_outervalues[i] = SQOuterVar(name,o, (SQOuterType)type);
    }
    _CHECK_IO(CheckTag(v,read,up,SQ_CLOSURESTREAM_PART));

    for(i = 0; i < nlocalvarinfos; i++){
        SQLocalVarInfo lvi;
        _CHECK_IO(ReadObject(v, up, read, lvi._name));
        _CHECK_IO(SafeRead(v,read,up, &lvi._pos, sizeof(SQUnsignedInteger)));
        _CHECK_IO(SafeRead(v,read,up, &lvi._start_op, sizeof(SQUnsignedInteger)));
        _CHECK_IO(SafeRead(v,read,up, &lvi._end_op, sizeof(SQUnsignedInteger)));
        f->_localvarinfos[i] = lvi;
    }
    _CHECK_IO(CheckTag(v,read,up,SQ_CLOSURESTREAM_PART));
    _CHECK_IO(SafeRead(v,read,up, f->_lineinfos, sizeof(SQLineInfo)*nlineinfos));

    _CHECK_IO(CheckTag(v,read,up,SQ_CLOSURESTREAM_PART));
    _CHECK_IO(SafeRead(v,read,up, f->_defaultparams, sizeof(SQInteger)*ndefaultparams));

    _CHECK_IO(CheckTag(v,read,up,SQ_CLOSURESTREAM_PART));
    _CHECK_IO(SafeRead(v,read,up, f->_instructions, sizeof(SQInstruction)*ninstructions));

    _CHECK_IO(CheckTag(v,read,up,SQ_CLOSURESTREAM_PART));
    for(i = 0; i < nfunctions; i++){
        _CHECK_IO(_funcproto(o)->Load(v, up, read, o));
        f->_functions[i] = o;
    }
    _CHECK_IO(SafeRead(v,read,up, &f->_stacksize, sizeof(f->_stacksize)));
    _CHECK_IO(SafeRead(v,read,up, &f->_bgenerator, sizeof(f->_bgenerator)));
    _CHECK_IO(SafeRead(v,read,up, &f->_varparams, sizeof(f->_varparams)));

    ret = f;
    return true;
}

#ifndef NO_GARBAGE_COLLECTOR

void SQTable::Mark(SQCollectable **chain)
{
    START_MARK()
        if(_delegate) _delegate->Mark(chain);
        SQInteger len = _numofnodes;
        for(SQInteger i = 0; i < len; i++){
            GC::MarkObject(_nodes[i].key, chain);
            GC::MarkObject(_nodes[i].val, chain);
        }
    END_MARK()
}

void SQClass::Mark(SQCollectable **chain)
{
    START_MARK()
        _members->Mark(chain);
        if(_base) _base->Mark(chain);
        GC::MarkObject(_attributes, chain);
        for(SQUnsignedInteger i =0; i< _defaultvalues.size(); i++) {
            GC::MarkObject(_defaultvalues[i].val, chain);
            GC::MarkObject(_defaultvalues[i].attrs, chain);
        }
        for(SQUnsignedInteger j =0; j< _methods.size(); j++) {
            GC::MarkObject(_methods[j].val, chain);
            GC::MarkObject(_methods[j].attrs, chain);
        }
        for(SQUnsignedInteger k =0; k< MT_LAST; k++) {
            GC::MarkObject(_metamethods[k], chain);
        }
    END_MARK()
}

void SQFunctionProto::Mark(SQCollectable **chain)
{
    START_MARK()
        for(size_t i = 0; i < _nliterals; i++) GC::MarkObject(_literals[i], chain);
        for(size_t k = 0; k < _nfunctions; k++) GC::MarkObject(_functions[k], chain);
    END_MARK()
}

void SQClosure::Mark(SQCollectable **chain)
{
    START_MARK()
        if(_base) _base->Mark(chain);
        SQFunctionProto *fp = _function;
        fp->Mark(chain);
        for(size_t i = 0; i < fp->_noutervalues; i++) GC::MarkObject(_outervalues[i], chain);
        for(size_t k = 0; k < fp->_ndefaultparams; k++) GC::MarkObject(_defaultparams[k], chain);
    END_MARK()
}

#endif

