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

    unsigned _nbits, _nelts;
    elt_t *_data;

    inline void bit2elt(unsigned bit, unsigned &elt, unsigned &off) const {
        elt = bit / _eltbits;
        off = bit % _eltbits;
    }

public:
    Bitset() : _nbits(0), _nelts(0), _data(NULL) {}

    Bitset(unsigned N) {
        allocate(N);
    }

    void allocate(unsigned N) {
        _nbits = N;
        _nelts = CEILING(_nbits, _eltbits);
        _data = (elt_t *)calloc(_nelts, sizeof(elt_t));
        assert(_data);
    }

    inline void set(unsigned bit) {
        unsigned elt, off;
        bit2elt(bit, elt, off);
        _data[elt] |= 1 << off;
    }

    inline void clear(unsigned bit) {
        unsigned elt, off;
        bit2elt(bit, elt, off);
        _data[elt] &= ~(1 << off);
    }

    unsigned count() const {
        unsigned result = 0;
        for (unsigned i = 0; i < _nelts; i++) {
            result += __builtin_popcount(_data[i]);
        }
        return result;
    }

    unsigned size() const {
        return _nbits;
    }
};


#endif
