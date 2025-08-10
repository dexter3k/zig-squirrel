#pragma once

#include <cstring>
#include <new>
#include <cassert>

#include "squirrel.h"
#include "squtils.h"
#include "SQString.hpp"

struct SQString;
struct SQSharedState;

extern "C" {

size_t hash_strings(uint8_t const * a, size_t a_len, uint8_t const * b, size_t b_len);

// typedef struct StringTable StringTable;

// extern StringTable * strtab_init(void * user_pointer);
// extern void strtab_deinit(StringTable * table);
// extern void * 

}

inline SQHash _hashstr (const SQChar *s, size_t l)
{
    SQHash h = (SQHash)l;  /* seed */
    size_t step = (l >> 5) + 1;  /* if string is too long, don't hash all its chars */
    size_t l1;
    for (l1 = l; l1 >= step; l1 -= step)
        h = h ^ ((h << 5) + (h >> 2) + ((unsigned short)s[l1 - 1]));
    return h;
}

class SQStringTable {
public:
    SQString ** strings;
    SQUnsignedInteger _numofslots;
    SQUnsignedInteger _slotused;

    SQStringTable()
        : strings(nullptr)
        , _numofslots(0)
        , _slotused(0)
    {
        AllocNodes(128);
    }

    ~SQStringTable() {
        sq_vm_free(strings, sizeof(SQString *) * _numofslots);
    }

    SQString * Add(const SQChar * news,SQInteger len) {
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
        t->owner = this;
        memcpy(t->_val, news, len);
        t->_val[len] = '\0';
        t->_len = len;
        t->_hash = newhash;
        t->_next = strings[h];
        strings[h] = t;
        _slotused++;
        if (_slotused > _numofslots)  /* too crowded? */
            Resize(_numofslots*2);
        return t;
    }

    SQString * Concat(const SQChar* a, SQInteger alen, const SQChar* b, SQInteger blen) {
        // SQHash newhash = ::_hashstr2(a, alen, b, blen);

        SQHash newhash = hash_strings(
            reinterpret_cast<uint8_t const *>(a), alen,
            reinterpret_cast<uint8_t const *>(b), blen
        );

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

        t->owner = this;
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

    void Remove(SQString const * bs) {
        SQHash const h = bs->_hash & (_numofslots - 1);

        SQString * prev = nullptr;
        SQString * s = strings[h];
        while (s) {
            if (s != bs) {
                prev = s;
                s = s->_next;
                continue;
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
private:
    void Resize(SQInteger size) {
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

    void AllocNodes(SQInteger size) {
        _numofslots = size;
        strings = (SQString **)sq_vm_malloc(sizeof(SQString *) * _numofslots);
        memset(strings, 0, sizeof(SQString *) * _numofslots);
    }
};
