#include <stdio.h>
#include <stdlib.h>

typedef int myint;

struct Point { int x; int y; };

int fib(int n) {
    if (n <= 1) return n;
    return fib(n-1) + fib(n-2);
}

int ptr_test() {
    int v = 42;
    int *p = &v;
    *p = *p + 8;
    return v; // 50
}

struct Point make_point(int a, int b) {
    struct Point pt;
    pt.x = a;
    pt.y = b;
    return pt;
}

int struct_test() {
    struct Point p = {3, 4};
    return p.x * p.x + p.y * p.y; // 25
}

int switch_loop() {
    int s = 0;
    for (int i = 0; i < 5; i++) {
        switch (i) {
            case 0:
            case 1:
                s += 1;
                break;
            case 2:
                continue;
            default:
                s += 10;
                break;
        }
        s += 100;
    }
    return s;
    // i=0: s=1+100=101; i=1: 102... wait compute:
    // i0: case0: s=1, break -> s+=100 =>101
    // i1: s=102 -> +100 =>202? Let's just compare orig vs flat.
}

myint typedef_test(myint x) {
    myint y = x * 2;
    return y;
}

int comma_for() {
    int s = 0;
    int i, j;
    for (i = 0, j = 10; i < j; i++, j--) {
        s++;
    }
    return s; // 5
}

int early() {
    for (int i = 0; i < 100; i++) {
        if (i == 3) return i;
    }
    return -1;
}

int empty_loop(int n) {
    int i = 0;
    for (; i < n; ) { i++; }
    return i;
}

int global_var = 7;
int use_global() {
    global_var += 3;
    return global_var;
}

int main() {
    printf("fib10=%d (55)\n", fib(10));
    printf("ptr=%d (50)\n", ptr_test());
    struct Point q = make_point(5, 6);
    printf("pt=%d,%d\n", q.x, q.y);
    printf("struct=%d (25)\n", struct_test());
    printf("switch_loop=%d\n", switch_loop());
    printf("typedef=%d (14)\n", typedef_test(7));
    printf("comma=%d (5)\n", comma_for());
    printf("early=%d (3)\n", early());
    printf("empty=%d (4)\n", empty_loop(4));
    printf("global=%d (10)\n", use_global());
    printf("global=%d (13)\n", use_global());
    return 0;
}
