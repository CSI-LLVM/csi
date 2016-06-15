#ifndef __CSI_STACK_H__
#define __CSI_STACK_H__

#include <assert.h>
#include <stdlib.h>
#include <stdint.h>

template<class T>
class Stack {
public:
    Stack() : size(0), capacity(0), data(nullptr) {}

    void push(T t) {
        ensure_space(size + 1);
        data[size] = t;
        size++;
    }

    T pop() {
        assert(size > 0);
        T t = data[size - 1];
        size--;
        return t;
    }

    const T &top() const {
        assert(size > 0);
        return data[size - 1];
    }

    bool empty() const {
        return size == 0;
    }
private:
    static const int64_t initial_capacity = 128;
    int64_t size, capacity;
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
