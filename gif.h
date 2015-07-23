#include <stdint.h>

typedef struct GIF {
    uint16_t w, h;
    int fd;
    int offset;
    uint8_t *cur, *old;
    uint32_t partial;
    uint8_t buffer[0xFF];
} GIF;

GIF *new_gif(const char *fname, uint16_t w, uint16_t h, uint8_t *gct, int loop);
void add_frame(GIF *gif, uint16_t d);
void close_gif(GIF* gif);
