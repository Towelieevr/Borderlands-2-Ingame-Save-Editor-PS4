#pragma once
static __attribute__((noinline)) char* ob(char* b, const unsigned char* e, int n) {
    for (int i = 0; i < n; i++) b[i] = (char)(e[i] ^ (unsigned char)(0x5A + i * 0x1F));
    b[n] = 0;
    return b;
}
