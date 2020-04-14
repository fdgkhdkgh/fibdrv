#include <stdio.h>
#include <stdlib.h>

// 這裡的實做也有點奇妙，在數字很大的情況，會有一半是 O(N)
// 應該說，這樣的實做方式就是 O(N)了
// 等等，但感覺又好像是 O(logN)	， 先不嘴了
// 總之，這一定還有可改善的空間。


long long fibo(long long k)
{
    if (k == 0)
        return 0;
    long long fn = 1;
    long long fn1 = 1;
    long long f2n = 1;
    long long f2n1 = 0;
    long long i = 1;
    while (i < k) {
        printf("i：%lld\n", i);
        if ((i << 1) <= k) {
            f2n1 = fn1 * fn1 + fn * fn;
            f2n = fn * (2 * fn1 - fn);
            fn = f2n;
            fn1 = f2n1;
            i = i << 1;
        } else {
            fn = f2n;
            f2n = f2n1;
            f2n1 = fn + f2n1;
            i++;
        }
    }
    return f2n;
}

int main()
{
    printf("%lld\n", fibo(90));

    return 0;
}
