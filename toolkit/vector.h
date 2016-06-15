#ifndef __CSI_VECTOR_H__
#define __CSI_VECTOR_H__

#include <assert.h>
#include <stdlib.h>
#include <stdint.h>

template<class T>
class Vector {
public:
    Vector() : _size(0), capacity(0), data(nullptr) {}

    void push_back(T t) {
        ensure_space(_size + 1);
        data[_size] = t;
        _size++;
    }

    T pop_back() {
        assert(_size > 0);
        T t = data[_size - 1];
        _size--;
        return t;
    }

    T& at(int64_t index) {
        assert(_size > index);
        return data[index];
    }

    bool empty() const {
        return _size == 0;
    }

    int64_t size() const {
        return _size;
    }

    void expand(int64_t newsize, T value) {
        int64_t oldend = _size;
        ensure_space(newsize);
        for (int64_t i = oldend; i < newsize; i++) {
            data[i] = value;
        }
        _size = newsize;
    }
private:
    static const int64_t initial_capacity = 128;
    int64_t _size, capacity;
    T *data;

    void ensure_space(int64_t newsize) {
        bool need_realloc = capacity == 0 || newsize >= capacity;
        if (capacity == 0) capacity = initial_capacity;
        while (newsize >= capacity) {
            capacity *= 2;
        }
        if (need_realloc) {
            data = (T *)realloc(data, capacity);
            assert(data != NULL);
        }
    }
};

#endif
