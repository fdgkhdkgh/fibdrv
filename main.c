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

#include "bn.h"

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 10000

// fast doubling
static void big_fib_sequence_v3(bn_t fib, long long k)
{
    // unlikely is for branch predictor
    if (unlikely(k <= 2)) {
        if (k == 0) {
            bn_init_u32(fib, 0);
	} else {
            bn_init_u32(fib, 1);
	}	
        return;
    }

    unsigned long long leftbit = (unsigned long long) 1
                                 << ((sizeof(leftbit) * 8) - 1);

    while (!(leftbit & k)) {
        leftbit = leftbit >> 1;
    }

    bn* a = fib;
    bn_t b;

    bn_t tmp1;
    bn_t tmp2;

    bn_init_u32(a, 0);
    bn_init_u32(b, 1);
    bn_init(tmp1);
    bn_init(tmp2);

    while (leftbit > 0) {
        //bn_mul(a, b, t1);
        //bn_lshift(t1, 1, t1);
	//bn_mul(a, a, t2);
	//t2->sign = 1; 
	//bn_add(t1, t2, tmp1);
	bn_lshift(b, 1, tmp1);
        a->sign = 1;
	bn_add(tmp1, a, tmp1);
	a->sign = 0;
	bn_mul(tmp1, a, tmp1);

	//bn_mul(a, a, a);
	//bn_mul(b, b, b);
	bn_sqr(a, a);
	bn_sqr(b, b);
	bn_add(a, b, tmp2);

        bn_swap(a, tmp1);
	bn_swap(b, tmp2);

	if (leftbit & k) {
            bn_swap(a, b);
	    bn_add(a, b, b);
            //bn_add(a, b, tmp1);
	    //bn_swap(a, b);
	    //bn_swap(b, tmp1);
	}

        leftbit = leftbit >> 1;
    }

    bn_free(tmp1);
    bn_free(tmp2);
    bn_free(b);
}

// fast doubling
static void big_fib_sequence_v2(bn_t fib, long long k)
{
    // unlikely is for branch predictor
    if (unlikely(k <= 2)) {
        if (k == 0) {
            bn_init_u32(fib, 0);
	} else {
            bn_init_u32(fib, 1);
	}	
        return;
    }

    unsigned long long leftbit = (unsigned long long) 1
                                 << ((sizeof(leftbit) * 8) - 1);

    while (!(leftbit & k)) {
        leftbit = leftbit >> 1;
    }

    bn* a = fib;
    bn_t b;

    bn_t t1;
    bn_t t2;
    bn_t tmp1;
    bn_t tmp2;

    bn_init_u32(a, 0);
    bn_init_u32(b, 1);
    bn_init(t1);
    bn_init(t2);
    bn_init(tmp1);
    bn_init(tmp2);

    while (leftbit > 0) {
        bn_mul(a, b, t1);
        bn_lshift(t1, 1, t1);
	bn_mul(a, a, t2);
	t2->sign = 1; 
	bn_add(t1, t2, tmp1);

	bn_mul(a, a, a);
	bn_mul(b, b, b);
	bn_add(a, b, tmp2);

        bn_swap(a, tmp1);
	bn_swap(b, tmp2);

	if (leftbit & k) {
            bn_add(a, b, tmp1);
	    bn_swap(a, b);
	    bn_swap(b, tmp1);
	}

        leftbit = leftbit >> 1;
    }

    bn_free(t1);
    bn_free(t2);
    bn_free(tmp1);
    bn_free(tmp2);
    bn_free(b);
}

// fast doubling
static void naive_fibo(bn_t fib, long long k)
{
    bn* a = fib;
    bn_t b;

    bn_init_u32(a, 0);
    bn_init_u32(b, 1);

    long long i;
    for(i = 0;i < k;i++) {
        bn_add(a, b, a);
	bn_swap(a, b);
    } 
}

static void teacher_fibonacci(bn *fib, uint64_t n)
{
    if (unlikely(n <= 2)) {
        if (n == 0)
            bn_zero(fib);
        else
            bn_set_u32(fib, 1);
        return;
    }

    bn *a1 = fib; /* Use output param fib as a1 */

    bn_t a0, tmp, a;
    bn_init_u32(a0, 0); /*  a0 = 0 */
    bn_set_u32(a1, 1);  /*  a1 = 1 */
    bn_init(tmp);       /* tmp = 0 */
    bn_init(a);

    /* Start at second-highest bit set. */
    for (uint64_t k = ((uint64_t) 1) << (62 - __builtin_clzll(n)); k; k >>= 1) {
        /* Both ways use two squares, two adds, one multipy and one shift. */
        bn_lshift(a0, 1, a); /* a03 = a0 * 2 */
        bn_add(a, a1, a);    /*   ... + a1 */
        bn_sqr(a1, tmp);     /* tmp = a1^2 */
        bn_sqr(a0, a0);      /* a0 = a0 * a0 */
        bn_add(a0, tmp, a0); /*  ... + a1 * a1 */
        bn_mul(a1, a, a1);   /*  a1 = a1 * a */
        if (k & n) {
            bn_swap(a1, a0);    /*  a1 <-> a0 */
            bn_add(a0, a1, a1); /*  a1 += a0 */
        }
    }
    /* Now a1 (alias of output parameter fib) = F[n] */

    bn_free(a0);
    bn_free(tmp);
    bn_free(a);
}


static uint64_t foo;

static ssize_t foo_show(struct kobject *kobj,
                        struct kobj_attribute *attr,
                        char *buf)
{
    return snprintf(buf, 1000, "%llu\n", foo);
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
static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);

static char message[8000] = {0};

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

    bn_t fib = BN_INITIALIZER; 

    ktime_t start;
    ktime_t end;

    start = ktime_get();
    //big_fib_sequence_v3(fib, *offset);
    //teacher_fibonacci(fib, *offset);
    naive_fibo(fib, *offset);
    end = ktime_get();

    bn_to_string(fib, message, 8000, 10);
    printk("fib[%llu] : %s\n", *offset, message);

    //message[0] = 'a';

    foo = ktime_to_ns(end) - ktime_to_ns(start);
    copy_to_user(buf, message, strlen(message));
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

    int rc = 0;

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



