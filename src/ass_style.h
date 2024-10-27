#ifndef _VTT2ASS_ASS_STYLE_H
#define _VTT2ASS_ASS_STYLE_H
#include "dyna.h"

#include <stdbool.h>
#include <stdint.h>

struct ass_style {
    char *name; /* free */
    char *fontpath; /* free */
    float frz;
    float fsp; /* font spacing */
    float bord;
    float shad;
    bool italic : 1;
    bool bold : 1;
    bool underline : 1;

    /* not really ass style members, but it's easier to add it here */
    bool ruby_under : 1;

    uint32_t bord_color; /* in rgb */

    /* These are only used in this is used as an inline tag
     * for normal styles, all is assumed to be used */
    bool fsp_set : 1;
    bool bord_set : 1;
    bool bord_color_set : 1;
    bool italic_set : 1;
    bool bold_set : 1;
    bool underline_set : 1;

    /* set fields for extra elements */
    bool ruby_under_set : 1;
};

struct dyna *ass_styles_create();
void ass_styles_destroy(struct dyna *ds);
void ass_style_free(struct ass_style *style);

struct ass_style *ass_styles_get(const struct dyna *styles, const char *name);
struct ass_style *ass_styles_add(struct dyna *styles, const char *name);

int ass_style_rgb_to_str(uint32_t rgb, char out[12]);

#endif /* _VTT2ASS_ASS_STYLE_H */
