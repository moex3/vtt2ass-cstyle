#ifndef _VTT2ASS_ASS_H
#define _VTT2ASS_ASS_H
#include "parser.h"
#include "cuepos.h"
#include "dyna.h"
#include "ass_style.h"
#include "cuestyle.h"
#include "stack.h"

#define IS_ASS_ALIGN_LEFT(al) (al == 1 || al == 4 || al == 7)
#define IS_ASS_ALIGN_RIGHT(al) (al == 3 || al == 6 || al == 9)
#define IS_ASS_ALIGN_TOP(al) (al == 7 || al == 8 || al == 9)
#define IS_ASS_ALIGN_BOTTOM(al) (al == 1 || al == 2 || al == 3)

struct ass_node {
    char *text; /* Free */
    int layer;
    int64_t start_ms, end_ms;
    struct ass_style *style; /* Can be NULL, pointer into the styles dyna */
    //const char *style; /* Can be NULL */
};

struct ass_cue_pos {
    int align; /* numpad style ass align */
    int logical_align; /* The align for internal calculations (only for vertical text) */
    //int margin_l, margin_r, margin_v;
    int posx, posy;
    int fs;

    //int width, height;
};

struct ass_params {
    const char *fontpath;
    struct dyna *ass_nodes, *styles;
    struct dyna *cuestyles;
};

int ass_write(struct dyna *cues, struct dyna *cstyles, const struct video_info *video_info, const char *fontpath, const char *fname);

void ass_append_box(const struct cue *c, const struct ass_cue_pos *an7pos,
        const struct cuepos_box *box, const char *color, struct ass_params *ap);

int style_to_inline_tags(const struct ass_style *style, int out_len, char out[out_len]);

void ass_node_to_style(const struct vtt_node *node, const struct ass_params *ap, struct ass_style *out);
void ass_push_style_stack(struct stack *style_stack, const struct ass_style *style);

#endif /* _VTT2ASS_ASS_H */
