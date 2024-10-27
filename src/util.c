#include "util.h"

#include <stdlib.h>

#include <math.h>
#include <string.h>
#include <assert.h>
#include <hb.h>

#include "font.h"

void deref_free(void *arg)
{
    free(*(char**)arg);
}

static void util_count_node_lines_inner(const struct vtt_node *node, int *lc)
{
    if (node->type == VNODE_TEXT) {
        const char *pos = node->text;
        while ((pos = strchr(pos, '\n'))) {
            (*lc)++;
            pos++;
        }
        return;
    }
    if (node->type == VNODE_TIMESTAMP)
        return;

    if (node->childs == NULL)
        return;
    for (int i = 0; i < node->childs->e_idx; i++)
        util_count_node_lines_inner(dyna_elem(node->childs, i), lc);
}

int util_count_node_lines(const struct vtt_node *root)
{
    int lc = 1;
    util_count_node_lines_inner(root, &lc);
    return lc;
}


void util_combine_extents(int ex_len, const struct text_extents ex[ex_len], struct text_extents *out)
{
    memset(out, 0, sizeof(*out));
    if (ex_len < 1) {
        assert(ex_len > 0);
        return;
    }

    out->width = ex[0].width;
    out->height = ex[0].height;
    for (int i = 1; i < ex_len; i++) {
        if (ex[i].width > out->width)
            out->width = ex[i].width;
        out->height += ex[i].height;
    }
}

void util_init()
{

} 

int util_utf8_ccount(int s_len, const char s[s_len])
{
  int count = 0;
  for (int i = 0; i < s_len; i++) {
    if ((s[i] & 0xC0) != 0x80)
        count++;
  }

  return count;
}

void util_cue_pos_to_an7(const struct ass_cue_pos *pos, const struct text_extents *ext, struct ass_cue_pos *an7_pos)
{
    float x_sub_fact = 0.5f;
    if (IS_ASS_ALIGN_RIGHT(pos->align)) {
        x_sub_fact = 1;
    } else if (IS_ASS_ALIGN_LEFT(pos->align)) {
        x_sub_fact = 0;
    }

    float y_sub_fact = 0.5f;
    if (IS_ASS_ALIGN_BOTTOM(pos->align)) {
        y_sub_fact = 1;
    } else if (IS_ASS_ALIGN_TOP(pos->align)) {
        y_sub_fact = 0;
    }

    an7_pos->align = 7;
    an7_pos->logical_align = an7_pos->align;
    an7_pos->fs = pos->fs;
    an7_pos->posx = pos->posx - x_sub_fact * ext->width;
    an7_pos->posy = pos->posy - y_sub_fact * ext->height;
}

bool util_is_utf8_start(char chr)
{
    return ((chr & 0xC0) != 0x80);
}

uint32_t util_colorname_to_rgb(const char *name)
{
    if (strcmp(name, "black") == 0)
        return 0x00000000;

    return 0x00000000;
}
