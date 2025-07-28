#include "string_table.h"

#include <cassert>
#include <new>

#include "sqstring.h"
#include "sqobject.h"

static inline SQHash _hashstr2(const SQChar* as, size_t al, const SQChar* bs, size_t bl) {
    size_t l = al + bl;
    SQHash h = (SQHash)l;

    // if string is too long, don't hash all its chars
    SQInteger step = (SQInteger)((l >> 5) + 1);

    SQInteger l1 = (SQInteger)l;
    for (; l1 >= step; l1 -= step) {
        SQInteger idx = l1 - 1 - al;
        if (idx < 0) {
            break;
        }
        h = h ^ ((h << 5) + (h >> 2) + ((unsigned short)bs[idx]));
    }
    for (; l1 >= step; l1 -= step) {
        SQInteger idx = l1 - 1;
        h = h ^ ((h << 5) + (h >> 2) + ((unsigned short)as[idx]));
    }
    return h;
}

SQString* SQStringTable::Concat(const SQChar* a, SQInteger alen, const SQChar* b, SQInteger blen)
{
    SQHash newhash = ::_hashstr2(a, alen, b, blen);
    SQHash h = newhash & (_numofslots - 1);
    SQString* s;
    SQInteger len = alen + blen;
    for (s = strings[h]; s; s = s->_next) {
        if (s->_len == len) {
            if ((!memcmp(a, s->_val, sq_rsl(alen)))
                && (!memcmp(b, &s->_val[alen], sq_rsl(blen)))) {
                return s; //found
            }
        }
    }
    SQString* t = (SQString*)sq_vm_malloc(sq_rsl(len) + sizeof(SQString));
    new (t) SQString;
    t->_sharedstate = _sharedstate;
    memcpy(t->_val, a, sq_rsl(alen));
    memcpy(&t->_val[alen], b, sq_rsl(blen));
    t->_val[len] = _SC('\0');
    t->_len = len;
    t->_hash = newhash;
    t->_next = strings[h];
    strings[h] = t;
    _slotused++;
    if (_slotused > _numofslots) {
        Resize(_numofslots * 2);
    }
    return t;
}

SQString *SQStringTable::Add(const SQChar *news,SQInteger len)
{
    if(len<0)
        len = (SQInteger)scstrlen(news);
    SQHash newhash = ::_hashstr(news,len);
    SQHash h = newhash&(_numofslots-1);
    SQString *s;
    for (s = strings[h]; s; s = s->_next){
        if(s->_len == len && (!memcmp(news, s->_val, len)))
            return s; //found
    }

    SQString *t = (SQString *)sq_vm_malloc(len + sizeof(SQString));
    new (t) SQString;
    t->_sharedstate = _sharedstate;
    memcpy(t->_val,news,sq_rsl(len));
    t->_val[len] = _SC('\0');
    t->_len = len;
    t->_hash = newhash;
    t->_next = strings[h];
    strings[h] = t;
    _slotused++;
    if (_slotused > _numofslots)  /* too crowded? */
        Resize(_numofslots*2);
    return t;
}

void SQStringTable::Resize(SQInteger size) {
    SQInteger oldsize = _numofslots;
    SQString **oldtable = strings;

    AllocNodes(size);

    for (SQInteger i=0; i<oldsize; i++){
        SQString *p = oldtable[i];
        while(p){
            SQString *next = p->_next;
            SQHash h = p->_hash&(_numofslots-1);
            p->_next = strings[h];
            strings[h] = p;
            p = next;
        }
    }

    sq_vm_free(oldtable, sizeof(SQString *) * oldsize);
}

void SQStringTable::Remove(SQString const * bs) {
    SQHash const h = bs->_hash & (_numofslots - 1);

    SQString * prev = nullptr;
    SQString * s = strings[h];
    while (s) {
        if (s != bs) {
            prev = s;
            s = s->_next;
        }
        
        if (prev) {
            prev->_next = s->_next;
        } else {
            strings[h] = s->_next;
        }

        _slotused--;

        SQInteger slen = s->_len;
        s->~SQString(); // invalidate weakrefs
        sq_vm_free(s, sizeof(SQString) + slen);

        return;
    }

    assert(0 && "string not found?");
}
