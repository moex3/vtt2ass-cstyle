#include "font.h"

#include <string.h>
#include <stdbool.h>
#include <assert.h>

#include "util.h"

static bool font_did_init = false;
FT_Library ftlib = NULL;

struct font_cache {
    char *fontpath;
    FT_Face face;
};
static struct font_cache caches[32] = {0};
int caches_count = 0;

void font_init()
{
    if (font_did_init)
        font_dinit();

    int err = FT_Init_FreeType(&ftlib);
    assert(err == FT_Err_Ok);
    font_did_init = true;
}

void font_dinit()
{
    if (!font_did_init) {
        assert(false && "Calling font_dinit() without font_ini()");
        return;
    }

    for (int i = 0; i < caches_count; i++) {
        FT_Done_Face(caches[i].face);
        free(caches[i].fontpath);
    }

    caches_count = 0;
    font_did_init = false;
}

FT_Face font_get_face(const char *fontpath)
{
    int err;
    struct font_cache *cfc;

    for (int i = 0; i < caches_count; i++) {
        if (strcmp(caches[i].fontpath, fontpath) == 0) {
            return caches[i].face;
        }
    }

    assert(caches_count < ARRSIZE(caches));
    cfc = &caches[caches_count];
    err = FT_New_Face(ftlib, fontpath, 0, &cfc->face);
    if (err != FT_Err_Ok) {
        return NULL;
    }
    cfc->fontpath = strdup(fontpath);
    
    caches_count++;
    return cfc->face;
}

const char *font_get_name(FT_Face face)
{
    return FT_Get_Postscript_Name(face);
}


FT_Library font_get_lib()
{
    return ftlib;
}
