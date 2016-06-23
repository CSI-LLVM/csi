#include <cstdio>

static int global_value = 0;

static int a() {
    global_value += 1;
    return 1;
}

static int b() {
    global_value += 2;
    return 2;
}

int main() {
    int result = 0;
    printf("Entering main.\n");
    result += a() + b();
    result += global_value;
    printf("Result is %d\n", global_value);
    return 0;
}
