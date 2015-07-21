#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "term.h"

void
dump_txt(Term *term, const char *fname)
{
    int i, j, fd;
    uint16_t code;
    char ch;

    fd = creat(fname, 0666);
    if (fd == -1)
        return;
    for (i = 0; i < term->rows; i++) {
        for (j = 0; j < term->cols; j++) {
            code = term->addr[i][j].code;
            if (code >= 0x20 && code < 0x7F)
                ch = (char) code;
            else
                ch = 0x20;
            write(fd, &ch, 1);
        }
        write(fd, "\n", 1);
    }
    close(fd);
}
