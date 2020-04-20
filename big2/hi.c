#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "bn.h"
static struct bn f[5000];
static int status[5000] = {0};

void init_f()
{
    for (int i = 0; i < 5000; i++) {
        bignum_from_int(&f[i], 0);
    }
}

void fibonacci_normal(long long k)
{
    bignum_from_int(&f[0], 0);
    bignum_from_int(&f[1], 1);

    for (int i = 2; i <= k; i++) {
        bignum_add(&f[i - 2], &f[i - 1], &f[i]);
    }
}

void fibonacci_fast_doubling_top_down(long long k)
{
    if (k == 0) {
        bignum_from_int(&f[0], 0);
        status[0] = 1;
    } else if (k == 1) {
        bignum_from_int(&f[1], 1);
        status[1] = 1;
    } else if (k == 2) {
        bignum_from_int(&f[2], 1);
        status[2] = 1;
    }

    if (status[k] != 0) {
        return;
    }

    status[k] = 1;

    if (status[k / 2 + 1] == 0) {
        fibonacci_fast_doubling_top_down(k / 2 + 1);
    }

    if (status[k / 2] == 0) {
        fibonacci_fast_doubling_top_down(k / 2);
    }

    struct bn tmp;
    struct bn tmp2;

    if (k & 1) {
        bignum_mul(&f[k / 2], &f[k / 2], &tmp);
        bignum_mul(&f[k / 2 + 1], &f[k / 2 + 1], &tmp2);
        bignum_add(&tmp, &tmp2, &f[k]);
    } else {
        bignum_mul(&f[k / 2], &f[k / 2 + 1], &tmp);
        bignum_lshift(&tmp, &tmp, 1);
        bignum_mul(&f[k / 2], &f[k / 2], &tmp2);
        bignum_sub(&tmp, &tmp2, &f[k]);
    }
}

void fibonacci_fast_doubling_bottom_up(long long k)
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

int main()
{
    char buf[1500];
    struct timespec start, end;
    FILE *fwp;

    // normal fibonacci
    fwp = fopen("./normal_fibonacci.txt", "w");
    printf("normal fibonacci : \n");
    for (int i = 0; i < 501; i++) {
        clock_gettime(CLOCK_REALTIME, &start);

        fibonacci_normal(i);

        clock_gettime(CLOCK_REALTIME, &end);

        printf("nsec : %lu, ", end.tv_nsec - start.tv_nsec);

        bignum_to_string(&f[i], buf, sizeof(buf));
        printf("f[%d] : 0x%s\n", i, buf);
        fprintf(fwp, "%d, %lu\n", i, end.tv_nsec - start.tv_nsec);
    }
    fclose(fwp);

    init_f();

    // fast_doubing top-down
    fwp = fopen("./fast_doubling_top_down.txt", "w");
    printf("fast doubling, top-down : \n");
    for (int i = 0; i < 501; i++) {
        for (int j = 0; j < 5000; j++) {
            status[j] = 0;
        }

        clock_gettime(CLOCK_REALTIME, &start);

        fibonacci_fast_doubling_top_down(i);

        clock_gettime(CLOCK_REALTIME, &end);

        printf("nsec : %lu, ", end.tv_nsec - start.tv_nsec);

        bignum_to_string(&f[i], buf, sizeof(buf));
        printf("f[%d] : 0x%s\n", i, buf);
        fprintf(fwp, "%d, %lu\n", i, end.tv_nsec - start.tv_nsec);
    }
    fclose(fwp);

    init_f();

    // fast_doubling bottom-up
    fwp = fopen("./fast_doubling_bottom_up.txt", "w");
    printf("fast doubling, bottom-up : \n");
    for (int i = 0; i < 501; i++) {
        for (int j = 0; j < 5000; j++) {
            status[j] = 0;
        }

        clock_gettime(CLOCK_REALTIME, &start);

        fibonacci_fast_doubling_bottom_up(i);

        clock_gettime(CLOCK_REALTIME, &end);

        printf("nsec : %lu, ", end.tv_nsec - start.tv_nsec);

        bignum_to_string(&f[i], buf, sizeof(buf));
        printf("f[%d] : 0x%s\n", i, buf);
        fprintf(fwp, "%d, %lu\n", i, end.tv_nsec - start.tv_nsec);
    }
    fclose(fwp);

    return 0;
}
