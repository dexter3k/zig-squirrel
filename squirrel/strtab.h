#pragma once

#include <cstring>
#include <new>
#include <cassert>

#include "squirrel.h"
#include "sqvector.hpp"
#include "SQString.hpp"

struct SQString;
struct SQSharedState;

extern "C" {

size_t hash_strings(uint8_t const * a, size_t a_len, uint8_t const * b, size_t b_len);

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
    SQString ** strings;
    size_t _numofslots;
    size_t _slotused;
public:
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

    SQString * Add(char const * news, size_t len) {
        SQHash newhash = ::_hashstr(news, len);
        SQHash h = newhash & (_numofslots - 1);
        SQString * s;
        for (s = strings[h]; s; s = s->_next) {
            if(s->_len == len && (0 == memcmp(news, s->_val, len))) {
                return s; //found
            }
        }

        SQString *t = (SQString *)sq_vm_malloc(sizeof(SQString) + len);
        new (t) SQString(this);

        memcpy(t->_val, news, len);
        t->_val[len] = '\0';
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

    SQString * Concat(const SQChar* a, size_t alen, const SQChar* b, size_t blen) {
        SQHash newhash = hash_strings(
            reinterpret_cast<uint8_t const *>(a), alen,
            reinterpret_cast<uint8_t const *>(b), blen
        );

        SQHash h = newhash & (_numofslots - 1);
        SQString* s;
        size_t len = alen + blen;
        for (s = strings[h]; s; s = s->_next) {
            if (s->_len != len) {
                continue;
            }
            if (0 != memcmp(a, s->_val, sq_rsl(alen))) {
                continue;
            }
            if (0 != memcmp(b, &s->_val[alen], sq_rsl(blen))) {
                continue;
            }
            return s;
        }

        SQString* t = (SQString*)sq_vm_malloc(sq_rsl(len) + sizeof(SQString));
        new (t) SQString(this);

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

            size_t slen = s->_len;
            s->~SQString(); // invalidate weakrefs
            sq_vm_free(s, sizeof(SQString) + slen);

            return;
        }

        assert(0 && "string not found?");
    }
private:
    void Resize(size_t size) {
        size_t oldsize = _numofslots;
        SQString ** oldtable = strings;

        AllocNodes(size);

        for (size_t i = 0; i < oldsize; i++){
            SQString * p = oldtable[i];
            while (p) {
                SQString * next = p->_next;
                SQHash h = p->_hash & (_numofslots - 1);
                p->_next = strings[h];
                strings[h] = p;
                p = next;
            }
        }

        sq_vm_free(oldtable, sizeof(SQString *) * oldsize);
    }

    void AllocNodes(size_t size) {
        _numofslots = size;
        strings = (SQString **)sq_vm_malloc(sizeof(SQString *) * _numofslots);
        memset(strings, 0, sizeof(SQString *) * _numofslots);
    }
};
