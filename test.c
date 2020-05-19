#include <stdio.h>
void main() {
    unsigned char a = 0x45;
    unsigned char b = a % 8;
    printf("%02x", a);
    printf("\n%d", b);
}