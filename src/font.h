#ifndef _VTT2ASS_FONT_H
#define _VTT2ASS_FONT_H
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

void font_init();
void font_dinit();

/* Returned face will be free'd with font_dinit() */
FT_Face font_get_face(const char *fontpath);

const char *font_get_name(FT_Face face);

/* debug */
FT_Library font_get_lib();

#endif /* _VTT2ASS_FONT_H */
