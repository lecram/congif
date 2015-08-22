#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>

#include "term.h"
#include "default.h"
#include "cs_vtg.h"
#include "cs_437.h"

#define MAX(A, B)   ((A) > (B) ? (A) : (B))

static int verbose = 0;

static void
logfmt(char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    if (verbose)
        vfprintf(stderr, fmt, args);
    va_end(args);
}

void
set_verbosity(int level)
{
    verbose = level;
}

static void
save_cursor(Term *term)
{
    term->save_cursor.row = term->row;
    term->save_cursor.col = term->col;
}

static void
load_cursor(Term *term)
{
    term->row = term->save_cursor.row;
    term->col = term->save_cursor.col;
}

static void
save_misc(Term *term)
{
    term->save_misc.row = term->row;
    term->save_misc.col = term->col;
    term->save_misc.origin_on = term->mode & M_ORIGIN;
    term->save_misc.attr = term->attr;
    term->save_misc.pair = term->pair;
    term->save_misc.cs_array[0] = term->cs_array[0];
    term->save_misc.cs_array[1] = term->cs_array[1];
    term->save_misc.cs_index = term->cs_index;
}

static void
load_misc(Term *term)
{
    term->row = term->save_misc.row;
    term->col = term->save_misc.col;
    if (term->save_misc.origin_on)
        term->mode |= M_ORIGIN;
    else
        term->mode &= ~M_ORIGIN;
    term->attr = term->save_misc.attr;
    term->pair = term->save_misc.pair;
    term->cs_array[0] = term->save_misc.cs_array[0];
    term->cs_array[1] = term->save_misc.cs_array[1];
    term->cs_index = term->save_misc.cs_index;
}

static void
reset(Term *term)
{
    int i, j;

    term->row = term->col = 0;
    term->top = 0;
    term->bot = term->rows - 1;
    term->mode = def_mode;
    term->attr = def_attr;
    term->pair = def_pair;
    term->cs_array[0] = CS_BMP;
    term->cs_array[1] = CS_VTG;
    term->cs_index = 0;
    term->state = S_ANY;
    term->parlen = 0;
    memcpy(term->plt, def_plt, sizeof(term->plt));
    for (i = 0; i < term->rows; i++) {
        term->addr[i] = &term->cells[i*term->cols];
        for (j = 0; j < term->cols; j++)
            term->addr[i][j] = (Cell) {EMPTY, def_attr, def_pair};
    }
    save_cursor(term);
    save_misc(term);
}

Term *
new_term(int rows, int cols)
{
    size_t size = sizeof(Term) + rows*sizeof(Cell *) + rows*cols*sizeof(Cell);
    Term *term = malloc(size);
    if (!term)
        return NULL;
    term->rows = rows;
    term->cols = cols;
    term->addr = (Cell **) &term[1];
    term->cells = (Cell *) &term->addr[rows];
    reset(term);
    return term;
}

static uint16_t
char_code(Term *term)
{
    int i;
    uint16_t code = term->partial[0] & ((1 << (8 - term->parlen)) - 1);
    for (i = 1; i < term->parlen; i++)
        code = (code << 6) | (term->partial[i] & 0x3F);
    return code;
}

static int
within_bounds(Term *term, int row, int col)
{
    if (row < 0 || row >= term->rows || col < 0 || col > term->cols) {
        logfmt("position %d,%d is out of bounds %d,%d\n",
               row+1, col+1, term->rows, term->cols);
        return 0;
    } else {
        return 1;
    }
}

/* Move lines down and put a blank line at the top. */
static void
scroll_up(Term *term)
{
    int row, col;
    Cell *addr;

    if (!within_bounds(term, term->top, 0))
        return;
    if (!within_bounds(term, term->bot, 0))
        return;
    addr = term->addr[term->bot];
    for (row = term->bot; row > term->top; row--)
        term->addr[row] = term->addr[row-1];
    term->addr[term->top] = addr;
    for (col = 0; col < term->cols; col++)
        term->addr[term->top][col] = BLANK;
}

/* Move lines up and put a blank line at the bottom. */
static void
scroll_down(Term *term)
{
    int row, col;
    Cell *addr;

    if (!within_bounds(term, term->top, 0))
        return;
    if (!within_bounds(term, term->bot, 0))
        return;
    addr = term->addr[term->top];
    for (row = term->top; row < term->bot; row++)
        term->addr[row] = term->addr[row+1];
    term->addr[term->bot] = addr;
    for (col = 0; col < term->cols; col++)
        term->addr[term->bot][col] = BLANK;
}

static void
addchar(Term *term, uint16_t code)
{
    Cell cell = (Cell) {code, term->attr, term->pair};
    if (term->col >= term->cols) {
        if (term->mode & M_AUTOWRAP) {
            term->col = 0;
            if (term->row < term->bot)
                term->row++;
            else
                scroll_down(term);
        } else {
            term->col = term->cols - 1;
        }
    }
    if (!within_bounds(term, term->row, term->col))
        return;
    if (term->mode & M_INSERT) {
        Cell next;
        int col;
        for (col = term->col; col < term->cols; col++) {
            next = term->addr[term->row][col];
            term->addr[term->row][col] = cell;
            cell = next;
        }
    } else {
        term->addr[term->row][term->col] = cell;
    }
    term->col++;
}

static void
linefeed(Term *term)
{
    if (term->row == term->bot)
        scroll_down(term);
    else
        term->row++;
    if (term->mode & M_NEWLINE)
        term->col = 0;
}

static void
ctrlchar(Term *term, uint8_t byte)
{
    switch (byte) {
    case 0x08:
        if (term->col) term->col--;
        break;
    case 0x09:
        /* TODO: go to next tab stop or end of line */
        logfmt("NYI: Control Character 0x09 (TAB)\n");
        break;
    case 0x0A: case 0x0B: case 0x0C:
        linefeed(term);
        break;
    case 0x0D:
        term->col = 0;
        break;
    case 0x0E:
        term->cs_index = 1;
        break;
    case 0x0F:
        term->cs_index = 0;
        break;
    }
}

static void
escseq(Term *term, uint8_t byte)
{
    uint8_t first, second;

    if (term->parlen) {
        first = *term->partial;
        second = byte;
    } else {
        first = byte;
        second = 0;
    }
    switch (first) {
    case 'c':
        reset(term);
        break;
    case 'D':
        if (term->row == term->bot)
            scroll_down(term);
        else
            term->row++;
        break;
    case 'E':
        if (term->row == term->bot) {
            scroll_down(term);
            term->col = 0;
        } else {
            term->row++;
        }
        break;
    case 'H':
        /* TODO: set tab stop at current column */
        logfmt("NYI: ESC Sequence H (HTS)\n");
        break;
    case 'M':
        if (term->row == term->top)
            scroll_up(term);
        else
            term->row--;
        break;
    case 'Z':
        /* Identify Terminal (DECID) */
        /* if we were a real terminal, we'd reply with "ESC [ ? 6 c" */
        /* since there is no application listening, we can ignore this */
        break;
    case '7':
        save_misc(term);
        break;
    case '8':
        load_misc(term);
        break;
    case '%':
        /* TODO: select charset */
        logfmt("NYI: ESC Sequence %% (character set selection)\n");
        break;
    case '#':
        /* TODO: DEC screen alignment test */
        logfmt("NYI: ESC Sequence # (DECALN)\n");
        break;
    case '(':
        switch (second) {
        case 'B':
            term->cs_array[0] = CS_BMP;
            break;
        case '0':
            term->cs_array[0] = CS_VTG;
            break;
        case 'U':
            term->cs_array[0] = CS_437;
            break;
        case 'K':
            logfmt("UNS: user-defined mapping\n");
        }
        break;
    case ')':
        switch (second) {
        case 'B':
            term->cs_array[1] = CS_BMP;
            break;
        case '0':
            term->cs_array[1] = CS_VTG;
            break;
        case 'U':
            term->cs_array[1] = CS_437;
            break;
        case 'K':
            logfmt("UNS: user-defined mapping\n");
        }
        break;
    case '>':
        /* TODO: set numeric keypad mode */
        logfmt("NYI: ESC Sequence > (DECPNM)\n");
        break;
    case '=':
        /* TODO: set application keypad mode */
        logfmt("NYI: ESC Sequence = (DECPAM)\n");
        break;
    default:
        logfmt("UNS: ESC Sequence %c\n", first);
    }
}

static int
getparams(char *partial, int *params, int n)
{
    char *next;
    char *token = partial;
    int i = 0;
    if (!*partial) {
        params[0] = 0;
        return 1;
    }
    while (i < n && *token) {
        params[i++] = strtol(token, &next, 10);
        if (*next) next++;
        token = next;
    }
    if (i < n && *(token-1) == ';')
        params[i++] = 0;
    return i;
}

#define SWITCH(T, F, V) (T)->mode = (V) ? (T)->mode | (F) : (T)->mode & ~(F)

static void
modeswitch(Term *term, int private, int number, int value)
{
    if (private) {
        /* DEC modes */
        switch (number) {
        case 1:
            SWITCH(term, M_CURSORKEY, value);
            break;
        case 3:
            /* TODO: 80/132 columns mode switch */
            logfmt("NYI: DEC mode 3\n");
            break;
        case 5:
            SWITCH(term, M_REVERSE, value);
            break;
        case 6:
            SWITCH(term, M_ORIGIN, value);
            term->row = term->top;
            term->col = 0;
            break;
        case 7:
            SWITCH(term, M_AUTOWRAP, value);
            break;
        case 8:
            SWITCH(term, M_AUTORPT, value);
            break;
        case 9:
            SWITCH(term, M_MOUSEX10, value);
            break;
        case 25:
            SWITCH(term, M_CURSORVIS, value);
            break;
        case 1000:
            SWITCH(term, M_MOUSEX11, value);
            break;
        default:
            logfmt("UNS: DEC mode %d\n", number);
        }
    } else {
        /* ANSI modes */
        switch (number) {
        case 3:
            SWITCH(term, M_DISPCTRL, value);
            break;
        case 4:
            SWITCH(term, M_INSERT, value);
            break;
        case 20:
            SWITCH(term, M_NEWLINE, value);
            break;
        default:
            logfmt("UNS: ANSI mode %d\n", number);
        }
    }
}

static void
sgr(Term *term, int number)
{
    switch (number) {
    case 0:
        term->attr = def_attr;
        term->pair = def_pair;
        break;
    case 1:
        term->attr |= A_BOLD;
        break;
    case 2:
        term->attr |= A_DIM;
        break;
    case 4:
        term->attr |= A_UNDERLINE;
        break;
    case 5:
        term->attr |= A_BLINK;
        break;
    case 7:
        term->attr |= A_INVERSE;
        break;
    case 10:
        /* TODO: reset toggle meta flag */
        term->cs_array[term->cs_index = 0] = CS_BMP;
        term->mode &= ~M_DISPCTRL;
        break;
    case 11:
        /* TODO: reset toggle meta flag */
        term->cs_array[term->cs_index] = CS_437;
        term->mode |= M_DISPCTRL;
        break;
    case 12:
        /* TODO: set toggle meta flag */
        term->cs_array[term->cs_index] = CS_437;
        term->mode |= M_DISPCTRL;
        break;
    case 21:
        term->attr &= ~A_BOLD;
        break;
    case 22:
        term->attr &= ~A_DIM;
        break;
    case 24:
        term->attr &= ~A_UNDERLINE;
        break;
    case 25:
        term->attr &= ~A_BLINK;
        break;
    case 27:
        term->attr &= ~A_INVERSE;
        break;
    case 30: case 31: case 32: case 33: case 34: case 35: case 36: case 37:
        term->pair = ((number - 30) << 4) | (term->pair & 0x0F);
        break;
    case 38:
        term->attr |= A_UNDERLINE;
        term->pair = (DEF_FORE << 4) | (term->pair & 0x0F);
        break;
    case 39:
        term->attr &= ~A_UNDERLINE;
        term->pair = (DEF_FORE << 4) | (term->pair & 0x0F);
        break;
    case 40: case 41: case 42: case 43: case 44: case 45: case 46: case 47:
        term->pair = (term->pair & 0xF0) | (number - 40);
        break;
    case 49:
        term->pair = (term->pair & 0xF0) | DEF_BACK;
        break;
    default:
        logfmt("UNS: SGR %d\n", number);
    }
}

static void
ctrlseq(Term *term, uint8_t byte)
{
    int private;
    int n, k, k1;
    int params[MAX_PARAMS];
    char *str;
    int ra, rb, ca, cb;
    int i, j;
    Cell cell;

    if (!within_bounds(term, term->row, term->col))
        return;
    term->partial[term->parlen] = '\0';
    if (*term->partial == '?') {
        private = 1;
        str = (char *) term->partial + 1;
    } else {
        private = 0;
        str = (char *) term->partial;
    }
    n = getparams(str, params, MAX_PARAMS);
    k = n ? *params : 0;
    k1 = k ? k : 1;
    switch (byte) {
    case '@':
        /* TODO: insert the indicated # of blank characters */
        logfmt("NYI: Control Sequence @ (ICH)\n");
        break;
    case 'A':
        term->row -= k1;
        break;
    case 'B': case 'e':
        term->row += k1;
        break;
    case 'C': case 'a':
        term->col += k1;
        break;
    case 'D':
        term->col -= k1;
        break;
    case 'E':
        term->row += k1;
        term->col = 0;
        break;
    case 'F':
        term->row -= k1;
        term->col = 0;
        break;
    case 'G': case '`':
        term->col = k1 - 1;
        break;
    case 'H': case 'f':
        if (n == 2) {
            term->row = MAX(params[0], 1) - 1;
            term->col = MAX(params[1], 1) - 1;
        } else {
            term->row = term->col = 0;
        }
        if (term->mode & M_ORIGIN)
            term->row += term->top;
        break;
    case 'J':
        ra = 0; rb = term->rows - 1;
        ca = 0; cb = term->cols - 1;
        if (k == 0) {
            ra = term->row;
            ca = term->col;
        } else if (k == 1) {
            rb = term->row;
            cb = term->col;
        }
        for (j = ca; j < term->cols; j++)
            term->addr[ra][j] = BLANK;
        for (i = ra+1; i < rb; i++) {
            for (j = 0; j < term->cols; j++) {
                term->addr[i][j] = BLANK;
            }
        }
        for (j = 0; j <= cb; j++)
            term->addr[rb][j] = BLANK;
        break;
    case 'K':
        ca = 0; cb = term->cols - 1;
        if (k == 0)
            ca = term->col;
        else if (k == 1)
            cb = term->col;
        for (j = ca; j <= cb; j++)
            term->addr[term->row][j] = BLANK;
        break;
    case 'L':
        if (term->row < term->top || term->row > term->bot)
            break;
        /* This is implemented naively:
             1. temporarily change the top margin to current row;
             2. scroll up as many times as requested;
             3. restore top margin to previous value. */
        i = term->top;
        term->top = term->row;
        for (j = 0; j < k1; j++)
            scroll_up(term);
        term->top = i;
        break;
    case 'M':
        if (term->row < term->top || term->row > term->bot)
            break;
        /* This is implemented naively:
             1. temporarily change the top margin to current row;
             2. scroll down as many times as requested;
             3. restore top margin to previous value. */
        /* TODO:
             vt102-ug says:
               "Lines added to bottom of screen have spaces with same character
               attributes as last line moved up."
             we need a more flexible scroll_down() to fix this. */
        i = term->top;
        term->top = term->row;
        for (j = 0; j < k1; j++)
            scroll_down(term);
        term->top = i;
        break;
    case 'P':
        cell = term->addr[term->row][term->cols-1];
        cell.code = EMPTY;
        for (j = term->col; j < term->cols-k1; j++)
            term->addr[term->row][j] = term->addr[term->row][j+k1];
        for (j = term->cols-k1; j < term->cols; j++)
            term->addr[term->row][j] = cell;
        break;
    case 'X':
        for (j = 0; j < k1; j++)
            term->addr[term->row][term->col+j] = BLANK;
        break;
    case 'c':
        /* Device Attributes (DA) */
        /* if we were a real terminal, we'd reply with "ESC [ ? 6 c" */
        /* since there is no application listening, we can ignore this */
        break;
    case 'd':
        term->row = k1 - 1;
        break;
    case 'g':
        /* TODO: clear tab stop */
        logfmt("NYI: Control Sequence g (TBC)\n");
        break;
    case 'h':
        for (i = 0; i < n; i++)
            modeswitch(term, private, params[i], 1);
        break;
    case 'l':
        for (i = 0; i < n; i++)
            modeswitch(term, private, params[i], 0);
        break;
    case 'm':
        for (i = 0; i < n; i++)
            sgr(term, params[i]);
        break;
    case 'n':
        /* Device Status Report (DSR) */
        /* if we were a real terminal, we'd send a status reply (e.g. CPR) */
        /* since there is no application listening, we can ignore this */
        break;
    case 'q':
        /* TODO: set keyboard LEDs */
        logfmt("NYI: Control Sequence q (DECLL)\n");
        break;
    case 'r':
        if (n == 2) {
            term->top = MAX(params[0], 1) - 1;
            term->bot = MAX(params[1], 1) - 1;
        } else {
            term->top = 0;
            term->bot = term->rows - 1;
        }
        term->row = term->mode & M_ORIGIN ? term->top : 0;
        term->col = 0;
        break;
    case 's':
        save_cursor(term);
        break;
    case 'u':
        load_cursor(term);
        break;
    default:
        logfmt("UNS: Control Sequence %c\n", byte);
    }
}

#define CHARSET(T)      ((T)->cs_array[(T)->cs_index])
#define PARCAT(T, B)    ((T)->partial[(T)->parlen++] = (B))
#define RESET_STATE(T)  do { (T)->state = S_ANY; (T)->parlen = 0; } while(0)
#define CHARLEN(B)      ((B) < 0xE0 ? 2 : ((B) < 0xF0 ? 3 : 4))

void
parse(Term *term, uint8_t byte)
{
    int es;

    if (byte != 0x1B && byte < 0x20 && !(term->mode & M_DISPCTRL)) {
        ctrlchar(term, byte);
    } else {
        switch (term->state) {
        case S_ANY:
            switch (byte) {
            case 0x1B:
                term->state = S_ESC;
                break;
            case 0x9B:
                term->state = S_CSI;
                break;
            default:
                switch (CHARSET(term)) {
                case CS_BMP:
                    if (byte < 0x80) {
                        /* single-byte UTF-8, i.e. ASCII */
                        addchar(term, byte);
                    } else {
                        term->unilen = CHARLEN(byte);
                        PARCAT(term, byte);
                        term->state = S_UNI;
                    }
                    break;
                case CS_VTG:
                    addchar(term, cs_vtg[byte]);
                    break;
                case CS_437:
                    addchar(term, cs_437[byte]);
                    break;
                }
            }
            break;
        case S_ESC:
            es = 1;
            if (!term->parlen) {
                if (byte == 0x5B) {
                    term->state = S_CSI;
                    es = 0;
                } else if (byte == 0x5D) {
                    term->state = S_OSC;
                    es = 0;
                }
            }
            if (es) {
                if (byte >= 0x20 && byte < 0x30) {
                    PARCAT(term, byte);
                } else {
                    escseq(term, byte);
                    RESET_STATE(term);
                }
            }
            break;
        case S_CSI:
            if (byte < 0x40 || byte >= 0x7F) {
                PARCAT(term, byte);
            } else {
                ctrlseq(term, byte);
                RESET_STATE(term);
            }
            break;
        case S_OSC:
            /* TODO: set/reset palette entries */
            logfmt("NYI: Operating System Sequence\n");
            RESET_STATE(term);
            break;
        case S_UNI:
            PARCAT(term, byte);
            if (term->parlen == term->unilen) {
                addchar(term, char_code(term));
                RESET_STATE(term);
            }
            break;
        }
    }
}
