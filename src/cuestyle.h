#ifndef _VTT2ASS_CUESTYLE_H
#define _VTT2ASS_CUESTYLE_H
#include <stdbool.h>

#include "dyna.h"

struct cue_style {
    char *selector; /* free */

    enum {
        RUBYPOS_UNSET = 0,
        RUBYPOS_OVER,
        RUBYPOS_UNDER,
    } ruby_position;

    struct {
        int xpos, ypos;
        char color[12];
    } text_shadow[4];
    int text_shadow_count;

    bool italic : 1;
};

struct dyna *cuestyle_parse(struct dyna *tokens);
const struct cue_style *cuestyle_get_by_selector(const struct dyna *styles, const char *selector);
int cuestyle_print(int o_text_size, char o_text[o_text_size], struct cue_style *cs);

#endif /* _VTT2ASS_CUESTYLE_H */
