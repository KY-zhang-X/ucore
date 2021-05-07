#include <ulib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stat.h>
#include <file.h>
#include <dir.h>
#include <unistd.h>

#define printf(...)                     fprintf(1, __VA_ARGS__)
#define BUFSIZE                         4096

static char buf[BUFSIZE];

int main(int argc, char *argv[])
{
    printf("\n");
    // {
    //     struct stat _stat;
    //     fstat(1, &_stat);
    //     cprintf("aft mode:%x nlinks:%d\n", _stat.st_mode, _stat.st_nlinks);
    // }

    if (argc == 1) {
        printf("This Function is not Implement!\n");
        return -1;
    }
    else {
        for (int i = 1; i < argc; i++) {
            struct stat stat;
            int fd;
            if ((fd = open(argv[i], O_RDONLY)) < 0) {
                printf("Can't find file named \"%s\"\n", argv[i]);
                return -1;
            }
            fstat(fd, &stat);
            int resid = stat.st_size;
            while (resid > 0) {
                int len = stat.st_size < BUFSIZE ? stat.st_size : BUFSIZE;
                len = read(fd, buf, len);
                // buf[len] = '\0';
                // printf(buf);
                write(1, buf, len);
                resid -= len;
            }
            close(fd);
            printf("\n");
        }
    }
}