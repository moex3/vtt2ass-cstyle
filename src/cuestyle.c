#include "cuestyle.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <regex.h>
#include <assert.h>

#include "tokenizer.h"
#include "util.h"

static void cuestyle_free(void *data)
{
    struct cue_style *cs = data;
    SAFE_FREE(cs->selector);
}

static void parse_text_shadow(const char *val, struct cue_style *cs)
{
    regex_t reg;
    regmatch_t regm[12];
    int xpos, ypos;
    char color[24];
    int consumed;
    int r, shad_count;
    size_t val_off = 0;

    r = regcomp(&reg, "(-?[[:digit:]]+)(px)? (-?[[:digit:]]+)(px)? ([^,]+),?", REG_EXTENDED);
    assert(r == 0);
    assert(reg.re_nsub < ARRSIZE(regm));

    for (shad_count = 0; shad_count < ARRSIZE(cs->text_shadow); shad_count++) {
        r = regexec(&reg, val + val_off, reg.re_nsub + 1, regm, 0);
        assert(r == 0);

        cs->text_shadow[shad_count].xpos = strtol(val + val_off + regm[1].rm_so, NULL, 10);
        cs->text_shadow[shad_count].ypos = strtol(val + val_off + regm[3].rm_so, NULL, 10);
        int colorlen = regm[5].rm_eo - regm[5].rm_so;
        assert(colorlen < sizeof(cs->text_shadow[0].color));
        memcpy(cs->text_shadow[shad_count].color, val + val_off + regm[5].rm_so, colorlen);
        cs->text_shadow[shad_count].color[colorlen] = '\0';

        val_off += regm[0].rm_eo;
    }

    cs->text_shadow_count = shad_count;

    regfree(&reg);
}

static void parse_keyval(struct token *tok, struct cue_style *cs)
{
    const char *key = tok->style_keyval.key;
    const char *val = tok->style_keyval.value;

    if (strcmp(key, "ruby-position") == 0) {
        if (strcmp(val, "under") == 0) {
            cs->ruby_position = RUBYPOS_UNDER;
        }
    } else if (strcmp(key, "x-ttml-shear") == 0) {
        cs->italic = true;
    } else if (strcmp(key, "text-shadow") == 0) {
        parse_text_shadow(val, cs);
    }
}

static int parse_group(struct dyna *tokens, int tok_idx, struct dyna *styles)
{
#define ADVANCE() { \
    i++; \
    if (i >= tokens->e_idx) goto err; \
    tok = dyna_elem(tokens, i); \
}
#define EXP(exp_token_type) if (tok->type != exp_token_type) { \
    fprintf(stderr, "Unexpected token: %s  expected %s\n", tok_type2str(tok->type), tok_type2str(exp_token_type)); \
    goto err; \
}

    int i = tok_idx;
    struct token *tok = dyna_elem(tokens, i);
    struct cue_style cs = {0};

    EXP(TOK_STYLE_SELECTOR);
    /* Move string instead of copy */
    cs.selector = tok->style_selector.str;
    tok->style_selector.str = NULL;

    ADVANCE(); EXP(TOK_STYLE_OPEN_BRACE);

    ADVANCE();
    while (tok->type == TOK_STYLE_KEYVAL) {
        parse_keyval(tok, &cs);
        ADVANCE();
    }
    EXP(TOK_STYLE_CLOSE_BRACE);

    dyna_append(styles, &cs);

err:
    return i;
#undef ADVANCE
#undef EXP
}

struct dyna *cuestyle_parse(struct dyna *tokens)
{
    struct dyna *styles = dyna_create_size(sizeof(struct cue_style), 4);
    dyna_set_free_fn(styles, cuestyle_free);

    for (int i = 0; i < tokens->e_idx; i++) {
        struct token *tok = dyna_elem(tokens, i);

        if (tok->type == TOK_STYLE_SELECTOR) {
            i = parse_group(tokens, i, styles);
            continue;
        }

        if (tok->type == TOK_TIMESTAMP) {
            /* According to the spec, style tags are only valid before
             * the first cue, so stop parsing there */
            break;
        }
    }

    if (styles->e_idx == 0) {
        /* No styles definied, return NULL */
        dyna_destroy(styles);
        styles = NULL;
    }

    return styles;
}

int cuestyle_print(int o_text_size, char o_text[o_text_size], struct cue_style *cs)
{
    int r = snprintf(o_text, o_text_size,
            "Selector: %s\nruby-position: %d\nitalic: %d\n",
            cs->selector, cs->ruby_position, cs->italic);

    return r;
}

const struct cue_style *cuestyle_get_by_selector(const struct dyna *styles, const char *selector)
{
    if (styles == NULL)
        return NULL;

    for (int i = 0; i < styles->e_idx; i++) {
        const struct cue_style *cs = dyna_elem(styles, i);

        if (cs->selector && strcmp(cs->selector, selector) == 0)
            return cs;
    }
    return NULL;
}
