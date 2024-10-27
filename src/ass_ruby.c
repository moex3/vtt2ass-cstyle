#include "ass_ruby.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <sys/param.h>
#include <wchar.h>

#include "cuetext.h"
#include "util.h"
#include "textextents.h"
#include "ass.h"
#include "opts.h"

#define MAX_RUBY_IN_LINE 128

#define DEBUG_RUBYBOX 1

struct ass_ruby {
    struct text_extents extents;
    const char *rubytext; /* no free */
    bool resize; /* if ruby base is bigger */

    /* True if this ruby should be rendered under the base text */
    bool under;
    float fsp;
};

struct ass_parts {
    size_t start_off, len;
    int line;
    const char *rubytext;
    struct text_extents extents; /* including spacing, but not padding_before */
    bool resize; /* if ruby text is bigger */

    /* Padding that will affect the next part basically */
    float last_char_fsp;
    bool has_last_char_fsp;

    /* For ruby padding */
    float fsp;

    bool is_ruby;
    struct ass_ruby ruby; /* If available */

    /* Inline tags for this part of text */
    struct ass_style inline_tags;
};

struct ass_ruby_params {
    /* The raw text of the current node
     * Ruby text not included */
    char text[1024];
    int text_len;
    /* Text with inline tags */
    char tags_text[4096];
    int tags_text_len;

    /* Each part contains a text part with, or without ruby. 
     * Newlines are always cause a part split */
    struct ass_parts parts[MAX_RUBY_IN_LINE];
    int parts_count;

    /* Text extents of each individual line after justify resize */
    struct text_extents line_ext[MAX_RUBY_IN_LINE];
    int line_ext_count;

    int base_fs, ruby_fs;
    const struct ass_params *ap;
    const struct ass_cue_pos *olpos;
    struct te_obj base_te_obj;

    struct stack *style_stack;

    /* private tmp elems */
    int current_line_off;
};

static void ass_ruby_text_and_parts(const struct vtt_node *node, struct ass_ruby_params *arp)
{
    if (node->type == VNODE_TIMESTAMP)
        return;
    if (node->type == VNODE_TEXT) {
        if (node->parent && node->parent->type == VNODE_RUBY_TEXT) {
            assert(strchr(node->text, '\n') == NULL);
            assert(arp->parts_count > 0);

            struct ass_ruby *rb = &arp->parts[arp->parts_count - 1].ruby;
            struct ass_style *top = stack_top(arp->style_stack);

            rb->rubytext = node->text;
            if (top->ruby_under_set)
                rb->under = top->ruby_under;
            return;
        }

        const char *start = node->text, *end = NULL;
        for (;;) {
            /* Split lines into different parts */
            int line_len;
            assert(arp->parts_count < ARRSIZE(arp->parts));
            struct ass_parts *part = &arp->parts[arp->parts_count];

            end = strchr(start, '\n');
            if (end)
                line_len = end - start;
            else
                line_len = strlen(start);

            if (line_len == 0)
                goto next; /* skip empty text */

            arp->parts_count++;
            memset(part, 0, sizeof(*part));
            part->start_off = arp->text_len;
            part->len = line_len;
            part->is_ruby = node->parent && node->parent->type == VNODE_RUBY;
            part->line = arp->current_line_off;
            part->inline_tags = *(struct ass_style*)stack_top(arp->style_stack);
            //printf("Copied under: %d\n", part->inline_tags.ruby_under);

            arp->text_len += snprintf(arp->text + arp->text_len,
                    sizeof(arp->text) - arp->text_len, "%.*s", line_len, start);
            assert(arp->text_len < sizeof(arp->text));

next:
            if (end == NULL)
                break;
            assert(*end != '\0');
            arp->current_line_off++;
            start = end + 1;
        }

        return;
    }

    /* These nodes (not timestamp and text) could signify styles */
    struct ass_style nodestyle;
    ass_node_to_style(node, arp->ap, &nodestyle);
    ass_push_style_stack(arp->style_stack, &nodestyle);
    //printf("nodestyle ruby under: %d\n", nodestyle.ruby_under);
    //struct ass_style *tmps = (struct ass_style*)stack_top(arp->style_stack);
    //printf("stacktop ruby under: %d\n", tmps->ruby_under);

    for (int i = 0; node->childs && i < node->childs->e_idx; i++) {
        struct vtt_node *cn = dyna_elem(node->childs, i);

        ass_ruby_text_and_parts(cn, arp);
    }

    stack_pop(arp->style_stack);

#if 0
    if (node->type == VNODE_RUBY) {
        /* This node was a ruby text, mark the last elem in parts as ruby */
        assert(*parts_len > 0);
        parts[*parts_len - 1].is_ruby = true;
    }
#endif

    return;
}

#if 0
static void ass_ruby_calc_box(const struct cue *c, const struct ass_cue_pos *olpos, int rubyfs,
        int *ruby_count, struct ass_ruby rubys[MAX_RUBY_IN_LINE], struct text_extents *out_full_ext)
{
    char txt[1024];
    struct ass_parts parts[MAX_RUBY_IN_LINE];
    struct text_extents full_ext;
    struct text_extents line_exts[MAX_RUBY_IN_LINE];
    struct ass_collect_ruby_priv collpriv = { 0 };
    int parts_len = 0;
    int line_exts_len = 0;
    size_t consumed = 0;
    float x_offset_fact = 0.5f; /* def for center */
    *ruby_count = 0;

    consumed = ctxt_text(c->text_node, sizeof(txt), txt);
    assert(consumed < sizeof(txt));
    util_get_text_extents_lines(NULL, txt, olpos->fs, MAX_RUBY_IN_LINE, line_exts, &line_exts_len);
    util_combine_extents(line_exts_len, line_exts, &full_ext);

    ass_ruby_split_to_parts(c->text_node, &collpriv, MAX_RUBY_IN_LINE, parts, &parts_len);
    assert(parts_len < MAX_RUBY_IN_LINE);

    if (IS_ASS_ALIGN_RIGHT(olpos->align))
        x_offset_fact = 1.0f;
    else if (IS_ASS_ALIGN_LEFT(olpos->align))
        x_offset_fact = 0.0f;

    for (int i = 0; i < parts_len; i++) {
        struct text_extents before_ext;
        struct text_extents ex = {0};

        util_get_text_extents_line(NULL, txt, collpriv.consumed, parts[i].line_start_off, (parts[i].start_off - parts[i].line_start_off), olpos->fs, &before_ext);
        util_get_text_extents_line(NULL, txt, collpriv.consumed, parts[i].start_off, parts[i].len, olpos->fs, &ex);
        printf("Before: '%.*s'\n", (int)(parts[i].start_off - parts[i].line_start_off), txt + parts[i].line_start_off);
        printf("Ruby: '%.*s'\n", (int)(parts[i].len), txt + parts[i].start_off);
        printf("Ruby text: '%s'\n", parts[i].rubytext);

        int width_diff = (full_ext.width - line_exts[parts[i].line].width) * x_offset_fact;
        rubys[i].base_box.left = before_ext.width + width_diff;
        rubys[i].base_box.top = (full_ext.height / line_exts_len) * parts[i].line;
        rubys[i].base_box.width = ex.width;
        rubys[i].base_box.height = ex.height;
        rubys[i].rubytext = parts[i].rubytext;
        util_get_text_extents(NULL, txt, rubyfs, &rubys[i].rubytext_ext);
    }

    *ruby_count = parts_len;
    *out_full_ext = full_ext;
}
#endif

static void ass_ruby_calc_parts_extents(struct ass_ruby_params *arp)
{
    const struct ass_style *style = ass_styles_get(arp->ap->styles, "Default");
    for (int i = 0; i < arp->parts_count; i++) {
        te_get_at(&arp->base_te_obj, arp->parts[i].start_off, arp->parts[i].len, style->fsp, &arp->parts[i].extents);

        if (arp->parts[i].is_ruby) {
            te_simple(arp->ap->fontpath, arp->parts[i].ruby.rubytext, arp->ruby_fs, style->fsp, false, &arp->parts[i].ruby.extents);
        }
    }
}

static void ass_ruby_mark_resize(struct ass_ruby_params *arp)
{
    for (int i = 0; i < arp->parts_count; i++) {
        if (arp->parts[i].is_ruby == false)
            continue;

        int diff = arp->parts[i].extents.width - arp->parts[i].ruby.extents.width;
        if (diff > 0) {
            arp->parts[i].ruby.resize = true;
            //arp->parts[i].ruby.extents = arp->parts[i].extents;
        } else if (diff < 0) {
            arp->parts[i].resize = true;
            //arp->parts[i].extents = arp->parts[i].ruby.extents;
        }
    }
}

static void ass_ruby_calc_line_exts(struct ass_ruby_params *arp)
{
    int sum_w = 0, sum_h = 0, idx = 0;
    for (int i = 0; i < arp->parts_count; i++) {
        assert(idx < ARRSIZE(arp->line_ext));

        const struct text_extents *calc_ext = &arp->parts[i].extents;
        if (arp->parts[i].resize) {
            /* Use the size of the ruby text, because this will be resized */
            calc_ext = &arp->parts[i].ruby.extents;
        }

        if (i > 0 && arp->parts[i - 1].line != arp->parts[i].line) {
            memset(&arp->line_ext[idx], 0, sizeof(arp->line_ext[idx]));

            arp->line_ext[idx].width = sum_w;
            arp->line_ext[idx].height = sum_h;
            idx++;
            sum_w = sum_h = 0;
        }

        /* again, horizontal writing only */
        sum_w += calc_ext->width;
        sum_h = MAX(arp->parts[i].extents.height, sum_h);

    }

    assert(sum_w && sum_h);
    assert(idx < ARRSIZE(arp->line_ext));
    memset(&arp->line_ext[idx], 0, sizeof(arp->line_ext[idx]));

    arp->line_ext[idx].width = sum_w;
    arp->line_ext[idx].height = sum_h;
    idx++;
    arp->line_ext_count = idx;
}

/* space is - before the 1st char
 *          - between each char
 *          - after the last char
 */
static float ass_ruby_calc_justify_amount(
        int char_exts_count, const struct text_extents char_exts[char_exts_count],
        const struct text_extents *fspace)
{
    /* horiz only */
    int space_width;
    int sum_width = 0;
    for (int i = 0; i < char_exts_count; i++) {
        sum_width += char_exts[i].width;
    }

    space_width = fspace->width - sum_width;

    return space_width / (float)(char_exts_count + 1);
}

/* Calculates the space between characters to fill up the given space with \fsp tags
 * target > current
 * space is - before the 1st char (if outer is true)
 *          - between each char
 *          - after the last char (if outer is true)
 */
static float calc_fsp_amount(const char *text, int text_len, bool outer, float def_spacing, const struct text_extents *current, const struct text_extents *target)
{
    /* Not the best impl, but will do for now */
    int ccount = util_utf8_ccount(text_len, text);
    /* The widths include the fsp as well, remove that here */
    float diff = target->width - (current->width - ccount * def_spacing);
    /* Technically, if outer is false, there will be a padding on the right side still */
    if (outer)
        return diff / (ccount + 1);
    else
        return diff / (ccount);
}

static void render_rubytext(const struct cue *c, struct ass_ruby_params *arp,
        int cursor_x, int cursor_y, struct ass_parts *part,
        const struct ass_cue_pos *an7pos, const struct text_extents *full_ext)
{
    char text[1024];
    int text_len = 0;
    struct ass_style *style = ass_styles_get(arp->ap->styles, "Default");
    struct ass_node anode = {
        .layer = 4,
        .start_ms = c->time_start,
        .end_ms = c->time_end,
        .style = style,
    };

    int align = 1;
    int x_off = 0;
    int y_off = 0;
    if (part->ruby.resize)
        x_off += part->ruby.fsp;
    if (part->ruby.under) {
        align = 7;
        y_off += part->extents.height;
    }

    text_len += snprintf(text, sizeof(text),
            "{\\an%d\\pos(%d,%d)\\fs%d",
            align, an7pos->posx + cursor_x + x_off, an7pos->posy + cursor_y + y_off,
            arp->ruby_fs);
    assert(text_len < sizeof(text));

    if (part->ruby.resize) {
        text_len += snprintf(text + text_len, sizeof(text) - text_len,
                "\\fsp%g", part->ruby.fsp + 0);
        assert(text_len < sizeof(text));
    }

    text_len += snprintf(text + text_len, sizeof(text) - text_len,
            "}%s", part->ruby.rubytext);
    assert(text_len < sizeof(text));

    anode.text = strdup(text);
    dyna_append(arp->ap->ass_nodes, &anode);

#if DEBUG_RUBYBOX == 1
    if (opts_ass_debug_boxes) {
        struct ass_cue_pos boxpos = {
            .posx = an7pos->posx + cursor_x,
            .posy = an7pos->posy + cursor_y,
        };
        
        struct text_extents *adv_ext = &part->extents;
        if (part->resize)
            adv_ext = &part->ruby.extents;
        struct cuepos_box box = {
            .width = adv_ext->width,
            .height = part->extents.height,
        };
        ass_append_box(c, &boxpos, &box, "0BE57F", (struct ass_params*)arp->ap);
    }
#endif
}

#if 0
static void set_part_inline_tags(const struct cue *c, int cursor_x, int cursor_y, const struct ass_cue_pos *an7pos,
        struct ass_ruby_params *arp, int text_len, const char text[text_len],
        struct ass_parts *part, struct dyna *ass_nodes)
{
    if (part->is_ruby == false) {
        return;
        /* Nothing to do for non-ruby parts for now */
    }

    char ntext[1024];
    struct text_extents char_exts[128];
    int ntext_len = 0, char_exts_count = 0;
    int len, coff, fs, align;
    const char *cbase;
    mbstate_t mbstate = {0};
    float space;
    int space_cursor = 0;
    struct ass_style *style = ass_styles_get(arp->ap->styles, "Default");
    struct ass_node anode = {
        .layer = 4,
        .start_ms = c->time_start,
        .end_ms = c->time_end,
        .style = style,
    };

    /* Simply add the non-resized part without resizing */
    if (part->resize) {
        /* Ruby base is larger */
        //int r = asprintf(&anode.text, "{\\an1\\fs%d\\pos(%d,%d)}%s",
                //arp->ruby_fs, an7pos->posx + cursor_x, an7pos->posy + cursor_y, part->ruby.rubytext);
        //assert(r != -1);

        /* Calculate text extents so that it will fill the given space. x_off is used here */
        //te_get_at_chars_justify(&arp->base_te_obj, part->start_off, part->len, part->ruby.extents.width, ARRSIZE(char_exts), char_exts, &char_exts_count);

        //render_resized_part(c, part->len, text + part->start_off, an7pos->fs, 0x69, an7pos->posx + cursor_x, an7pos->posy + cursor_y,
                //7, char_exts_count, char_exts, ass_nodes);
        float fsp = calc_fsp_amount(text + part->start_off, part->len, &part->extents, &part->ruby.extents);
        fsp += style->fsp; /* This spacing is in addition to the one definited by style */
        part->inline_tags.fsp = fsp;
        part->inline_tags.fsp_set = true;

    } else if (part->ruby.resize) {
        /* Ruby text is larger */
        //int r = asprintf(&anode.text, "{\\an7\\fs%d\\pos(%d,%d)}%.*s",
                //an7pos->fs, an7pos->posx + cursor_x, an7pos->posy + cursor_y, (int)part->len, text + part->start_off);
        //assert(r != -1);

        size_t len = strlen(part->ruby.rubytext);

        /* Calculate text extents so that it will fill the given space. x_off is used here */
        //te_simple_justify_chars(arp->ap->fontpath, part->ruby.rubytext, arp->ruby_fs, part->extents.width, ARRSIZE(char_exts), char_exts, &char_exts_count);

        //render_resized_part(c, len, part->ruby.rubytext, arp->ruby_fs, 0x69, an7pos->posx + cursor_x, an7pos->posy + cursor_y,
                //1, char_exts_count, char_exts, ass_nodes);

        float fsp = calc_fsp_amount(part->ruby.rubytext, len, &part->ruby.extents, &part->extents);
        fsp += style->fsp; /* This spacing is in addition to the one definited by style */
        part->ruby.inline_tags.fsp = fsp;
        part->ruby.inline_tags.fsp_set = true;
    } else {
        //assert(false && "None of the parts are resized????");
    }
    //dyna_append(ass_nodes, &anode);

}
#endif

static void ass_ruby_render_parts(const struct cue *c, struct ass_ruby_params *arp)
{
    struct ass_style *style = ass_styles_get(arp->ap->styles, "Default");
    struct ass_node anode = {
        .layer = 4,
        .start_ms = c->time_start,
        .end_ms = c->time_end,
        .style = style,
    };
    struct text_extents full_ext = {0};
    struct ass_cue_pos an7pos = {0};
    bool space_1st = false;
    char text[1024 * 4];
    int text_len = 0;
    int line_height = arp->olpos->fs;

    /* Calculate the larger bounding box from the bounding box of each line */
    util_combine_extents(arp->line_ext_count, arp->line_ext, &full_ext);

    /* Convert \anx position to \an7 pos */
    util_cue_pos_to_an7(arp->olpos, &full_ext, &an7pos);
    //an7pos.posy -= olpos->fs * 2;

#if DEBUG_RUBYBOX == 1
    if (opts_ass_debug_boxes) {
        struct cuepos_box box = {
            .width = full_ext.width,
            .height = full_ext.height,
        };
        ass_append_box(c, &an7pos, &box, "FC19DA", (struct ass_params*)arp->ap);
    }
#endif

    /* Set tags for the rest of the base line*/
    text_len += sprintf(text, "{\\an%d\\pos(%d,%d)\\fs%d}",
            arp->olpos->align, arp->olpos->posx, arp->olpos->posy, arp->olpos->fs);

    float x_offset_fact = 0.5f;
    if (IS_ASS_ALIGN_RIGHT(arp->olpos->align))
        x_offset_fact = 1.0f;
    else if (IS_ASS_ALIGN_LEFT(arp->olpos->align))
        x_offset_fact = 0.0f;

    int cursor_x = 0, cursor_y = 0;
    /* Render base text */
    for (int i = 0; i < arp->parts_count; i++) {
        struct ass_parts *part = &arp->parts[i];

        if (i > 0 && arp->parts[i - 1].line != part->line) {
            /* Line switch, append a \N */
            text_len += snprintf(text + text_len, sizeof(text) - text_len,
                    "%s", "\\N");
            assert(text_len < sizeof(text));
            cursor_x = 0;
            cursor_y += arp->line_ext[arp->parts[i - 1].line].height;
        }

        int last_char_off = part->len;
        if (part->has_last_char_fsp) {
            const char *str = arp->text + part->start_off;
            for (int i = part->len - 1; i >= 0; i--) {
                if (util_is_utf8_start(str[i])) {
                    last_char_off = i;
                    break;
                }
            }
        }

        float fsp = style->fsp;
        if (part->resize) {
            fsp = part->fsp;
        }

        /* Write inline tags for this part */
        text_len += style_to_inline_tags(&part->inline_tags, sizeof(text) - text_len, text + text_len);
        assert(text_len < sizeof(text));

        /* Render fsp align + actual text */
        const char *ps_start = arp->text + part->start_off;
        text_len += snprintf(text + text_len, sizeof(text) - text_len,
                "{\\fsp%g}%.*s", fsp, last_char_off, arp->text + part->start_off);
        assert(text_len < sizeof(text));

        if (part->has_last_char_fsp) {
            text_len += snprintf(text + text_len, sizeof(text) - text_len,
                    "{\\fsp%g}%.*s", part->last_char_fsp, (int)(part->len - last_char_off), arp->text + part->start_off + last_char_off);
            assert(text_len < sizeof(text));
        }

        if (part->is_ruby) {
            int line_diff = full_ext.width - arp->line_ext[part->line].width;
            /* Render ruby text */
            render_rubytext(c, arp, cursor_x + line_diff * x_offset_fact, cursor_y, part, &an7pos, &full_ext);
        }

        //if (part->has_last_char_fsp)
            //cursor_x -= part->last_char_fsp;

        if (part->resize)
            cursor_x += part->ruby.extents.width;
        else
            cursor_x += part->extents.width;
    }
    anode.text = strdup(text);
    dyna_append(arp->ap->ass_nodes, &anode);

}

#if 0
static void split_part_on_last_char(struct ass_ruby_params *arp, int part_idx)
{
    struct ass_parts *part = &arp->parts[part_idx];

    assert(part->len > 1);
    const char *str = arp->text + part->start_off;

    int split_off = -1;
    for (int i = arp->len - 1; i >= 0; i--) {
        if (util_is_utf8_start(str[i])) {
            /* Last UTF8 character position, split here */
            split_off = i;
            break;
        }
    }
    assert(split_off != -1);

    struct ass_parts newp = *part;

    for (int j = arp->parts[i].len - 1; j >= 0; j--) {
        char ch = arp->text[arp->parts[i].start_off + j];
        if ((ch & 0xC0) != 0x80) {
            /* This is the starting char, mark it */
            last_char_len = arp->parts[i].len - j;
            break;
        }
    }
}
#endif

static void ass_ruby_align_resized(struct ass_ruby_params *arp)
{
    struct ass_style *style = ass_styles_get(arp->ap->styles, "Default");
    for (int i = 0; i < arp->parts_count; i++) {
        struct ass_parts *part = &arp->parts[i];

        if (part->resize) {
            /* Only do this if this is not the 1st part */
            bool outer_fsp = i > 0;
            float fsp = calc_fsp_amount(arp->text + part->start_off, part->len, outer_fsp,
                    style->fsp, &part->extents, &part->ruby.extents);
            part->fsp = fsp;
            if (outer_fsp) {
                float def_fsp = style->fsp;
                if (arp->parts[i - 1].resize)
                    def_fsp = arp->parts[i - 1].fsp;
                arp->parts[i - 1].last_char_fsp = def_fsp + fsp;
                arp->parts[i - 1].has_last_char_fsp = true;
            }
        } else if (part->ruby.resize) {
            /* On ruby texts, we can always do outer spacing, because that's just a position addition */
            float fsp = calc_fsp_amount(part->ruby.rubytext, strlen(part->ruby.rubytext), true,
                    style->fsp, &part->ruby.extents, &part->extents);
            part->ruby.fsp = fsp;
        }
    }
}

#if 0
// TODO: this is wrong, should apply each inline tag separatly in redner_parts 
static void fill_tags_text(const struct cue *c, struct ass_ruby_params *arp)
{
    struct ass_style *style = ass_styles_get(arp->ap->styles, "Default");

    stack_init(style_stack, sizeof(struct ass_style), 30);
    stack_push(&style_stack, style);

    arp->tags_text_len = ass_text_collect_tags_and_escape(c->text_node, sizeof(arp->tags_text), arp->tags_text, &style_stack, NULL, arp->ap);
    assert(arp->tags_text_len < sizeof(arp->tags_text));
}
#endif

void ass_ruby_write(const struct cue *c, const struct ass_cue_pos *olpos, const struct ass_params *ap)
{
    struct ass_style *style = ass_styles_get(ap->styles, "Default");

    stack_init(style_stack, sizeof(struct ass_style), 30);
    stack_push(&style_stack, style);
    /* Struct to hold all info about the ruby rendering process,
     * because otherwise there are too many arguments to functions lol */
    struct ass_ruby_params arp = {
        .base_fs = olpos->fs,
        .ruby_fs = (int)(olpos->fs * 0.55f),
        .olpos = olpos,
        .ap = ap,
        .style_stack = &style_stack,
    };

    /* Copy the full raw text, and split the text into parts based on ruby and newlines
     * Fills: arp.text, arp.parts */
    ass_ruby_text_and_parts(c->text_node, &arp);

    /* Setup object for calculating text extents later */
    te_create_obj(ap->fontpath, arp.text, arp.text_len, arp.base_fs, false, &arp.base_te_obj);

    /* Calculate text extents for each part, as they are, without any resizing
     * Fills: arp.parts and .ruby .extents */
    ass_ruby_calc_parts_extents(&arp);

#if 1
    for (int i = 0; i < arp.parts_count; i++) {
        printf("Part: [%d%s] '%.*s'\n", arp.parts[i].line, (char*[]){"       ", " - RUBY"}[arp.parts[i].is_ruby], (int)arp.parts[i].len, arp.text + arp.parts[i].start_off);
        if (arp.parts[i].is_ruby) {
            printf("  ^\\- Ruby text: %s\n", arp.parts[i].ruby.rubytext);
        }
    }
    printf("\n");
#endif

    /* Either mark the ruby base, or the ruby text as the one to be resized
     * depending on whichever is larger
     * Fills: arp.parts and .ruby .resize */
    ass_ruby_mark_resize(&arp);

    /* Calculate text extents for each line, with resized sizes
     * Fills arp.line_ext */
    ass_ruby_calc_line_exts(&arp);

    /* Center the resized parts
     * Fills arp.part and .ruby .fsp and last_char_fsp */
    ass_ruby_align_resized(&arp);

    /* Render base and ruby text */
    ass_ruby_render_parts(c, &arp);

#if 0
    text_len = ass_draw_box(sizeof(text), text, &an7pos, &rb_box, "D63E73");
    assert(text_len < sizeof(text));
    anode.text = strdup(text);
    dyna_append(ass_nodes, &anode);
#endif

#if 0
    for (int i = 0; i < ruby_count; i++) {
        text_len = snprintf(text, sizeof(text), "{\\an2\\fs%d\\pos(%d,%d)}%s", (int)(olpos->fs * 0.55f),
                an7pos.posx + rubys[i].base_box.left + rubys[i].base_box.width / 2, an7pos.posy + rubys[i].base_box.top, rubys[i].rubytext);
        assert(text_len < sizeof(text));
        anode.text = strdup(text);
        dyna_append(ass_nodes, &anode);
    }
#endif
    
    te_destroy_obj(&arp.base_te_obj);
}

