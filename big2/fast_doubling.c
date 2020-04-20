#include <stdio.h>
#include <stdlib.h>
#include "bn.h"

static struct bn f[50000];
static int state[50000] = {0};

void fast_doubling2(long long k)
{
    if (k == 0) {
        bignum_from_int(&f[0], 0);
        return;
    }

    unsigned long long leftbit = (unsigned long long) 1
                                 << ((sizeof(leftbit) * 8) - 1);

    while (!(leftbit & k)) {
        leftbit = leftbit >> 1;
    }

    struct bn a;
    struct bn b;

    struct bn tmp1;
    struct bn tmp2;
    struct bn t1;
    struct bn t2;

    bignum_from_int(&a, 0);
    bignum_from_int(&b, 1);

    while (leftbit > 0) {
        bignum_mul(&a, &b, &t1);
        bignum_lshift(&t1, &t1, 1);
        bignum_mul(&a, &a, &t2);
        bignum_sub(&t1, &t2, &tmp1);

        bignum_mul(&a, &a, &t1);
        bignum_mul(&b, &b, &t2);
        bignum_add(&t1, &t2, &tmp2);

        bignum_assign(&a, &tmp1);
        bignum_assign(&b, &tmp2);

        if (leftbit & k) {
            bignum_add(&a, &b, &tmp1);
            bignum_assign(&a, &b);
            bignum_assign(&b, &tmp1);
        }

        leftbit = leftbit >> 1;
    }

    bignum_assign(&f[k], &a);
}


//可用一個東西去記哪些數字已經走過了

void fast_doubling(long long k)
{
    if (k == 0 || k == 1 || k == 2) {
        return;
    }

    if (state[k] != 0) {
        printf("k L %lld, have done!\n", k);
        return;
    }

    state[k] = 1;
    printf("K : %lld\n", k);

    fast_doubling(k / 2 + 1);
    fast_doubling(k / 2);


    struct bn tmp;
    struct bn tmp2;

    if (k & 1) {
        printf("k : %lld, is odd\n", k);

        bignum_mul(&f[k / 2], &f[k / 2], &tmp);
        bignum_mul(&f[k / 2 + 1], &f[k / 2 + 1], &tmp2);
        bignum_add(&tmp, &tmp2, &f[k]);
    } else {
        printf("k : %lld, is even\n", k);

        bignum_mul(&f[k / 2], &f[k / 2 + 1], &tmp);
        bignum_lshift(&tmp, &tmp, 1);
        bignum_mul(&f[k / 2], &f[k / 2], &tmp2);
        bignum_sub(&tmp, &tmp2, &f[k]);
    }
}

int main()
{
    char buf[8000];

    bignum_from_int(&f[0], 0);
    bignum_from_int(&f[1], 1);
    bignum_from_int(&f[2], 1);


    // 500 需要 11 個 uint32_t
    // 200      5
    // 300      7

    for (int i = 0; i < 10; i++) {
        fast_doubling2(i);

        bignum_to_string(&f[i], buf, sizeof(buf));
        printf("0x%s\n", buf);
    }

    return 0;
}
