#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/uaccess.h>  // Required for the copy to user function


MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
//#define MAX_LENGTH 92
#define MAX_LENGTH 500

//#define BN_ARRAY_SIZE 32
#define BN_ARRAY_SIZE 11
#define MAX_ELEMENT ((uint64_t) 0xFFFFFFFF)
#define WORD_SIZE 4

static uint64_t foo;

static ssize_t foo_show(struct kobject *kobj,
                        struct kobj_attribute *attr,
                        char *buf)
{
    return snprintf(buf, 1000, "%lu\n", foo);
}

static ssize_t foo_store(struct kobject *kobj,
                         struct kobj_attribute *attr,
                         const char *buf,
                         size_t count)
{
    int ret;

    // ret = kstrtoint(buf, 10, &foo);
    ret = kstrtoull(buf, 10, &foo);
    if (ret < 0)
        return ret;

    return count;
}


/* Sysfs attributes cannot be world-writable. */
static struct kobj_attribute foo_attribute =
    __ATTR(foo, 0664, foo_show, foo_store);


/*
 * Create a group of attributes so that we can create and destroy them all
 * at once.
 */
static struct attribute *attrs[] = {
    &foo_attribute.attr,
    NULL,
};

/*
 * An unnamed attribute group will put all of the attributes directly in
 * the kobject directory.  If we specify a name, a subdirectory will be
 * created for the attributes with the directory being the name of the
 * attribute group.
 */
static struct attribute_group attr_group = {
    .attrs = attrs,
};

static struct kobject *example_kobj;


struct bignum {
    uint32_t array[BN_ARRAY_SIZE];
};

static void bignum_init(struct bignum *n)
{
    int i;
    for (i = 0; i < BN_ARRAY_SIZE; i++) {
        n->array[i] = 0;
    }
}

static void bignum_from_uint64(struct bignum *n, uint64_t i)
{
    bignum_init(n);

    n->array[0] = i;
    uint64_t num_32 = 32;
    uint64_t tmp =
        i >> num_32; /* bit-shift with U64 operands to force 64-bit results */
    n->array[1] = tmp;
}

void bignum_assign(struct bignum *dst, struct bignum *src)
{
    int i;
    for (i = 0; i < BN_ARRAY_SIZE; ++i) {
        dst->array[i] = src->array[i];
    }
}

static void _lshift_word(struct bignum *a, int nwords)
{
    int i;
    /* Shift whole words */
    for (i = (BN_ARRAY_SIZE - 1); i >= nwords; --i) {
        a->array[i] = a->array[i - nwords];
    }
    /* Zero pad shifted words. */
    for (; i >= 0; --i) {
        a->array[i] = 0;
    }
}

// c = a + b
static void bignum_add(struct bignum *a, struct bignum *b, struct bignum *c)
{
    int carry = 0;
    int i;
    for (i = 0; i < BN_ARRAY_SIZE; i++) {
        uint64_t tmp;
        tmp = (uint64_t) a->array[i] + b->array[i] + carry;
        carry = (tmp > MAX_ELEMENT);
        c->array[i] = (tmp & MAX_ELEMENT);
    }
}

// c = a - b
static void bignum_sub(struct bignum *a, struct bignum *b, struct bignum *c)
{
    int borrow = 0;
    int i;

    for (i = 0; i < BN_ARRAY_SIZE; i++) {
        uint64_t res;
        uint64_t tmp1;
        uint64_t tmp2;

        tmp1 = (uint64_t) a->array[i] + (MAX_ELEMENT + 1); /* + number_base */
        tmp2 = (uint64_t) b->array[i] + borrow;
        ;
        res = (tmp1 - tmp2);

        c->array[i] = (uint32_t)(
            res & MAX_ELEMENT); /* "modulo number_base" == "% (number_base - 1)"
                                   if number_base is 2^N */

        // 假如減去的結果 < MAX_ELEMENT ， 表示在這次的計算，需要借用
        borrow = (res <= MAX_ELEMENT);
    }
}

// c = a * b
static void bignum_mul(struct bignum *a, struct bignum *b, struct bignum *c)
{
    struct bignum row;
    struct bignum tmp;

    int i, j;

    bignum_init(c);

    uint32_t upper;
    uint32_t lower;

    for (i = 0; i < BN_ARRAY_SIZE; i++) {
        bignum_init(&row);
        for (j = 0; j < BN_ARRAY_SIZE; j++) {
            if (i + j < BN_ARRAY_SIZE) {
                uint64_t intermediate =
                    ((uint64_t) a->array[i] * (uint64_t) b->array[j]);

		lower = intermediate & MAX_ELEMENT;
		upper = intermediate >> (8 * WORD_SIZE);

                intermediate = (uint64_t) lower + (uint64_t) c->array[i+j];

                c->array[i+j] = intermediate & MAX_ELEMENT;
                intermediate  = intermediate >> (8 * WORD_SIZE);

                if (i + j + 1 < BN_ARRAY_SIZE) {
                    intermediate = (uint64_t) upper + (uint64_t) c->array[i+j+1] + intermediate;
                    c->array[i+j+1] = intermediate & MAX_ELEMENT;
                    intermediate  = intermediate >> (8 * WORD_SIZE);

                    int k = i + j + 2;
                    while (intermediate > 0 && k < BN_ARRAY_SIZE) {
                        intermediate = (uint64_t) c->array[k] + intermediate;

                        c->array[k] = intermediate & MAX_ELEMENT;
                        intermediate  = intermediate >> (8 * WORD_SIZE);
                    }
                }
            }
        }
        bignum_add(c, &row, c);
    }
}

void bignum_lshift(struct bignum *a, struct bignum *b, int nbits)
{
    bignum_assign(b, a);
    /* Handle shift in multiples of word-size */
    const int nbits_pr_word = (4 * 8);
    int nwords = nbits / nbits_pr_word;
    if (nwords != 0) {
        _lshift_word(b, nwords);
        nbits -= (nwords * nbits_pr_word);
    }

    if (nbits != 0) {
        int i;
        for (i = (BN_ARRAY_SIZE - 1); i > 0; --i) {
            b->array[i] =
                (b->array[i] << nbits) | (b->array[i - 1] >> ((8 * 4) - nbits));
        }
        b->array[i] <<= nbits;
    }
}


static void bignum_to_string(struct bignum *n, char *str, int bytes)
{
    int j = BN_ARRAY_SIZE - 1;
    int i = 0;

    while ((j >= 0) && (bytes > (i + 1))) {
        snprintf(&str[i], 9, "%.08x", n->array[j]);
        i += (2 * 4);
        j -= 1;
    }

    j = 0;
    while (str[j] == '0') {
        j += 1;
    }

    for (i = 0; i < (bytes - j); ++i) {
        str[i] = str[i + j];
    }

    str[i] = 0;

    if (strlen(str) == 0) {
        str[0] = '0';
        str[1] = 0;
    }
}


static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);

static char message[8000] = {0};
static char BeAppended[100] = {0};

static struct bignum bigfibo[4000];
static int state[4000] = {0};

/*static void big_fib_sequence_v1(long long k)
{
    bignum_from_uint64(&(bigfibo[0]), 0);
    bignum_from_uint64(&(bigfibo[1]), 1);

    for (int i = 2; i <= k; i++) {
        bignum_add(&(bigfibo[i - 2]), &(bigfibo[i - 1]), &(bigfibo[i]));
    }
}*/

// fast doubling
static void big_fib_sequence_v3(long long k)
{
    if (k == 0) {
        bignum_from_uint64(&bigfibo[0], 0);
        return;
    }

    unsigned long long leftbit = (unsigned long long) 1
                                 << ((sizeof(leftbit) * 8) - 1);

    while (!(leftbit & k)) {
        leftbit = leftbit >> 1;
    }

    struct bignum a;
    struct bignum b;

    struct bignum tmp1;
    struct bignum tmp2;
    struct bignum t1;
    struct bignum t2;

    bignum_from_uint64(&a, 0);
    bignum_from_uint64(&b, 1);

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

    bignum_assign(&bigfibo[k], &a);
}

// fast doubling
static void big_fib_sequence_v2(long long k)
{
    if (k == 0) {
        bignum_from_uint64(&(bigfibo[0]), 0);
        state[k] = 1;
    } else if (k == 1) {
        bignum_from_uint64(&(bigfibo[1]), 1);
        state[k] = 1;
    } else if (k == 2) {
        bignum_from_uint64(&(bigfibo[2]), 1);
        state[k] = 1;
    }

    if (state[k] != 0) {
        return;
    }

    state[k] = 1;

    if (state[k / 2 + 1] == 0) {
        big_fib_sequence_v2(k / 2 + 1);
    }

    if (state[k / 2] == 0) {
        big_fib_sequence_v2(k / 2);
    }

    struct bignum tmp;
    struct bignum tmp2;

    if (k & 1) {
        bignum_mul(&bigfibo[k / 2], &bigfibo[k / 2], &tmp);
        bignum_mul(&bigfibo[k / 2 + 1], &bigfibo[k / 2 + 1], &tmp2);
        bignum_add(&tmp, &tmp2, &bigfibo[k]);
    } else {
        bignum_mul(&bigfibo[k / 2], &bigfibo[k / 2 + 1], &tmp);
        bignum_lshift(&tmp, &tmp, 1);
        bignum_mul(&bigfibo[k / 2], &bigfibo[k / 2], &tmp2);
        bignum_sub(&tmp, &tmp2, &bigfibo[k]);
    }
}

// 計算 fibonacci 的本體
/*static long long fib_sequence(long long k)
{
    FIXME: use clz/ctz and fast algorithms to speed up
    long long f[k + 2];

    f[0] = 0;
    f[1] = 1;

    for (int i = 2; i <= k; i++) {
        f[i] = f[i - 1] + f[i - 2];
    }

    return f[k];
}*/

static int fib_open(struct inode *inode, struct file *file)
{
    if (!mutex_trylock(&fib_mutex)) {
        printk(KERN_ALERT "fibdrv is in use");
        return -EBUSY;
    }
    return 0;
}

static int fib_release(struct inode *inode, struct file *file)
{
    mutex_unlock(&fib_mutex);
    return 0;
}

/* calculate the fibonacci number at given offset */
// static ssize_t fib_read(struct file *file,
//這個 return type 不能變
static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    ktime_t start;
    ktime_t end;

    // set experiment environment
    for (int i = 0; i < 4000; i++) {
        state[i] = 0;
    }

    start = ktime_get();
    big_fib_sequence_v3(*offset);
    end = ktime_get();

    foo = ktime_to_ns(end) - ktime_to_ns(start);

    bignum_to_string(&(bigfibo[*offset]), message, sizeof(message));

    // 在這裡把算完的 fibonacci 用 copy_to_user
    // 回傳到 user space 去
    copy_to_user(buf, message, strlen(message));

    // 這個 offset 就是第幾個 fibonacci 數。
    // return (ssize_t) fib_sequence(*offset);
    return (ssize_t) strlen(message);
}

/* write operation is skipped */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    return 1;
}

static loff_t fib_device_lseek(struct file *file, loff_t offset, int orig)
{
    loff_t new_pos = 0;
    switch (orig) {
    case 0: /* SEEK_SET: */
        new_pos = offset;
        break;
    case 1: /* SEEK_CUR: */
        new_pos = file->f_pos + offset;
        break;
    case 2: /* SEEK_END: */
        new_pos = MAX_LENGTH - offset;
        break;
    }

    if (new_pos > MAX_LENGTH)
        new_pos = MAX_LENGTH;  // max case
    if (new_pos < 0)
        new_pos = 0;        // min case
    file->f_pos = new_pos;  // This is what we'll use now
    return new_pos;
}

// Q: THIS_MODULE 是什麼？
const struct file_operations fib_fops = {
    .owner = THIS_MODULE,
    .read = fib_read,
    .write = fib_write,
    .open = fib_open,
    .release = fib_release,
    .llseek = fib_device_lseek,
};


static int __init init_fib_dev(void)
{
    // kobject
    int retval;

    /*
     * Create a simple kobject with the name of "kobject_example",
     * located under /sys/kernel/
     *
     * As this is a simple directory, no uevent will be sent to
     * userspace.  That is why this function should not be used for
     * any type of dynamic kobjects, where the name and number are
     * not known ahead of time.
     */
    example_kobj = kobject_create_and_add("kobject_example", kernel_kobj);
    if (!example_kobj)
        return -ENOMEM;

    /* Create the files associated with this kobject */
    retval = sysfs_create_group(example_kobj, &attr_group);
    if (retval)
        kobject_put(example_kobj);


    // ----
    // big fibonacci
    int rc = 0;

    // bignum_to_string(&(bigfibo[99]), message, sizeof(message));
    // printk("bignum test : %s\n", message);

    mutex_init(&fib_mutex);

    // Let's register the device
    // This will dynamically allocate the major number
    rc = alloc_chrdev_region(&fib_dev, 0, 1, DEV_FIBONACCI_NAME);

    if (rc < 0) {
        printk(KERN_ALERT
               "Failed to register the fibonacci char device. rc = %i",
               rc);
        return rc;
    }

    fib_cdev = cdev_alloc();
    if (fib_cdev == NULL) {
        printk(KERN_ALERT "Failed to alloc cdev");
        rc = -1;
        goto failed_cdev;
    }
    cdev_init(fib_cdev, &fib_fops);
    rc = cdev_add(fib_cdev, fib_dev, 1);

    if (rc < 0) {
        printk(KERN_ALERT "Failed to add cdev");
        rc = -2;
        goto failed_cdev;
    }

    fib_class = class_create(THIS_MODULE, DEV_FIBONACCI_NAME);

    if (!fib_class) {
        printk(KERN_ALERT "Failed to create device class");
        rc = -3;
        goto failed_class_create;
    }

    if (!device_create(fib_class, NULL, fib_dev, NULL, DEV_FIBONACCI_NAME)) {
        printk(KERN_ALERT "Failed to create device");
        rc = -4;
        goto failed_device_create;
    }
    return rc;
failed_device_create:
    class_destroy(fib_class);
failed_class_create:
    cdev_del(fib_cdev);
failed_cdev:
    unregister_chrdev_region(fib_dev, 1);
    return rc;
}

static void __exit exit_fib_dev(void)
{
    // kobjdect
    kobject_put(example_kobj);

    mutex_destroy(&fib_mutex);
    device_destroy(fib_class, fib_dev);
    class_destroy(fib_class);
    cdev_del(fib_cdev);
    unregister_chrdev_region(fib_dev, 1);
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);
