#include <stdio.h>
// comment with { braces } and "quotes"
int proto(int x);  // prototype passthrough

int empty_func() { return 42; }
void void_empty() { printf("void\n"); }

int switch_no_default(int x) {
    int r = 0;
    switch (x) {
        case 1: r = 10; break;
        case 2: r = 20; break;
    }
    return r;
}

int braces_in_string() {
    printf("{ not a brace } (also ; semicolon)\n");
    printf("quote \" still string %d\n", 7);
    /* block comment with { } ; */
    return 7;
}

int proto(int x) { return x * 3; }

int main() {
    printf("%d\n", empty_func());
    void_empty();
    printf("%d %d %d\n", switch_no_default(1), switch_no_default(2), switch_no_default(9));
    printf("%d\n", braces_in_string());
    printf("%d\n", proto(5));
    return 0;
}
