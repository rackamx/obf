#include <stdio.h>

void greet(int n) {
    if (n <= 0) return;
    printf("hello %d\n", n);
    return;
}

int elsechain(int x) {
    if (x < 0) return -1;
    else if (x == 0) return 0;
    else if (x < 10) return 1;
    else return 2;
}

int logic(int a, int b) {
    if (a > 0 && b > 0) return 1;
    if (a < 0 || b < 0) return -1;
    return 0;
}

int while_true() {
    int i = 0;
    while (1) {
        i++;
        if (i >= 5) break;
    }
    return i;
}

int goto_loop() {
    int i = 0;
start:
    i++;
    if (i < 4) goto start;
    return i;
}

int for_decl_multi() {
    int s = 0;
    for (int i = 0, j = 5; i < j; i++, j--) s += 2;
    return s;
}

int tern(int x) {
    int y = x > 5 ? 100 : 200;
    return y;
}

int main() {
    greet(0);
    greet(2);
    printf("else -5=%d\n", elsechain(-5));
    printf("else 0=%d\n", elsechain(0));
    printf("else 5=%d\n", elsechain(5));
    printf("else 20=%d\n", elsechain(20));
    printf("logic=%d %d %d\n", logic(1,2), logic(-1,2), logic(0,0));
    printf("while_true=%d\n", while_true());
    printf("goto_loop=%d\n", goto_loop());
    printf("for_multi=%d\n", for_decl_multi());
    printf("tern=%d %d\n", tern(3), tern(7));
    return 0;
}
