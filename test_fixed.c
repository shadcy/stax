#include <stdio.h>
int main() {
    int a = -848520;
    int b = 9429989;
    long long c = ((long long)a * (long long)b);
    int res = c >> 16;
    printf("res: %d\n", res);
    return 0;
}
