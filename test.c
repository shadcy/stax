#include <stdio.h>
int main() {
    int len = 3;
    int alloc_len = len + (len & 1 ? 1 : 0);
    printf("odd len=%d alloc_len=%d len_field=%d\n", len, alloc_len, alloc_len+6);
    len = 4;
    alloc_len = len + (len & 1 ? 1 : 0);
    printf("even len=%d alloc_len=%d len_field=%d\n", len, alloc_len, alloc_len+6);
    return 0;
}
