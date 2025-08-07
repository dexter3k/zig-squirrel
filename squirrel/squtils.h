#pragma once

#include <cstdint>

#include "sqmem.h"

#define sq_aligning(v) (((size_t)(v) + (SQ_ALIGNMENT-1)) & (~(SQ_ALIGNMENT-1)))

template<typename T>
class sqvector {
    size_t len;
    size_t cap;
public:
    T* _vals;

    sqvector()
        : len(0)
        , cap(0)
        , _vals(nullptr)
    {}

    sqvector(sqvector<T> const & v) {
        copy(v);
    }

    void copy(sqvector<T> const & v) {
        if(len) {
            resize(0); //destroys all previous stuff
        }

        if(v.len > cap) {
            _realloc(v.len);
        }

        for(size_t i = 0; i < v.len; i++) {
            new ((void *)&_vals[i]) T(v._vals[i]);
        }

        len = v.len;
    }

    ~sqvector() {
        if(cap) {
            for(size_t i = 0; i < len; i++) {
                _vals[i].~T();
            }

            sq_vm_free(_vals, cap * sizeof(T));
        }
    }

    void reserve(size_t newsize) {
        _realloc(newsize);
    }

    void resize(size_t newsize, const T& fill = T()) {
        if(newsize > cap) {
            _realloc(newsize);
        }

        if(newsize > len) {
            while(len < newsize) {
                new ((void *)&_vals[len]) T(fill);
                len++;
            }
        } else {
            for (size_t i = newsize; i < len; i++) {
                _vals[i].~T();
            }
            len = newsize;
        }
    }

    void shrinktofit() {
        if(len > 4) {
            _realloc(len);
        }
    }

    inline T& top() const {
        return _vals[len - 1];
    }

    inline size_t size() const {
        return len;
    }

    bool empty() const {
        return len <= 0;
    }

    inline T & push_back(const T& val = T()) {
        if(cap <= len) {
            _realloc(len * 2);
        }
        return *(new ((void *)&_vals[len++]) T(val));
    }

    inline void pop_back() {
        len--;
        _vals[len].~T();
    }

    void insert(size_t idx, const T& val) {
        resize(len + 1);

        for(size_t i = len - 1; i > idx; i--) {
            _vals[i] = _vals[i - 1];
        }

        _vals[idx] = val;
    }

    void remove(size_t idx) {
        _vals[idx].~T();

        if(idx < (len - 1)) {
            memmove((void*)&_vals[idx], &_vals[idx+1], sizeof(T) * (len - idx - 1));
        }

        len--;
    }

    size_t capacity() {
        return cap;
    }

    inline T & back() const {
        return _vals[len - 1];
    }

    inline T & operator[](size_t pos) const {
        return _vals[pos];
    }
private:
    void _realloc(size_t newsize) {
        newsize = (newsize > 0)
            ? newsize
            : 4;
        _vals = (T*)sq_vm_realloc(_vals, cap * sizeof(T), newsize * sizeof(T));
        cap = newsize;
    }
};
