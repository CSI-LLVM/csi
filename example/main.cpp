#include <cstdio>
#include <cstdlib>
#include <sys/time.h>

static int global_value = 0;

static int a() {
    global_value += 1;
    return 1;
}

static int b() {
    global_value += 2;
    return 2;
}

static int c() {
    global_value += 3;
    return 3;
}

int main() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    srandom(tv.tv_sec + tv.tv_usec/1000000);

    int result = 0;
    printf("Entering main.\n");
    result += a() + b();
    result += global_value;
    if (random() < RAND_MAX/2) {
        result += c();
        result += global_value;
    }
    printf("Result is %d\n", result);
    return 0;
}
