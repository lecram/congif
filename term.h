#define A_NORMAL    0x00
#define A_BOLD      0x01
#define A_DIM       0x02
#define A_ITALIC    0x04
#define A_UNDERLINE 0x08
#define A_BLINK     0x10
#define A_INVERSE   0x20
#define A_INVISIBLE 0x40
#define A_CROSSED   0x80

#define M_DISPCTRL  0x0001
#define M_INSERT    0x0002
#define M_NEWLINE   0x0004
#define M_CURSORKEY 0x0008
#define M_WIDETERM  0x0010
#define M_REVERSE   0x0020
#define M_ORIGIN    0x0040
#define M_AUTOWRAP  0x0080
#define M_AUTORPT   0x0100
#define M_MOUSEX10  0x0200
#define M_CURSORVIS 0x0400
#define M_MOUSEX11  0x0800

#define EMPTY       0x0020
#define BCE         1
#if BCE
  #define BLANK (Cell) {EMPTY, def_attr, def_pair}
#else
  #define BLANK (Cell) {EMPTY, term->attr, term->pair}
#endif

#define MAX_PARTIAL 0x100
#define MAX_PARAMS  0x10

typedef struct Cell {
    uint16_t code;
    uint8_t attr;
    uint8_t pair;
} Cell;

typedef enum CharSet {CS_BMP, CS_VTG, CS_437} CharSet;
typedef enum State {S_ANY, S_ESC, S_CSI, S_OSC, S_UNI} State;

typedef struct SaveCursor {
    int row, col;
} SaveCursor;

typedef struct SaveMisc {
    int row, col;
    int origin_on;
    uint8_t attr;
    uint8_t pair;
    CharSet cs_array[2];
    int cs_index;
} SaveMisc;

typedef struct Term {
    int rows, cols;
    int row, col;
    int top, bot;
    uint16_t mode;
    uint8_t attr;
    uint8_t pair;
    Cell **addr;
    Cell *cells;
    CharSet cs_array[2];
    int cs_index;
    SaveCursor save_cursor;
    SaveMisc save_misc;
    State state;
    int parlen;
    int unilen;
    uint8_t partial[MAX_PARTIAL];
    uint8_t plt[0x30];
} Term;

void set_verbosity(int level);
Term *new_term(int rows, int cols);
void parse(Term *term, uint8_t byte);
