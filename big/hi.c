#include <stdio.h>
#include <stdlib.h>
#include "bn.h"
static struct bn f[50000];

int main()
{
    /*bignum_from_int(&(f[0]), 0);
    bignum_from_int(&(f[1]), 1);

    for(int i = 2;i < 50000;i++) {
      bignum_add(&(f[i-2]), &(f[i-1]), &(f[i]));
    }*/

    /*
    char buf[10000];

    struct bn num;
    // struct bn result;
    bignum_from_int(&num, 1);
    // bignum_add(&num, &num, &num);
    bignum_to_string(&num, buf, sizeof(buf));

    // bignum_to_string(&(f[4999]), buf, sizeof(buf));

    printf("result : 0x%s\n", buf);*/

    unsigned long long k = (unsigned long long) 1
                           << ((sizeof(long long) * 8) - 1);

    long long g = 8;

    while (!(k & g)) {
        k = k >> 1;
    }
    // printf("sizeof(long long) : %lu\n", sizeof(long long));
    printf("k = %llu\n", k);

    return 0;
}
