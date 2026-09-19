#include <stdio.h>

int classify(int x) {
    switch (x) {
        case 1:
            return 10;
        case 2:
        case 3:
            return 20;
        default:
            return 30;
    }
}

int loop_break() {
    int s = 0;
    for (int i = 0; i < 10; i++) {
        if (i == 5) break;
        s += i;
    }
    return s;
}

int do_while_test(int n) {
    int i = 0;
    int s = 0;
    do {
        s += i;
        i++;
    } while (i < n);
    return s;
}

int goto_test(int x) {
    int y = 0;
    if (x == 0) goto end;
    y = x * 2;
    goto mid;
end:
    y += 1;
    return y;
mid:
    y += 100;
    return y;
}

int nested(int n) {
    int s = 0;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if ((i + j) % 2 == 0) continue;
            s += i * j;
        }
    }
    return s;
}

int shadow() {
    int x = 1;
    int r = 0;
    {
        int x = 2;
        r += x;
        {
            int x = 3;
            r += x;
        }
        r += x;
    }
    r += x;
    return r; // expect 2+3+2+1 = 8
}

int array_test() {
    int a[3] = {1, 2, 3};
    int s = 0;
    for (int i = 0; i < 3; i++) s += a[i];
    char msg[] = "hi";
    s += msg[0] + msg[1]; // 'h'+'i' = 209
    return s; // 6 + 209 = 215
}

int main() {
    printf("classify 1=%d (10)\n", classify(1));
    printf("classify 2=%d (20)\n", classify(2));
    printf("classify 3=%d (20)\n", classify(3));
    printf("classify 9=%d (30)\n", classify(9));
    printf("loop_break=%d (10)\n", loop_break());
    printf("do_while=%d (6)\n", do_while_test(4));
    printf("goto0=%d (1)\n", goto_test(0));
    printf("goto5=%d (110)\n", goto_test(5));
    printf("nested=%d\n", nested(4));
    printf("shadow=%d (8)\n", shadow());
    printf("array=%d (215)\n", array_test());
    // while with empty body / comma?
    int k = 0;
    while (k < 3) k++;
    printf("k=%d (3)\n", k);
    return 0;
}
