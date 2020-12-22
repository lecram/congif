#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/stat.h>

#include "mbf.h"

#define MAX_NAME    0x10
#define FN	    "default_font"

int
main(int argc, char *argv[])
{
    Font *font;
    int i;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s font.mbf\n", argv[0]);
        return 1;
    }
    font = load_font(argv[1]);
    if (font == NULL) {
        fprintf(stderr, "Failed to load font '%s'.\n", argv[1]);
        return 1;
    }

    printf("static Range "FN"_ranges[%d] = {\n", font->header.nr);
    for(i=0; i<font->header.nr; i++) {
        printf("    { %d, %d }%s\n",
            (int) font->ranges[i].offset,
            (int) font->ranges[i].length,
            i+1 == font->header.nr?"":",");
    }
    printf("};\n\n");

    printf("static uint8_t "FN"_data[] = {\n");
    for (i = 0; i < font->header.ng * font->stride * font->header.h; i++) {
        if (i%12 == 0) {
            if (i) printf(",\n");
            printf("    ");
        } else
            printf(", ");
        printf("0x%02x", font->data[i]);
    }
    printf("\n};\n\n");

    printf("Font "FN"[1] = {{ ");

    printf("{ %d, %d, %d, %d }, ",
        (int) font->header.ng,
        (int) font->header.w,
        (int) font->header.h,
        (int) font->header.nr);

    printf("%d, "FN"_ranges, "FN"_data }};\n",
        (int) font->stride);

    free(font);
    return 0;
}
