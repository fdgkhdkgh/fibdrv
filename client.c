#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

int main()
{
    // int frp;
    FILE *fwp;

    char data[256];

    fwp = fopen("expe_data.txt", "w+");
    // frp = fopen("/sys/kernel/kobject_example/foo", "r");

    if (fwp == NULL) {
        printf("fopen error!\n");
        exit(-1);
    }


    long long sz;

    // char buf[1];
    char buf[8000];
    char write_buf[] = "testing writing";
    int offset = 500; /* TODO: try test something bigger than the limit */

    memset(buf, 0, sizeof(buf));

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    for (int i = 0; i <= offset; i++) {
        sz = write(fd, write_buf, strlen(write_buf));
        printf("Writing to " FIB_DEV ", returned the sequence %lld\n", sz);
    }

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, 1);
        /*printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%lld.\n",
               i, sz);*/

        buf[sz] = 0;

        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "0x%s.\n",
               i, buf);


        FILE *frp;
        // read data
        frp = fopen("/sys/kernel/kobject_example/foo", "r");
        if (frp == NULL) {
            printf("open error\n");
            exit(-1);
        }
        fscanf(frp, "%255s", data);
        fclose(frp);
        // write data
        fprintf(fwp, "%d, %s\n", i, data);

        // printf("buf len : %lu, buf : %s\n", strlen(buf), buf);
    }

    for (int i = offset; i >= 0; i--) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, 1);
        /*printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%lld.\n",
               i, sz);*/

        buf[sz] = 0;

        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "0x%s.\n",
               i, buf);
    }

    close(fd);
    fclose(fwp);
    // fclose(frp);

    return 0;
}
