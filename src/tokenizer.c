#include "tokenizer.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>

#include "reader.h"

#define x(n, ...) #n,
static const char *token_str_map[] = {
    TOKEN_DEF(x, x)
};
#undef x

static int64_t tok_cline = 1;

static void tok_free_inner(void *data)
{
    struct token *tok = data;
    switch (tok->type) {
    case TOK_IDENT:
        free(tok->ident.str);
        break;
    case TOK_CUE_SETTING:
        free(tok->cue_setting.key);
        free(tok->cue_setting.value);
        break;
    case TOK_CUE_TEXT:
        free(tok->cue_text.str);
        break;
    case TOK_STYLE_SELECTOR:
        free(tok->style_selector.str);
        break;
    case TOK_STYLE_KEYVAL:
        free(tok->style_keyval.value);
        free(tok->style_keyval.key);
        break;
    }
}

static int tok_read_magic(struct dyna *tokens)
{
    char buf[16] = {0};
    const char *exp = "WEBVTT";
    int64_t lineskip;

    int64_t r = rdr_line_peek(strlen(exp) + 1, buf, &lineskip);
    if (r == EOF) {
        fprintf(stderr, "Found EOF while parsing magic bytes\n");
        return -1;
    }

    if (r != strlen(exp) || memcmp(buf, exp, r) != 0) {
        fprintf(stderr, "Not a WEBVTT file: %s\n", buf);
        return -1;
    }

    ((struct token*)dyna_emplace(tokens))->type = TOK_FILE_MAGIC;
    rdr_skip(lineskip);

    return 0;
}

static void tok_skip_bom()
{
    uint8_t bom[3] = {0xef, 0xbb, 0xbf};
    uint8_t buf[4];
    int64_t read = rdr_peekn(sizeof(buf), buf);
    //for (int i = 0; i < read; i++) { printf("%X\n", buf[i]); }
    if (read == 4 && memcmp(bom, buf, 3) == 0) {
        rdr_skip(3);
    }
}

static void tok_skip_line()
{
    int c;
    while ((c = rdr_getc()) != EOF) {
        if (c == '\n') {
            tok_cline++;
            return;
        }
    }
}

static int tok_parse_note(struct dyna *tokens, int64_t li, char line[li])
{
    char rl[1024];
    int64_t lineskip, rli;

    rli = rdr_line_peek(sizeof(rl), rl, &rli);
    while (rli > 0) {
        rdr_skip_line();
        rli = rdr_line_peek(sizeof(rl), rl, &rli);
    }
    rdr_skip_line();

    ((struct token*)dyna_emplace(tokens))->type = TOK_NOTE;
    return 0;
}

static int tok_parse_timestamp(struct dyna *tokens, int64_t li, char line[li])
{
    int8_t hour, min, sec;
    int64_t ms;
    int consumed = 0;

    int read = sscanf(line, "%hhd:%hhd:%hhd.%ld%n", &hour, &min, &sec, &ms, &consumed);
    if (read != 4) {
        hour = 0;

        read = sscanf(line, "%hhd:%hhd.%ld%n", &min, &sec, &ms, &consumed);
        if (read != 3) {
            fprintf(stderr, "Timestamp parse, failed on sscanf: %d (%s)\n", read, line);
            return -1;
        }
    }
    ms += ((sec + ((min + hour * 60) * 60)) * 1000);

    struct token tok = { .type = TOK_TIMESTAMP, .timestamp.ms = ms };
    dyna_append(tokens, &tok);

    return consumed;
}

static int tok_parse_style_group(struct dyna *tokens)
{
    int64_t bi = 0;
    char *pos;
    char buff[1024] = {0};
    int c;

    while (true) {
        c = rdr_getc();
        if (c == EOF)
            return -1;
        if (isspace(c))
            break;
        buff[bi++] = c;
    }

    struct token tok = { .type = TOK_STYLE_SELECTOR };
    tok.style_selector.str = strndup(buff, bi);
    dyna_append(tokens, &tok);

    rdr_skip_whitespace();
    c = rdr_getc();
    if (c != '{') {
        fprintf(stderr, "Expected '{' in style after selector, got %c\n", c);
        return -1;
    }
    tok.type = TOK_STYLE_OPEN_BRACE;
    dyna_append(tokens, &tok);

    rdr_skip_whitespace();
    bi = 0;

    tok.type = TOK_STYLE_KEYVAL;
    bool in_elem = false, have_key = false, have_value = false;
    while (rdr_peek() != '}' && rdr_peek() != EOF) {
        int c = rdr_getc();

        in_elem = true;

        if (c == ':') {
            if (in_elem == false) {
                fprintf(stderr, "Style parse error: not in elem, when :\n");
                return -1;
            }
            if (have_key) {
                fprintf(stderr, "Style parse error: multiple keys?\n");
                return -1;
            }
            tok.style_keyval.key = strndup(buff, bi);
            have_key = true;
            bi = 0;
            rdr_skip_whitespace();
        } else if (c == ';') {
            if (in_elem == false) {
                fprintf(stderr, "Style parse error: not in elem, when :\n");
                return -1;
            }
            if (have_key == false) {
                fprintf(stderr, "Style parse error: value without key?\n");
                return -1;
            }
            tok.style_keyval.value = strndup(buff, bi);
            dyna_append(tokens, &tok);
            in_elem = have_key = have_value = false;
            bi = 0;
            rdr_skip_whitespace();
        } else {
            buff[bi++] = (char)c;
        }
    }
    if (rdr_peek() == EOF) {
        fprintf(stderr, "Style parse error: End of file inside block\n");
        return -1;
    }
    rdr_skip(1); /* skip the '}' */
    rdr_skip_line(); /* Skip until the next line */
    tok.type = TOK_STYLE_CLOSE_BRACE;
    dyna_append(tokens, &tok);

    return 0;
}

static int tok_parse_style(struct dyna *tokens)
{
    char buff[1024];
    int64_t bi;
    int en;

    bi = rdr_line_peek(sizeof(buff), buff, NULL);
    while (bi > 0) {
        en = tok_parse_style_group(tokens);
        if (en == -1)
            return -1;
        //rdr_skip_whitespace();
        bi = rdr_line_peek(sizeof(buff), buff, NULL);
        //printf("Bi: %ld - %s %.20s\n", bi, buff, rdr_curptr());
    }
    return 0;

    while (true) {
        char p2[2];
        if (rdr_peekn(2, p2) != 2)
            return -1;
        //printf("Read: '%X' and '%X'\n", p2[0], p2[1]);
        if (p2[0] == '\n' && p2[1] == '\n') {
            /* Double newline, end the block */
            rdr_skip(2);
            return 0;
        }
        //printf("Read: ");
        //for (int i = 0; i < 5; i++)
            //putchar(rdr_getc());
        en = tok_parse_style_group(tokens);
        if (en == -1)
            return -1;
    }

    return 0;
}

static int tok_parse_cue_attrib(struct dyna *tokens, int64_t li, char pos[li])
{
    if (li == 0)
        return 0; /* no attribs */

    while(li > 0 && isspace(*pos)) {
        /* Skip whitespace */
        li--;
        pos++;
    }

    while (li > 0) {
        struct token tok = { .type = TOK_CUE_SETTING };
        char *sep = strchr(pos, ':');
        if (sep == NULL)
            return -1;
        tok.cue_setting.key = strndup(pos, sep - pos);
        li -= sep - pos;
        pos = sep + 1;
        sep = strchr(pos, ' ');
        if (sep == NULL) {
            sep = pos + li;
        }
        tok.cue_setting.value = strndup(pos, sep - pos);
        li -= sep - pos;
        pos = sep + 1;

        dyna_append(tokens, &tok);
    }

    return 0;
}

static int tok_parse_cue_text(struct dyna *tokens)
{
    int64_t li, lineskip;
    char line[1024];

    while (true) {
        li = rdr_line_peek(sizeof(line), line, &lineskip);
        if (li == EOF)
            return 0;
        if (li == 0) {
            /* End of cue */
            //printf("ENd of cue: %s %ld\n", line, lineskip);
            rdr_skip(lineskip);
            return 0;
        }

        struct token tok = { .type = TOK_CUE_TEXT };
        tok.cue_text.str = strdup(line);
        dyna_append(tokens, &tok);
        rdr_skip(lineskip);
    }

    return 0;
}

static int tok_parse_cue(struct dyna *tokens, int64_t li, char line[li])
{
    char *pos = line;
    int en;

    en = tok_parse_timestamp(tokens, li, line);
    if (en == -1)
        return -1;
    li -= en;
    pos += en;

    if (memcmp(pos, " --> ", strlen(" --> ")) == 0) {
        pos += strlen(" --> ");
        li -= strlen(" --> ");
        struct token tok = { .type = TOK_ARROW };
        dyna_append(tokens, &tok);
    } else {
        return -1;
    }

    en = tok_parse_timestamp(tokens, li, pos);
    if (en == -1)
        return -1;
    li -= en;
    pos += en;

    /* TODO: line attrib */
    en = tok_parse_cue_attrib(tokens, li, pos);
    if (en == -1)
        return -1;

    rdr_skip_line();
    en = tok_parse_cue_text(tokens);
    if (en == -1)
        return -1;

    return 0;
}

static int tok_parse_line_ident(struct dyna *tokens, int64_t li, char line[li])
{
    /* line is an ident, make an TOK_IDENT and copy the line contents */
    struct token tok = { .type = TOK_IDENT };
    char *ident_str = strdup(line);
    if (ident_str == NULL)
        return -1;
    tok.ident.str = ident_str;

    dyna_append(tokens, &tok);

    return 0;
}

static void tok_skip_whitespace(int64_t *li, char *line)
{
    /* My test file has 114 spaces on an empty line for some reason lol */
    int prev_space = 0;
    for (; prev_space < *li && isspace(line[prev_space]); prev_space++)
        ;
    if (prev_space == *li) {
        /* all whitespace */
        *li = 0;
        line[0] = '\0';
        return;
    }

    /* Move non-whitespace chars to the front */
    memmove(line, line + prev_space, *li - prev_space);
    *li -= prev_space;
}

struct dyna *tok_tokenize()
{
    struct dyna *tokens = dyna_create(sizeof(struct token));
    int en, c;

    dyna_set_free_fn(tokens, tok_free_inner);
    if (rdr_pos() != 0) {
        goto error;
    }

    tok_skip_bom();

    en = tok_read_magic(tokens);
    if (en != 0)
        goto error;

    char line[1024];
    int64_t li = 0, lineskip;
    bool in_ts_parse = false;
    while ((li = rdr_line_peek(sizeof(line), line, &lineskip)) != EOF) {
        tok_cline++;
        tok_skip_whitespace(&li, line);
        if (li == 0)
            goto next_skip; /* empty line */
        //printf("Line: '%s'\n", line);

        if (li >= 4 && strncmp(line, "NOTE", 4) == 0) {
            rdr_skip(lineskip);
            en = tok_parse_note(tokens, li, line);
            if (en == -1)
                goto error;
            goto next_noskip;
        }

        if (li >= 5 && strncmp(line, "STYLE", strlen("STYLE")) == 0) {
            rdr_skip(lineskip);
            en = tok_parse_style(tokens);
            if (en == -1)
                goto error;
            goto next_noskip;
        }

        if (strstr(line, "-->") != NULL) {
            en = tok_parse_cue(tokens, li, line);
            if (en == -1)
                goto error;
            goto next_noskip;
        } else {
            en = tok_parse_line_ident(tokens, li, line);
            if (en == -1)
                goto error;
            goto next_skip;
        }

#if 0
        if (isdigit(c)) {
            en = tok_parse_numline(tokens);
            if (en != 0)
                goto error;
            goto next_noskip;
        }
#endif
        printf("Unknown line at linenum %ld: '%s'\n", tok_cline, line);
        goto end;

next_skip:
        rdr_skip(lineskip);
        continue;
next_noskip:
        continue;
    }

end:
    return tokens;

error:
    printf("Failed parse on line %ld: '%s'\n", tok_cline, line);
    dyna_destroy(tokens);
    return NULL;
}

const char *tok_type2str(enum token_type type)
{
    return token_str_map[type];
}

char *tok_2str(struct token *tok, int maxn, char out[maxn])
{
    int n = sprintf(out, "%s: ", tok_type2str(tok->type));
    switch (tok->type) {
        case TOK_IDENT:
            n += sprintf(out + n, ".str = '%s'", tok->ident.str);
            break;
        case TOK_TIMESTAMP:
            n += sprintf(out + n, ".ms = %ld", tok->timestamp.ms);
            break;
        case TOK_CUE_SETTING:
            n += sprintf(out + n, "%s = %s", tok->cue_setting.key, tok->cue_setting.value);
            break;
        case TOK_CUE_TEXT:
            n += sprintf(out + n, "%s", tok->cue_text.str);
            break;
        case TOK_STYLE_SELECTOR:
            n += sprintf(out + n, "%s", tok->style_selector.str);
            break;
        case TOK_STYLE_KEYVAL:
            n += sprintf(out + n, "%s = %s", tok->style_keyval.key, tok->style_keyval.value);
            break;
    }

    return out;
}

