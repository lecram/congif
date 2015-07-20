#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "term.h"
//#include "dump.h"
#include "mbf.h"
#include "gif.h"

void
parse_script(Term *term, const char *timing, const char *dialogue)
{
    FILE *ft;
    int fd;
    float t;
    int n;
    uint8_t ch;

    ft = fopen(timing, "r");
    fd = open(dialogue, O_RDONLY);
    if (ft == NULL || fd == -1)
        return;
    /* discard first line of dialogue */
    do read(fd, &ch, 1); while (ch != '\n');
    while (fscanf(ft, "%f %d\n", &t, &n) == 2) {
        while (n--) {
            read(fd, &ch, 1);
            parse(term, ch);
        }
    }
    close(fd);
    fclose(ft);
}

int
get_index(Font *font, uint16_t code)
{
    int index;

    index = search_glyph(font, code);
    if (index == -1)
        index = search_glyph(font, 0xFFFD);
    if (index == -1)
        index = search_glyph(font, 0x003F);
    if (index == -1)
        index = search_glyph(font, 0x0020);
    if (index == -1)
        index = search_glyph(font, 0x0000);
    return index;
}

uint8_t
get_pair(Term *term, int row, int col)
{
    Cell cell;
    uint8_t fore, back;
    int inverse;

    /* TODO: add support for A_INVISIBLE */
    inverse = term->mode & M_REVERSE;
    //~ if (term->mode & M_CURSORVIS)
        //~ inverse = term->row == row && term->col == col ? !inverse : inverse;
    cell = term->addr[row][col];
    inverse = cell.attr & A_INVERSE ? !inverse : inverse;
    if (inverse) {
        fore = cell.pair & 0xF;
        back = cell.pair >> 4;
    } else {
        fore = cell.pair >> 4;
        back = cell.pair & 0xF;
    }
    if (cell.attr & (A_DIM | A_UNDERLINE))
        fore = 0x6;
    else if (cell.attr & (A_ITALIC | A_CROSSED))
        fore = 0x2;
    if (cell.attr & A_BOLD)
        fore |= 0x8;
    return (fore << 4) | (back & 0xF);
}

void
draw_char(Font *font, GIF *gif, uint16_t code, uint8_t pair, int row, int col)
{
    int i, j;
    int x, y;
    int index;
    int pixel;
    uint8_t *strip;

    index = get_index(font, code);
    if (index == -1)
        return;
    strip = &font->data[font->stride * font->header.h * index];
    y = font->header.h * row;
    for (i = 0; i < font->header.h; i++) {
        x = font->header.w * col;
        for (j = 0; j < font->header.w; j++) {
            pixel = strip[j >> 3] & (1 << (7 - (j & 7)));
            gif->cur[y * gif->w + x] = pixel ? pair >> 4 : pair & 0xF;
            x++;
        }
        y++;
        strip += font->stride;
    }
}

void
render(Term *term, Font *font, GIF *gif, uint16_t delay)
{
    int i, j;
    uint16_t code;
    uint8_t pair;

    for (i = 0; i < term->rows; i++) {
        for (j = 0; j < term->cols; j++) {
            code = term->addr[i][j].code;
            pair = get_pair(term, i, j);
            draw_char(font, gif, code, pair, i, j);
        }
    }
    add_frame(gif, delay);
}

void
convert_script(Term *term, const char *timing, const char *dialogue,
               const char *mbf, const char *anim)
{
    FILE *ft;
    int fd;
    float t;
    int n;
    uint8_t ch;
    Font *font;
    int w, h;
    int i;
    uint16_t d;
    GIF *gif;

    ft = fopen(timing, "r");
    if (!ft)
        goto no_ft;
    fd = open(dialogue, O_RDONLY);
    if (fd == -1)
        goto no_fd;
    font = load_font(mbf);
    if (!font)
        goto no_font;
    w = term->cols * font->header.w;
    h = term->rows * font->header.h;
    gif = new_gif(anim, w, h, term->plt);
    if (!gif)
        goto no_gif;
    /* discard first line of dialogue */
    do read(fd, &ch, 1); while (ch != '\n');
    i = 0;
    while (fscanf(ft, "%f %d\n", &t, &n) == 2) {
        d = (uint16_t) (t * 100.0 / 3.0);
        if (i)
            render(term, font, gif, d);
        while (n--) {
            read(fd, &ch, 1);
            parse(term, ch);
        }
        i++;
    }
    render(term, font, gif, 0);
    close_gif(gif);
no_gif:
    free(font);
no_font:
    close(fd);
no_fd:
    fclose(ft);
no_ft:
    return;
}

int
main(int argc, char *argv[])
{
    Term *term;

    if (argc != 3)
        return 1;
    term = new_term(30, 80);
    //parse_script(term, argv[1], argv[2]);
    //dump_txt(term, "matrix.txt");
    convert_script(term, argv[1], argv[2], "6x11.mbf", "con.gif");
    free(term);
    return 0;
}
