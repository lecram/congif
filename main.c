#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <float.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>

#include "term.h"
#include "mbf.h"
#include "gif.h"

#define MIN(A, B)   ((A) < (B) ? (A) : (B))
#define MAX(A, B)   ((A) > (B) ? (A) : (B))

#define MIN_DELAY   6

static struct Options {
    char *timings, *dialogue;
    char *output;
    float maxdelay, divisor;
    int loop;
    char *font;
    int height, width;
    int cursor;
    int quiet;
    int barsize;
} options;

uint8_t
get_pair(Term *term, int row, int col)
{
    Cell cell;
    uint8_t fore, back;
    int inverse;

    /* TODO: add support for A_INVISIBLE */
    inverse = term->mode & M_REVERSE;
    if (term->mode & M_CURSORVIS)
        inverse = term->row == row && term->col == col ? !inverse : inverse;
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
    if (cell.attr & A_BLINK)
        back |= 0x8;
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

int
convert_script(Term *term)
{
    FILE *ft;
    int fd;
    float t;
    int n;
    uint8_t ch;
    Font *font;
    int w, h;
    int i, c;
    float d;
    uint16_t rd;
    float lastdone, done;
    char pb[options.barsize+1];
    GIF *gif;

    ft = fopen(options.timings, "r");
    if (!ft) {
        fprintf(stderr, "error: could not load timings: %s\n", options.timings);
        goto no_ft;
    }
    fd = open(options.dialogue, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "error: could not load dialogue: %s\n", options.dialogue);
        goto no_fd;
    }
    font = load_font(options.font);
    if (!font) {
        fprintf(stderr, "error: could not load font: %s\n", options.font);
        goto no_font;
    }
    w = term->cols * font->header.w;
    h = term->rows * font->header.h;
    gif = new_gif(options.output, w, h, term->plt, options.loop);
    if (!gif) {
        fprintf(stderr, "error: could not create GIF: %s\n", options.output);
        goto no_gif;
    }
    /* discard first line of dialogue */
    do read(fd, &ch, 1); while (ch != '\n');
    if (options.barsize) {
        pb[0] = '[';
        pb[options.barsize-1] = ']';
        pb[options.barsize] = '\0';
        for (i = 1; i < options.barsize-1; i++)
            pb[i] = '-';
        lastdone = 0;
        printf("%s\r[", pb);
        /* get number of chunks */
        for (c = 0; fscanf(ft, "%f %d\n", &t, &n) == 2; c++);
        rewind(ft);
    }
    i = 0;
    d = rd = 0;
    while (fscanf(ft, "%f %d\n", &t, &n) == 2) {
        if (options.barsize) {
            done = i * (options.barsize-1) / c;
            if (done > lastdone) {
                while (done > lastdone) {
                    putchar('#');
                    lastdone++;
                }
                fflush(stdout);
            }
        }
        d += (MIN(t, options.maxdelay) * 100.0 / options.divisor);
        rd = (uint16_t) (d + 0.5);
        if (i && rd >= MIN_DELAY) {
            render(term, font, gif, rd);
            d = 0;
        }
        while (n--) {
            read(fd, &ch, 1);
            parse(term, ch);
        }
        if (!options.cursor)
            term->mode &= ~M_CURSORVIS;
        i++;
    }
    if (options.barsize) {
        while (lastdone < options.barsize-2) {
            putchar('#');
            lastdone++;
        }
        putchar('\n');
    }
    render(term, font, gif, MAX(rd, MIN_DELAY));
    close_gif(gif);
    return 0;
no_gif:
    free(font);
no_font:
    close(fd);
no_fd:
    fclose(ft);
no_ft:
    return 1;
}

void
help(char *name)
{
    fprintf(stderr,
        "Usage: %s [options] timings dialogue\n\n"
        "timings:       File generated by script(1)'s -t option\n"
        "dialogue:      File generated by script(1)'s regular output\n\n"
        "options:\n"
        "  -o output    File name of GIF output\n"
        "  -m maxdelay  Maximum delay, as in scriptreplay(1)\n"
        "  -d divisor   Speedup, as in scriptreplay(1)\n"
        "  -l count     GIF loop count (0 = infinite loop)\n"
        "  -f font      File name of MBF font to use\n"
        "  -h lines     Terminal height\n"
        "  -w columns   Terminal width\n"
        "  -c on|off    Show/hide cursor\n"
        "  -q           Quiet mode (don't show progress bar)\n"
        "  -v           Verbose mode (show parser logs)\n"
    , name);
}

void
set_defaults()
{

    options.output = "con.gif";
    options.maxdelay = FLT_MAX;
    options.divisor = 1.0;
    options.loop = -1;
    options.font = "misc-fixed-6x10.mbf";
    options.cursor = 1;
    options.quiet = 0;
    options.barsize = 0;
}

int
main(int argc, char *argv[])
{
    int opt;
    int ret;
    int has_winsize, has_height, has_width;
    Term *term;
    struct winsize size;

    set_defaults();
    has_winsize = has_height = has_width = 0;
    if (ioctl(0, TIOCGWINSZ, &size) != -1) {
        options.height = size.ws_row;
        options.width = size.ws_col;
        has_winsize = 1;
    }
    while ((opt = getopt(argc, argv, "o:m:d:l:f:h:w:c:qv")) != -1) {
        switch (opt) {
        case 'o':
            options.output = optarg;
            break;
        case 'm':
            options.maxdelay = atof(optarg);
            break;
        case 'd':
            options.divisor = atof(optarg);
            break;
        case 'l':
            options.loop = atoi(optarg);
            break;
        case 'f':
            options.font = optarg;
            break;
        case 'h':
            options.height = atoi(optarg);
            has_height = 1;
            break;
        case 'w':
            options.width = atoi(optarg);
            has_width = 1;
            break;
        case 'c':
            if (!strcmp(optarg, "on") || !strcmp(optarg, "1"))
                options.cursor = 1;
            else if (!strcmp(optarg, "off") || !strcmp(optarg, "0"))
                options.cursor = 0;
            break;
        case 'q':
            options.quiet = 1;
            break;
        case 'v':
            set_verbosity(1);
            break;
        default:
            help(argv[0]);
            return 1;
        }
    }
    if (optind >= argc - 1) {
        fprintf(stderr, "error: no input given\n");
        help(argv[0]);
        return 1;
    }
    if (!has_winsize && (!has_height || !has_width)) {
        fprintf(stderr, "error: no terminal size specified\n");
        return 1;
    }
    options.timings = argv[optind++];
    options.dialogue = argv[optind++];
    if (!options.quiet && has_winsize)
        options.barsize = size.ws_col - 1;
    term = new_term(options.height, options.width);
    ret = convert_script(term);
    free(term);
    return ret;
}
