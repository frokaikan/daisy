#include <stdio.h>
#include <ft2build.h>
#include FT_FREETYPE_H

int main (int argc, char** argv) {
    if (argc != 2) {
        printf("Usage : %s fileName\n", argv[0]);
        return 1;
    }

    FT_Library lib;
    if (FT_Init_FreeType(&lib)) {
        printf("error on init lib");
        return 2;
    }

    FT_Face face;
    if (FT_New_Face(lib, argv[1], 0, &face)) {
        printf("error on open face");
        return 3;
    }

    if (FT_Set_Char_Size(face, 0, 16*16, 300, 300)) {
        printf("error on set char size");
        return 4;
    }

    if (FT_Set_Pixel_Sizes(face, 0, 16)) {
        printf("error on set pixel size");
        return 5;
    }

    FT_UInt idx = FT_Get_Char_Index(face, 0);

    if (FT_Load_Glyph(face, idx, 0)) {
        printf("error on load glyph");
        return 6;
    }

    FT_Done_FreeType(lib);

}
