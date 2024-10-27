#ifndef _VTT2ASS_TEXTEXTENTS_H
#define _VTT2ASS_TEXTEXTENTS_H
#include "util.h"
#include "font.h"
#include <hb.h>

struct te_obj {
    //FT_Face ftface;
    hb_buffer_t *hbuf;
    hb_font_t *hfont;
    int fs;

    double fs_mul;
};

void te_create_obj(const char *fontpath, const char *text, int text_len, int fs, bool kern, struct te_obj *out_te);
void te_destroy_obj(struct te_obj *te);

/* out_ext includes the spacing width as well! */
void te_get_at(struct te_obj *te, int offset, int len, float spacing, struct text_extents *out_ext);
void te_get_at_chars(struct te_obj *te, int offset, int len,
        int out_exts_size, struct text_extents out_ext[out_exts_size], int *out_ext_count);
void te_get_at_chars_justify(struct te_obj *te, int offset, int len, int target_justify,
        int out_ext_size, struct text_extents out_ext[out_ext_size], int *out_ext_count);

void te_simple(const char *fontpath, const char *text, int fs, float spacing, bool kern, struct text_extents *out_ext);
#if 0
void te_simple_justify_chars(const char *fontpath, const char *text, int fs, int target_justify,
        int out_ext_size, struct text_extents out_ext[out_ext_size], int *out_ext_count);
#endif

#endif /* _VTT2ASS_TEXTEXTENTS_H */
