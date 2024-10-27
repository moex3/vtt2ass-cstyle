#include "textextents.h"

#include <wchar.h>
#include <assert.h>
#include <math.h>
#include <sys/param.h>
#include <hb-ft.h>

#if 1 // debug
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_BITMAP_H
#include FT_TRUETYPE_TABLES_H
#endif

// https://github.com/libass/libass/blob/ad42889c85fc61a003ad6d4cdb985f56de066f91/libass/ass_font.c#L278
static void set_font_metrics(FT_Face ftface)
{
    TT_OS2 *os2 = FT_Get_Sfnt_Table(ftface, FT_SFNT_OS2);
    if (os2 && ((short)os2->usWinAscent + (short)os2->usWinDescent != 0)) {
        ftface->ascender  =  (short)os2->usWinAscent;
        ftface->descender = -(short)os2->usWinDescent;
        ftface->height    = ftface->ascender - ftface->descender;
    }
    if (ftface->ascender - ftface->descender == 0 || ftface->height == 0) {
        if (os2 && (os2->sTypoAscender - os2->sTypoDescender) != 0) {
            ftface->ascender = os2->sTypoAscender;
            ftface->descender = os2->sTypoDescender;
            ftface->height = ftface->ascender - ftface->descender;
        } else {
            ftface->ascender = ftface->bbox.yMax;
            ftface->descender = ftface->bbox.yMin;
            ftface->height = ftface->ascender - ftface->descender;
        }
    }
}

void te_create_obj(const char *fontpath, const char *text, int text_len, int fs, bool kern, struct te_obj *out_te)
{
    FT_Face ftface = NULL;
    hb_buffer_t *hbuf = NULL;
    hb_font_t *hfont = NULL;
    hb_feature_t features[8] = {0};
    int feat_idx = 0;
    int err;

    // https://github.com/libass/libass/blob/master/libass/ass_render.c#L2039
    //double fs = 256.0;
    //double fs_mul = o_fs / fs;
    //printf("Glyph scale: %f\n", fs_mul);

    ftface = font_get_face(fontpath);
    assert(ftface);

    hbuf = hb_buffer_create();
    assert(hbuf);

    set_font_metrics(ftface);

    FT_Size_RequestRec rq = {
        .type = FT_SIZE_REQUEST_TYPE_REAL_DIM,
        .width = 0,
        .height = lrint(fs * 64),
    };
    err = FT_Request_Size(ftface, &rq);
    assert(!err);

    hfont = hb_ft_font_create_referenced(ftface);
    assert(hfont);
    //hb_ft_font_set_funcs(hfont);
    //hb_face_set_upem(hb_font_get_face(hfont), ftface->units_per_EM);
    hb_font_set_scale(hfont, 
            ((uint64_t)ftface->size->metrics.x_scale * (uint64_t)ftface->units_per_EM) >> 16,
            ((uint64_t)ftface->size->metrics.y_scale * (uint64_t)ftface->units_per_EM) >> 16);
    hb_font_set_ppem(hfont, ftface->size->metrics.x_ppem, ftface->size->metrics.y_ppem);

    hb_buffer_set_direction(hbuf, HB_DIRECTION_LTR);
    hb_buffer_set_script(hbuf, hb_script_from_string("Jpan", -1));
    hb_buffer_set_language(hbuf, hb_language_from_string("jp", -1));

    /* Also do some if text is vertical https://github.com/libass/libass/blob/master/libass/ass_shaper.c#L175 */

    /* Ligatures should be disabled when spacing > 0 but we don't have that info here yet */
    features[feat_idx++] = (hb_feature_t){
        .tag = HB_TAG('l', 'i', 'g', 'a'),
        .end = HB_FEATURE_GLOBAL_END,
        .start = HB_FEATURE_GLOBAL_START,
        .value = 0,
    };
    features[feat_idx++] = (hb_feature_t){
        .tag = HB_TAG('c', 'l', 'i', 'g'),
        .end = HB_FEATURE_GLOBAL_END,
        .start = HB_FEATURE_GLOBAL_START,
        .value = 0,
    };

    /* Keming */
    features[feat_idx++] = (hb_feature_t){
        .tag = HB_TAG('k', 'e', 'r', 'n'),
        .end = HB_FEATURE_GLOBAL_END,
        .start = HB_FEATURE_GLOBAL_START,
        .value = kern,
    };


    hb_buffer_add_utf8(hbuf, text, text_len, 0, text_len);
    hb_shape(hfont, hbuf, features, feat_idx);

    *out_te = (struct te_obj){
        .hbuf = hbuf,
        .hfont = hfont,
        .fs = fs,
        .fs_mul = 1,
    };
}

void te_destroy_obj(struct te_obj *te)
{
    hb_buffer_destroy(te->hbuf);
    hb_font_destroy(te->hfont);
}

static void find_cluster_indexes(unsigned int gcount, hb_glyph_info_t gi[gcount],
        int offset, int len, int *out_first_idx, int *out_last_idx)
{
    int cluster_start = -1, cluster_end = -1;
    if (len == -1)
        cluster_end = gcount - 1;
    for (unsigned int i = 0; i < gcount; i++) {
        if (gi[i].cluster == offset) {
            cluster_start = i;
        }

        if (cluster_end == -1 && gi[i].cluster >= offset + len) {
            /* Because the point-at-end problem described below */
            cluster_end = i - 1;
        }

        if (cluster_start != -1 && cluster_end != -1) {
            break;
        }
    }
    if (cluster_end == -1) {
        /* If len is != -1, but no cluster index is found, it's probably
         * because len points to at the end of the string, while the
         * cluster index points at the start. So this basically a
         * off by one error, but 1 character is multiple bytes.
         * So let's just treat it as the whole string */
        cluster_end = gcount - 1;
    }
    assert(cluster_start != -1 && cluster_end != -1);

    *out_first_idx = cluster_start;
    *out_last_idx = cluster_end;
}

void te_get_at(struct te_obj *te, int offset, int len, float spacing, struct text_extents *out_ext)
{
    memset(out_ext, 0, sizeof(*out_ext));

    unsigned int glyph_count;
    int cluster_start, cluster_end;
    hb_glyph_info_t *glyph_info    = hb_buffer_get_glyph_infos(te->hbuf, &glyph_count);
    hb_glyph_position_t *glyph_pos = hb_buffer_get_glyph_positions(te->hbuf, &glyph_count);

    find_cluster_indexes(glyph_count, glyph_info, offset, len, &cluster_start, &cluster_end);

    FT_Face ftf = hb_ft_font_get_face(te->hfont);
    int width = 0, height = 0;
    for (int i = cluster_start; i <= cluster_end; i++) {
        width += (glyph_pos[i].x_advance * te->fs_mul) + (spacing * 64);
#if 0
        printf("Offset x: %d  y: %d\n", glyph_pos[i].x_offset, glyph_pos[i].y_offset);
        int r = FT_Load_Glyph(ftf, glyph_info[i].codepoint, 0);
        assert(r == 0);
        r = FT_Render_Glyph(ftf->glyph, 0);
        assert(r == 0);
        FT_Bitmap *bmpo = &ftf->glyph->bitmap;
        //FT_Bitmap bmp;
        //FT_Bitmap_Init(&bmp);
        //r = FT_Bitmap_Convert(font_get_lib(), bmpo, &bmp, 4);
        //assert(r == 0);

        //BP;
        FILE* f = fopen("./img.bin", "wb");
        fwrite(bmpo->buffer, 1, bmpo->rows * bmpo->width, f);
        printf("img w: %d  h: %d\n", bmpo->width, bmpo->rows);
        fclose(f);

        //FT_Bitmap_Done(font_get_lib(), &bmp);
#endif
    }

    out_ext->width = (width / 64.0f);
    out_ext->height = te->fs * te->fs_mul;
}

void te_get_at_chars(struct te_obj *te, int offset, int len,
        int out_ext_size, struct text_extents out_ext[out_ext_size], int *out_ext_count)
{
    int cluster_start, cluster_end, gc;
    hb_glyph_info_t *gi     = hb_buffer_get_glyph_infos(te->hbuf, &gc);
    hb_glyph_position_t *gp = hb_buffer_get_glyph_positions(te->hbuf, &gc);

    find_cluster_indexes(gc, gi, offset, len, &cluster_start, &cluster_end);

    *out_ext_count = 0;
    for (int i = cluster_start; i <= cluster_end; i++) {
        assert(*out_ext_count < out_ext_size);
        struct text_extents *curr_ext = &out_ext[*out_ext_count];

        *curr_ext = (struct text_extents){
            .width = gp[i].x_advance / 64,
            .height = te->fs,
        };
        (*out_ext_count)++;
    }
}

void te_get_at_chars_justify(struct te_obj *te, int offset, int len, int target_justify,
        int out_ext_size, struct text_extents out_ext[out_ext_size], int *out_ext_count)
{
    int unjust_size = 0;
    float space = 0;

    /* 1st calculate all of the sizes of each char */
    te_get_at_chars(te, offset, len, out_ext_size, out_ext, out_ext_count);

    /* 2nd calculate the total size of the unjustified text */
    for (int i = 0; i < *out_ext_count; i++) {
        unjust_size += out_ext[i].width;
    }

    /* 3rd calculate the spaces needed on
     * - before the 1st char
     * - between each char
     * - after the last char
     */
    if (target_justify <= unjust_size)
        return;
    space = (target_justify - unjust_size) / (*out_ext_count + 1);

    /* 4th modify x_off of text_extents */
    for (int i = 0; i < *out_ext_count; i++) {
        out_ext[i].x_off = space;
    }
}

void te_simple(const char *fontpath, const char *text, int fs, float spacing, bool kern, struct text_extents *out_ext)
{
    struct te_obj te;
    te_create_obj(fontpath, text, -1, fs, kern, &te);
    te_get_at(&te, 0, -1, spacing, out_ext);
    te_destroy_obj(&te);
}

#if 0
void te_simple_justify_chars(const char *fontpath, const char *text, int fs, int target_justify,
        int out_ext_size, struct text_extents out_ext[out_ext_size], int *out_ext_count)
{
    struct te_obj te;
    te_create_obj(fontpath, text, -1, fs, spacing, &te);
    te_get_at_chars_justify(&te, 0, -1, target_justify, out_ext_size, out_ext, out_ext_count);
    te_destroy_obj(&te);
}
#endif
