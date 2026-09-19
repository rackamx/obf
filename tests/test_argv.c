#include <stdio.h>
#include <string.h>
int main(int argc, char **argv) {
    int total = 0;
    for (int i = 1; i < argc; i++) {
        printf("arg%d=%s\n", i, argv[i]);
        total += strlen(argv[i]);
    }
    printf("total=%d\n", total);
    return 0;
}
