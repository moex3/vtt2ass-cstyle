#include "ass_style.h"

#include <string.h>
#include <stdlib.h>
#include <byteswap.h>
#include <stdio.h>

#include "util.h"

void ass_style_free(struct ass_style *s)
{
    SAFE_FREE(s->name);
    SAFE_FREE(s->fontpath);
    //memset(style, 0, sizeof(*style));
}

struct dyna *ass_styles_create()
{
    struct dyna *styles = dyna_create_size_flags(sizeof(struct ass_style), 4, DYNAFLAG_HEAPCOPY);
    dyna_set_free_fn(styles, (dyna_free_fn)ass_style_free);
    return styles;
}

void ass_styles_destroy(struct dyna *ds)
{
    dyna_destroy(ds);
}

struct ass_style *ass_styles_get(const struct dyna *styles, const char *name)
{
    for (int i = 0; i < styles->e_idx; i++) {
        struct ass_style *s = dyna_elem(styles, i);
        if (strcmp(s->name, name) == 0)
            return s;
    }
    return NULL;
}

struct ass_style *ass_styles_add(struct dyna *styles, const char *name)
{
    struct ass_style *s = dyna_emplace(styles);
    memset(s, 0, sizeof(*s));
    s->name = strdup(name);
    return s;
}

int ass_style_rgb_to_str(uint32_t rgb, char out[12])
{
    uint32_t bgr = bswap_32(rgb);
    return snprintf(out, 12, "&H%08X", bgr);
}
