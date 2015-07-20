#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "mbf.h"

Font *
load_font(const char *fname)
{
    int fd;
    char sig[4];
    Header header;
    int stride;
    size_t ranges_size, data_size;
    Font *font;

    fd = open(fname, O_RDONLY);
    if (fd == -1)
        return NULL;
    read(fd, sig, sizeof(sig));
    if (memcmp(sig, (char []) {'M', 'B', 'F', 0x01}, sizeof(sig))) {
        close(fd);
        return NULL;
    }
    read(fd, &header, sizeof(header));
    /* stride = ceil(w / 8) = floor(w / 8) + (w % 8 ? 1 : 0) */
    stride = (header.w >> 3) + !!(header.w & 7);
    ranges_size = header.nr * sizeof(Range);
    data_size = header.ng * stride * header.h;
    font = malloc(sizeof(Font) + ranges_size + data_size);
    if (!font) {
        close(fd);
        return NULL;
    }
    font->stride = stride;
    font->header = header;
    font->ranges = (Range *) &font[1];
    read(fd, font->ranges, ranges_size);
    font->data = (uint8_t *) &font->ranges[header.nr];
    read(fd, font->data, data_size);
    close(fd);
    return font;
}

int
search_glyph(Font *font, uint16_t code)
{
    int index, i;
    Range r;

    index = 0;
    for (i = 0; i < font->header.nr; i++) {
        r = font->ranges[i];
        if (code < r.offset)
            return -1;
        if (code < r.offset + r.length)
            return index + code - r.offset;
        index += r.length;
    }
    return -1;
}

void
print_glyph(Font *font, uint16_t code)
{
    int index, pixel, i, j;
    uint8_t *row;

    index = search_glyph(font, code);
    if (index == -1)
        return;
    row = &font->data[font->stride * font->header.h * index];
    for (i = 0; i < font->header.h; i++) {
        for (j = 0; j < font->header.w; j++) {
            pixel = row[j >> 3] & (1 << (7 - (j & 7)));
            putchar(pixel ? 'X' : ' ');
        }
        putchar('\n');
        row += font->stride;
    }
}

#if 0
int
main(int argc, char *argv[])
{
    Font *font;

    if (argc != 2)
        return 1;
    font = load_font(argv[1]);
    if (!font)
        return 1;
    //printf("%d\n", search_glyph(font, 200));
    print_glyph(font, 0xFFFD);
    free(font);
    return 0;
}
#endif
