#include <stdio.h>
int counter() {
    static int n = 0;
    n++;
    return n;
}
int const_test() {
    const int x = 5;
    int y = x + 2;
    return y;
}
int main() {
    printf("%d\n", counter());
    printf("%d\n", counter());
    printf("%d\n", counter());
    printf("const=%d (7)\n", const_test());
    return 0;
}
