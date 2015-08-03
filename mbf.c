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

int
get_index(Font *font, uint16_t code)
{
    int index;
    uint16_t *cur_code;
    uint16_t codes[] = {code, 0xFFFD, 0x003F, 0x0020, 0};

    for (cur_code = &codes[0]; *cur_code; cur_code++) {
        index = search_glyph(font, *cur_code);
        if (index != -1)
            break;
    };
    return index;
}
