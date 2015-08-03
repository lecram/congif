typedef struct Header {
    uint16_t ng;
    uint8_t w, h;
    uint16_t nr;
} Header;

typedef struct Range {
    uint16_t offset, length;
} Range;

typedef struct Font {
    Header header;
    int stride;
    Range *ranges;
    uint8_t *data;
} Font;

Font *load_font(const char *fname);
int search_glyph(Font *font, uint16_t code);
int get_index(Font *font, uint16_t code);
