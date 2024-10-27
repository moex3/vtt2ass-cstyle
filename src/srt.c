#include "srt.h"

#include "cuetext.h"
#include <stdio.h>
#include <string.h>

#include "util.h"

static void srt_ms_to_str(int64_t tms, int n, char out[n])
{
    int h, m, s, ms;
    h = tms / H_IN_MS;
    tms %= H_IN_MS;

    m = tms / M_IN_MS;
    tms %= M_IN_MS;

    s = tms / S_IN_MS;
    ms = tms % S_IN_MS;

    snprintf(out, n, "%02d:%02d:%02d,%03d", h, m, s, ms);
}

static void srt_write_timestamp(FILE *f, struct cue *c)
{
    char tsb[32];

    srt_ms_to_str(c->time_start, sizeof(tsb), tsb);
    fprintf(f, "%s --> ", tsb);

    srt_ms_to_str(c->time_end, sizeof(tsb), tsb);
    fprintf(f, "%s\n", tsb);
}

enum tag_position {
    TAG_START = 0,
    TAG_END,
};

static void handle_position_tags(FILE *f, const struct cue *c, enum tag_position tpos)
{
    /* Only handle left to right horizontal text for now */
    
    static const int alignmap[3][3] = {
        { 7, 8, 9 },
        { 4, 5, 6 },
        { 1, 2, 3 },
    };
    int xal = 1, yal = 2;
    char alignstr[16];

    if (tpos == TAG_END)
        return; // This dosn't need an ending tag

    switch (c->text_align) {
        case TEXT_ALIGN_START:
        case TEXT_ALIGN_LEFT:
            xal = 0;
            break;
        case TEXT_ALIGN_END:
        case TEXT_ALIGN_RIGHT:
            xal = 2;
            break;
    }
#if 0
    switch (c->line_align) {
        case LINE_ALIGN_START:
            yal = 0;
            break;
        case LINE_ALIGN_CENTER:
            yal = 1;
            break;
    }
#endif

    if (xal == 1 && yal == 2)
        return; // default

    sprintf(alignstr, "{\\an%d}", alignmap[yal][xal]);
    fputs(alignstr, f);
}

static const char *tag_map[][2] = {
    [VNODE_RUBY_TEXT][0] = "(",
    [VNODE_RUBY_TEXT][1] = ")",
    [VNODE_ITALIC][0] = "<i>",
    [VNODE_ITALIC][1] = "</i>",
    [VNODE_BOLD][0] = "<b>",
    [VNODE_BOLD][1] = "</b>",
    [VNODE_UNDERLINE][0] = "<u>",
    [VNODE_UNDERLINE][1] = "</u>",
};

static void srt_write_tag(FILE *f, const struct cue *c, struct vtt_node *node, const struct dyna *cstyles, enum tag_position pos)
{
    enum vtt_node_type type = node->type;

    if (type == VNODE_ROOT) {
        handle_position_tags(f, c, pos);
        return;
    }

    if (type == VNODE_CLASS) {
        // NOTE: only 1 class name is handled here, and we should do it another way anyway
        if (node->class_names) {
            const char *classname = *(char**)dyna_elem(node->class_names, 0);
            char cname_with_cue[128];

            snprintf(cname_with_cue, sizeof(cname_with_cue), "::cue(.%s)", classname);
            //printf("Searching for class name '%s'\n", cname_with_cue);
            const struct cue_style *cs = cuestyle_get_by_selector(cstyles, cname_with_cue);
            if (cs) {
                if (cs->italic)
                    type = VNODE_ITALIC;
            }
        }
    }

    switch (type) {
    case VNODE_RUBY_TEXT:
    case VNODE_ITALIC:
    case VNODE_BOLD:
    case VNODE_UNDERLINE:
        fputs(tag_map[type][pos], f);
        break;
    }
}

static void srt_write_text(FILE *f, const struct cue *c, struct vtt_node *node, const struct dyna *cstyles)
{
    //assert(node->type == VNODE_ROOT);

    /* These two cannot have childrens */
    if (node->type == VNODE_TEXT) {
        fputs(node->text, f);
        return;
    }
    if (node->type == VNODE_TIMESTAMP)
        return;

    srt_write_tag(f, c, node, cstyles, TAG_START);

    for (int i = 0; node->childs && i < node->childs->e_idx; i++) {
        struct vtt_node *cn = dyna_elem(node->childs, i);

        srt_write_text(f, c, cn, cstyles);

#if 0
        else if (tok->type == TTOK_TAG_START && strcmp(tok->ttok_tag_start.tag_name, "rt") == 0)
            fputc('(', f);
        else if (tok->type == TTOK_TAG_END && strcmp(tok->ttok_tag_start.tag_name, "rt") == 0)
            fputc(')', f);
#endif

    }

    srt_write_tag(f, c, node, cstyles, TAG_END);
}

int srt_write(struct dyna *cues, struct dyna *cstyles, const char *fname)
{
    FILE *f = fopen(fname, "w");
    if (f == NULL)
        return -1;

    for (int i = 0; i < cues->e_idx; i++) {
        struct cue *c = dyna_elem(cues, i);
        if (c->text_node == NULL)
            continue;

        fprintf(f, "%d\n", i + 1);
        srt_write_timestamp(f, c);

        srt_write_text(f, c, c->text_node, cstyles);
        fputc('\n', f);
        fputc('\n', f);
    }

    fclose(f);
    return 0;
}
