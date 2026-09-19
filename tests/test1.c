#include <stdio.h>

int fact(int n) {
    int r = 1;
    for (int i = 2; i <= n; i++) {
        r = r * i;
    }
    return r;
}

int main() {
    for (int i = 0; i <= 6; i++) {
        printf("%d! = %d\n", i, fact(i));
    }
    int x = 10;
    if (x > 5) {
        printf("big\n");
    } else {
        printf("small\n");
    }
    int s = 0;
    int j = 0;
    while (j < 5) {
        if (j == 3) { j++; continue; }
        s += j;
        j++;
    }
    printf("s=%d\n", s);
    return 0;
}
