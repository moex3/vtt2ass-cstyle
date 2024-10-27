#include "parser.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdbool.h>

#include "tokenizer.h"
#include "cuestyle.h"

#define PARSER_LEAN 1

const char *str_cue_writing_direction[] = {
    [WD_HORIZONTAL] = "horizontal",
    [WD_VERTICAL_GROW_LEFT] = "vertical grow left",
    [WD_VERTICAL_GROW_RIGHT] = "vertical grow right",
};
const char *str_cue_line_align[] = {
    [LINE_ALIGN_START] = "start",
    [LINE_ALIGN_CENTER] = "center",
    [LINE_ALIGN_END] = "end",
};
const char *str_cue_pos_align[] = {
    [POS_ALIGN_AUTO] = "auto",
    [POS_ALIGN_LINE_LEFT] = "line-left",
    [POS_ALIGN_CENTER] = "center",
    [POS_ALIGN_LINE_RIGHT] = "line-right",
};
const char *str_cue_text_align[] = {
    [TEXT_ALIGN_CENTER] = "center",
    [TEXT_ALIGN_START] = "start",
    [TEXT_ALIGN_END] = "end",
    [TEXT_ALIGN_LEFT] = "left",
    [TEXT_ALIGN_RIGHT] = "right",
};

static enum cue_line_align prs_cue_line_align(const char *str)
{
    if (strcmp(str, "start") == 0)
        return LINE_ALIGN_START;
    if (strcmp(str, "center") == 0)
        return LINE_ALIGN_CENTER;
    if (strcmp(str, "end") == 0)
        return LINE_ALIGN_END;
    return -1;
}


static void prs_cue_free(void *data)
{
    struct cue *cue = data;
    if (cue->ident) {
        free(cue->ident);
    }
    if (cue->text_node)
        ctxt_free_node(cue->text_node);
}

static void prs_default_cue(struct cue *cue)
{
    memset(cue, 0, sizeof(*cue));
    cue->line = cue->position = CUE_AUTO;
    cue->size = 1;
    cue->snap_to_lines = true;
    //cue->line_align = LINE_ALIGN_END; // mine
    cue->line_align = LINE_ALIGN_START;
}
#define ADVANCE() { \
    i++; \
    if (i >= tokens->e_idx) goto err; \
    tok = dyna_elem(tokens, i); \
}
#define EXP(exp_token_type) if (tok->type != exp_token_type) { \
    fprintf(stderr, "Unexpected token: %s  expected %s\n", tok_type2str(tok->type), tok_type2str(exp_token_type)); \
    goto err; \
}

/* Returns NAN on error */
static float prs_percentage(const char *str)
{
    char *end;
    errno = 0;
    float f = strtof(str, &end);
    if (errno != 0 || end == str)
        return NAN;
    if (*end != '%')
        return NAN;
    if (*(end + 1) != '\0')
        return NAN;
    return f;
}

static int prs_cue_settings_line(struct token *tok, struct cue *cue)
{
    char *val = tok->cue_setting.value;
    char *sep = strchr(val, ',');

    if (sep) {
        *sep = '\0';
        enum cue_line_align line_align = prs_cue_line_align(sep + 1);
        if (line_align == -1)
            return -1;
        cue->line_align = line_align;
    }

    float lineval;
    if (val[strlen(val) - 1] == '%') {
        cue->snap_to_lines = false;
        lineval = prs_percentage(val);
        if (isnan(lineval)) {
            char *fend;
            errno = 0;
            lineval = strtof(val, &fend);
            if (errno != 0 || *fend != '\0')
                return -1;
        } else {
            cue->line = lineval / 100.0f;
        }
    }

    if (sep)
        *sep = ',';
    return 0;
}

/* Return -1 on error */
static enum cue_pos_align prs_cue_pos_align(const char *str)
{
    /* This is kind of weird, the spec says that this cannot be "middle"
     * but the example vtt i have have this value as middle... */
    if (strcmp(str, "line-left") == 0)
        return POS_ALIGN_LINE_LEFT;
    if (strcmp(str, "center") == 0
#if PARSER_LEAN == 1
            || strcmp(str, "middle") == 0
#endif
       )
        return POS_ALIGN_CENTER;
    if (strcmp(str, "line-right") == 0)
        return POS_ALIGN_LINE_RIGHT;
    return -1;
}

static int prs_cue_settings_position(struct token *tok, struct cue *cue)
{
    char *colpos = tok->cue_setting.value;
    char *sep = strchr(colpos, ',');
    enum cue_pos_align align = POS_ALIGN_AUTO;
    if (sep) {
        *sep = '\0';
        char *colalign = sep + 1;
        align = prs_cue_pos_align(colalign);
        if (align == -1)
            return -1;
            //align = POS_ALIGN_AUTO; /* uuuuh not in spec */
    }

    float pos = prs_percentage(colpos);
    if (pos == NAN)
        return -1;

    if (sep)
        *sep = ',';
    cue->position = pos / 100.0f;
    cue->pos_align = align;
    return 0;
}

static int prs_cue_settings_size(struct token *tok, struct cue *cue)
{
    char *val = tok->cue_setting.value;
    float size = prs_percentage(val);
    if (size == NAN)
        return -1;
    /* TODO: invalidate region here, when we decide to support that */
    cue->size = size / 100.0f;
    return 0;
}

static int prs_cue_settings_align(struct token *tok, struct cue *cue)
{
    char *val = tok->cue_setting.value;

    if (strcmp(val, "start") == 0) {
        cue->text_align = TEXT_ALIGN_START;
    } else if (strcmp(val, "center") == 0
#if PARSER_LEAN == 1
            || strcmp(val, "middle") == 0
#endif
            ) {
        cue->text_align = TEXT_ALIGN_CENTER;
    } else if (strcmp(val, "end") == 0) {
        cue->text_align = TEXT_ALIGN_END;
    } else if (strcmp(val, "left") == 0) {
        cue->text_align = TEXT_ALIGN_LEFT;
    } else if (strcmp(val, "right") == 0) {
        cue->text_align = TEXT_ALIGN_RIGHT;
    } else {
        return -1;
    }

    return 0;
}

static int prs_cue_settings_vertical(struct token *tok, struct cue *cue)
{
    char *val = tok->cue_setting.value;

    if (strcmp(val, "rl") == 0) {
        cue->writing_direction = WD_VERTICAL_GROW_LEFT;
    } else if (strcmp(val, "lr") == 0) {
        cue->writing_direction = WD_VERTICAL_GROW_RIGHT;
    } else {
        return -1;
    }

    /* TODO: set region here to NULL */
    return 0;
}

/* Returns an updated i */
static int prs_cue_settings(struct dyna *tokens, int i, struct cue *cue)
{
    int en;
    //struct token *tok = dyna_elem(tokens, i);
    struct token *tok;
    for (; i < tokens->e_idx && (tok = dyna_elem(tokens, i))->type == TOK_CUE_SETTING; i++) {
        char *skey = tok->cue_setting.key;

        if (strcmp(skey, "vertical") == 0) {
            en = prs_cue_settings_vertical(tok, cue);
        } else if (strcmp(skey, "line") == 0) {
            en = prs_cue_settings_line(tok, cue);
        } else if (strcmp(skey, "position") == 0) {
            en = prs_cue_settings_position(tok, cue);
        } else if (strcmp(skey, "size") == 0) {
            en = prs_cue_settings_size(tok, cue);
        } else if (strcmp(skey, "align") == 0) {
            en = prs_cue_settings_align(tok, cue);
        } else {
            printf("cue setting key '%s' not handled!\n", skey);
            return -1;
        }
        if (en != 0) {
            /* We should skip invalid settings */
            fprintf(stderr, "Failed to parse setting with key '%s', skipping\n", skey);
            continue;
        }
    }
    return i;
}

static int prs_parse_cue_text(struct dyna *tokens, int i, struct cue *cue)
{
    struct token *tok;
    int oi = i;
    size_t len = 0;
    for (; i < tokens->e_idx && (tok = dyna_elem(tokens, i))->type == TOK_CUE_TEXT; i++) {
        len += strlen(tok->cue_text.str);
        len += 1; /* for \n and \0 */
    }
    char full_txt[len];
    char *ptr = full_txt;
    *ptr = '\0';
    i = oi;
    for (; i < tokens->e_idx && (tok = dyna_elem(tokens, i))->type == TOK_CUE_TEXT; i++) {
        /* no care about performance righ now */
        ptr = stpcpy(ptr, tok->cue_text.str);
        if (ptr - full_txt < len - 1) {
            *ptr = '\n';
            ptr++;
            *ptr = '\0';
        }
    }

    cue->text_node = ctxt_parse(full_txt);
    return i;
}

int prs_parse_tokens(struct dyna *tokens, struct dyna **out_cues, struct dyna **out_styles)
{
    struct dyna *cues = dyna_create(sizeof(struct cue));
    dyna_set_free_fn(cues, prs_cue_free);

    bool in_cue = false;
    struct cue cc;
    for (int i = 0; i < tokens->e_idx;) {
        struct token *tok = dyna_elem(tokens, i);
        if (tok->type != TOK_TIMESTAMP && tok->type != TOK_IDENT) {
            i++;
            continue;
        }
        if (in_cue) {
            dyna_append(cues, &cc);
            in_cue = false;
        }
        in_cue = true;
        prs_default_cue(&cc);

        if (tok->type == TOK_IDENT) {
            //cc.ident = strdup(tok->ident.str);
            /* Move the string into the cue */
            cc.ident = tok->ident.str;
            tok->ident.str = NULL;
            ADVANCE(); EXP(TOK_TIMESTAMP);
        }
        
        cc.time_start = tok->timestamp.ms;
        ADVANCE(); EXP(TOK_ARROW);
        ADVANCE(); EXP(TOK_TIMESTAMP);
        cc.time_end = tok->timestamp.ms;

        ADVANCE();
        if (tok->type == TOK_CUE_SETTING) {
            int curr_i = prs_cue_settings(tokens, i, &cc);
            if (curr_i == -1)
                goto err;
            i = curr_i;
            tok = dyna_elem(tokens, i);
        }

        EXP(TOK_CUE_TEXT);
        int consumed = prs_parse_cue_text(tokens, i, &cc);
        if (consumed == -1)
            goto err;
        i = consumed;
    }
    if (in_cue)
        dyna_append(cues, &cc);


    *out_styles = cuestyle_parse(tokens);
    *out_cues = cues;
    return 0;
err:
    fprintf(stderr, "Exiting from parse_tokens\n");
    dyna_destroy(cues);
    return -1;

}

#undef EXP
#undef ADVANCE

void prs_cue2str(int size, char out_str[size], const struct cue *cue)
{
    char linestr[20], posstr[20];
    char nodestr[1024] = { [0] = '\0' };
    if (IS_AUTO(cue->line))
        sprintf(linestr, "%s", "auto");
    else
        sprintf(linestr, "%.4f", cue->line * 100);
    if (IS_AUTO(cue->position))
        sprintf(posstr, "%s", "auto");
    else
        sprintf(posstr, "%.4f", cue->position * 100);

    if (cue->text_node)
        ctxt_print_node(cue->text_node, sizeof(nodestr), nodestr);

    snprintf(out_str, size, "ident: %s\n"
            "%ld --> %ld\n"
            "line: %s line_align: %s\n"
            "pos: %s pos_align: %s\n"
            "size: %.4f text_align: %s wr_dir: %s\n"
            "text: '%s'", cue->ident, cue->time_start, cue->time_end,
            linestr, str_cue_line_align[cue->line_align],
            posstr, str_cue_pos_align[cue->pos_align],
            cue->size * 100.0f, str_cue_text_align[cue->text_align], str_cue_writing_direction[cue->writing_direction],
            nodestr);
}
