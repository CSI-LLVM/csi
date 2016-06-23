#ifndef __CSI_BITSET_H__
#define __CSI_BITSET_H__

#include <assert.h>
#include <stdlib.h>
#include <stdint.h>

// Return the smallest multiple N of y such that x <= y * N
#ifdef CEILING
# error "CEILING already defined."
#endif
#define CEILING(x,y) (((x) + (y) - 1) / (y))

class Bitset {
private:
    typedef unsigned char elt_t;
    static const unsigned _eltbits = sizeof(elt_t) * 8;

    uint64_t _nbits, _nelts;
    elt_t *_data;

    inline void bit2elt(uint64_t bit, uint64_t &elt, unsigned &off) const {
        elt = bit / _eltbits;
        off = bit % _eltbits;
    }

public:
    Bitset() : _nbits(0), _nelts(0), _data(NULL) {}

    Bitset(uint64_t N) {
        allocate(N);
    }

    void allocate(uint64_t N) {
        _nbits = N;
        _nelts = CEILING(_nbits, _eltbits);
        _data = (elt_t *)calloc(_nelts, sizeof(elt_t));
        assert(_data);
    }

    void expand(uint64_t N) {
        _nbits += N;
        _nelts = CEILING(_nbits, _eltbits);
        _data = (elt_t *)realloc(_data, _nelts * sizeof(elt_t));
        assert(_data);
    }

    inline void set(uint64_t bit) {
        uint64_t elt;
        unsigned off;
        bit2elt(bit, elt, off);
        _data[elt] |= 1 << off;
    }

    inline bool get(uint64_t bit) const {
        uint64_t elt;
        unsigned off;
        bit2elt(bit, elt, off);
        return (_data[elt] & (1 << off)) != 0;
    }

    inline void clear(uint64_t bit) {
        uint64_t elt;
        unsigned off;
        bit2elt(bit, elt, off);
        _data[elt] &= ~(1 << off);
    }

    uint64_t count() const {
        uint64_t result = 0;
        for (uint64_t i = 0; i < _nelts; i++) {
            result += __builtin_popcount(_data[i]);
        }
        return result;
    }

    uint64_t size() const {
        return _nbits;
    }
};


#endif
