#include "cuetext.h"

#include <stddef.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "dyna.h"
#include "util.h"

#define SAFE_FREE(x) if (x) free(x);

struct ctxt_class {
    char *name;
};

#define x(n, ...) #n,
static const char *ctxt_token_str_map[] = {
    TEXT_TOKEN_DEF(x)
};
#undef x

#define ex(n) #n,
static const char *ctxt_node_type_str_map[] = {
    VTT_NODE_TYPE_DEF(ex)
};
#undef ex

static void ctxt_token_free(struct ctxt_token *tok)
{
    switch (tok->type) {
        case TTOK_STRING:
            SAFE_FREE(tok->ttok_string.value);
            break;
        case TTOK_TAG_START:
            SAFE_FREE(tok->ttok_tag_start.tag_name);
            if (tok->ttok_tag_start.classes)
                dyna_destroy(tok->ttok_tag_start.classes);
            SAFE_FREE(tok->ttok_tag_start.annotation);
            break;
        case TTOK_TAG_END:
            SAFE_FREE(tok->ttok_tag_end.tag_name);
            break;
        case TTOK_TIMESTAMP:
            break;
    }
}

static void ctxt_print_node_inner(const struct vtt_node *node, int nest, int *n, char *out[*n])
{
    int wr = 0;
#define CN() *n -= wr; if (*n <= 0) return; *out += wr;
    for (int i = 0; i < nest && *n > 0; i++) {
        **out = ' ';
        (*out)++;
        (*n)--;
    }
    CN();

    wr = snprintf(*out, *n, "%s [", ctxt_node_type_str_map[node->type]);
    CN();
    for (int i = 0; node->class_names && i < node->class_names->e_idx; i++) {
        char **cn = dyna_elem(node->class_names, i);
        wr = snprintf(*out, *n, ".%s", *cn);
        CN();
    }
    wr = snprintf(*out, *n, "] ");
    CN();
    if (node->parent) {
        wr = snprintf(*out, *n, " p: %s ", ctxt_node_type_str_map[node->parent->type]);
        CN();
    }
    if (node->type == VNODE_TEXT) {
        wr = snprintf(*out, *n, " .text: %s", node->text);
        CN();
    } else if (node->type == VNODE_VOICE) {
        wr = snprintf(*out, *n, " .annotation: %s", node->annotation);
        CN();
    }
    wr = snprintf(*out, *n, "\n");
    CN();
    for (int i = 0; (node->type != VNODE_TEXT && node->type != VNODE_TIMESTAMP) && node->childs && i < node->childs->e_idx; i++) {
        const struct vtt_node *cn = dyna_elem(node->childs, i);
        ctxt_print_node_inner(cn, nest + 4, n, out);
    }
#undef CN
}

void ctxt_print_node(const struct vtt_node *root, int n, char out[n])
{
    assert(root->type == VNODE_ROOT);
    int cn = n;
    char *co = out;
    ctxt_print_node_inner(root, 0, &cn, &co);

#if 0
    printf("%s\n", ctxt_node_type_str_map[root->type]);
    printf("clen: %ld\n", root->childs->e_idx);
    struct vtt_node *n = dyna_elem(root->childs, 0);
    printf("    %s\n", ctxt_node_type_str_map[n->type]);
    printf("    nchilds %p\n", n->childs);
#endif
}

static void ctxt_normalize_annotation_str(int *pbufi, char buffer[*pbufi])
{
    int bufi = *pbufi;
    /* This needs some testing */
    int i;
    for (i = 0; i < bufi; i++) {
        /* not techinally valid, but will do for now */
        if (!isspace(buffer[i]))
            break;
    }
    /* remove leading whitespace */
    if (i > 0) {
        bufi -= i;
        memmove(buffer, &buffer[i], bufi);
    }

    for (i = 0; i < bufi; i++) {
        if (isspace(buffer[i])) {
            int j;
            for (j = i + 1; j < bufi; j++) {
                if (!isspace(buffer[j])) {
                    break;
                }
            }
            if (j - i > 1) {
                buffer[i] = ' ';
                memmove(&buffer[i+1], &buffer[j], bufi - j);
                bufi -= (j - i);
                i = j;
            }
        }
    }

    *pbufi = bufi;
}

enum ctxt_states {
    STATE_DATA,
    STATE_TAG,
    STATE_START_TAG_CLASS,
    STATE_END_TAG,
    STATE_START_TAG,
    STATE_TAG_ANNOTATION,
    STATE_TIMESTAMP,
    STATE_HTML_CHAR_REF_IN_DATA_STATE,
};

// https://html.spec.whatwg.org/multipage/named-characters.html#named-character-references
struct html_charref {
    const char *name, *chars;
};
static const struct html_charref html_charrefs[] = {
    //{ .name = "lrm", .chars = "\u200E" },
    { .name = "lrm", .chars = "" }, /* Just strip it out for now */
};

const struct html_charref *find_html_charref(const char *name)
{
    for (int i = 0; i < ARRSIZE(html_charrefs); i++) {
        const struct html_charref *cr = &html_charrefs[i];
        if (strcmp(cr->name, name) == 0) {
            return cr;
        }
    }
    return NULL;
}

/* Returns the characters advanced - 1
 * out_buff, is the output buffer for charaters
 * out_buff_idx is the index into out_buff (this will be advanced)
 * out_buff_size is the max size of it */
int read_html_character_references(const char *txt, char *out_buff, int *out_buff_idx, size_t out_buff_size)
{
#define MAX_CHARREF_SIZE 16
    char buff[MAX_CHARREF_SIZE];
    int buffi = 0;
    const struct html_charref *htmlchar;

    for (int i = 0; i < sizeof(buff); i++) {
        char c = txt[i];

        switch (c) {
        case ';':
            buff[buffi] = '\0';
            htmlchar = find_html_charref(buff);
            if (htmlchar == NULL) {
                /* Don't add anything if not found, but consume the escape sequence */
                fprintf(stderr, "[Warning] HTML escape character not found with name '%s' skipping...\n", buff);
                return buffi;
            }
            /* If found, copy it into the output buffer */
            strcpy(out_buff + *out_buff_idx, htmlchar->chars);
            *out_buff_idx += strlen(htmlchar->chars);
            assert(*out_buff_idx < out_buff_size);
            return buffi;
        default:
            buff[buffi++] = c;
        }
    }

    //printf("smth: %16s\n", txt);
    assert(buffi < sizeof(buff));

    return buffi;
}

struct dyna *ctxt_tokenize(const char *txt)
{
    char result[1024] = {0};
    int resi = 0;
    char buffer[1024] = {0};
    int bufi = 0;
    struct dyna *classes = dyna_create_size(sizeof(char*), 4);
    dyna_set_free_fn(classes, deref_free);

    struct dyna *tokens = dyna_create_size_flags(sizeof(struct ctxt_token), 6, DYNAFLAG_HEAPCOPY);
    dyna_set_free_fn(tokens, (dyna_free_fn)ctxt_token_free);

    struct ctxt_token tok = {0};

    enum ctxt_states state = STATE_DATA;

    for (;;) {
        char c = *txt;

        switch (state) {

        case STATE_DATA:
            switch (c) {
            case '<':
                if (resi == 0) {
                    state = STATE_TAG;
                    goto next;
                }
                txt--;
            /* Fall */
            case '\0':
                if (resi > 0) {
                    tok.type = TTOK_STRING;
                    tok.ttok_string.value = strndup(result, resi);
                    dyna_append(tokens, &tok);
                    resi = 0;
                }
                goto next;
                break;
            case '&':
                state = STATE_HTML_CHAR_REF_IN_DATA_STATE;
                goto next;
                break;
            default:
                result[resi++] = c;
                goto next;
            }
            break;

        case STATE_TAG:
            switch (c) {
                case '\t':
                case '\n':
                case '\f':
                case ' ':
                    state = STATE_TAG_ANNOTATION;
                    goto next;
                    break;
                case '.':
                    state = STATE_START_TAG_CLASS;
                    goto next;
                case '/':
                    state = STATE_END_TAG;
                    goto next;
                case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
                    assert(0 && "No timestamp");
                    break;
                case '>':
                    //txt++;
                case '\0':
                    state = STATE_DATA;
                    tok.type = TTOK_TAG_START;
                    tok.ttok_tag_start.tag_name = tok.ttok_tag_start.annotation = NULL;
                    tok.ttok_tag_start.classes = NULL;
                    dyna_append(tokens, &tok);
                    goto next;
                default:
                    result[resi++] = c;
                    state = STATE_START_TAG;
                    goto next;
            }
            break;

        case STATE_START_TAG:
            switch (c) {
                case '\t':
                case '\f':
                case ' ':
                    state = STATE_TAG_ANNOTATION;
                    goto next;
                case '\n':
                    assert(0 && "Newline annotation not supported!");
                    break;
                case '.':
                    state = STATE_START_TAG_CLASS;
                    goto next;
                case '>':
                    //txt++;
                case '\0':
                    state = STATE_DATA;
                    tok.type = TTOK_TAG_START;
                    tok.ttok_tag_start.tag_name = strndup(result, resi);
                    resi = 0;
                    tok.ttok_tag_start.annotation = NULL;
                    tok.ttok_tag_start.classes = NULL;
                    dyna_append(tokens, &tok);
                    goto next;
                default:
                    result[resi++] = c;
            }
            break;

        case STATE_START_TAG_CLASS:
            switch (c) {
                case '\t':
                case '\f':
                case ' ':
                    state = STATE_TAG_ANNOTATION;
                    *(char**)dyna_emplace(classes) = strndup(buffer, bufi);
                    bufi = 0;
                    goto next;
                case '\n':
                    assert(0 && "Newline annotation not supported!");
                    break;
                case '.':
                    *(char**)dyna_emplace(classes) = strndup(buffer, bufi);
                    bufi = 0;
                    goto next;
                case '>':
                    //txt++;
                case '\0':
                    state = STATE_DATA;
                    *(char**)dyna_emplace(classes) = strndup(buffer, bufi);
                    tok.type = TTOK_TAG_START;
                    tok.ttok_tag_start.tag_name = strndup(result, resi);
                    tok.ttok_tag_start.classes = classes;
                    tok.ttok_tag_start.annotation = NULL;
                    dyna_append(tokens, &tok);

                    classes = dyna_create_size(sizeof(char*), 4);
                    dyna_set_free_fn(classes, deref_free);
                    resi = bufi = 0;
                    goto next;
                default:
                    buffer[bufi++] = c;
            }
            break;

        case STATE_TAG_ANNOTATION:
            switch (c) {
            case '&':
                assert(0 && "No HTML references yet!");
                break;
            case '>':
            case '\0': {
                state = STATE_DATA;
                tok.type = TTOK_TAG_START;
                tok.ttok_tag_start.tag_name = strndup(result, resi);
                if (classes->e_idx > 0) {
                    tok.ttok_tag_start.classes = classes;
                    classes = dyna_create_size(sizeof(char*), 4);
                    dyna_set_free_fn(classes, deref_free);
                } else {
                    tok.ttok_tag_start.classes = NULL;
                }

                ctxt_normalize_annotation_str(&bufi, buffer);
                if (bufi)
                    tok.ttok_tag_start.annotation = strndup(buffer, bufi);
                else
                    tok.ttok_tag_start.annotation = NULL;

                dyna_append(tokens, &tok);
                resi = 0;
                bufi = 0;
            }
                break;
            default:
                buffer[bufi++] = c;
            }
            break;

        case STATE_END_TAG:
            switch (c) {
            case '>':
                //txt++;
            case '\0':
                state = STATE_DATA;
                tok.type = TTOK_TAG_END;
                tok.ttok_tag_end.tag_name = strndup(result, resi);
                dyna_append(tokens, &tok);
                resi = 0;
                break;
            default:
                result[resi++] = c;
                goto next;
            }
            break;

        case STATE_TIMESTAMP:
            assert(0 && "No timestamp state");
            break;
        case STATE_HTML_CHAR_REF_IN_DATA_STATE:
            txt += read_html_character_references(txt, result, &resi, sizeof(result));
            state = STATE_DATA;
            goto next;
        }

next:
        if (c == '\0')
            break;
        txt++;
    }

end:
    dyna_destroy(classes);
    return tokens;
}

void ctxt_token_print(const struct ctxt_token *tok, int len, char out[len])
{
    int i = 0;
    i += snprintf(out, len - i, "type: %s", ctxt_token_str_map[tok->type]);
    switch (tok->type) {
        case TTOK_STRING:
            i += snprintf(out + i, len - i, "\n .value = %s", tok->ttok_string.value);
            break;
        case TTOK_TAG_START: {
            i += snprintf(out + i, len - i, "\n .tag_name = %s\n .classes = ", tok->ttok_tag_start.tag_name);
            struct dyna *cl = tok->ttok_tag_start.classes;
            for (int c = 0; cl && c < cl->e_idx; c++) {
                i += snprintf(out + i, len - i, "%s ", *(char**)dyna_elem(cl, c));
            }
            break;
        }
        case TTOK_TAG_END:
            i += snprintf(out + i, len - i, "\n .tag_name = %s", tok->ttok_tag_end.tag_name);
            break;
    }
}

static void ctxt_free_node_inner(void *data)
{
    struct vtt_node *node = data;
    if (node->type == VNODE_TEXT) {
        if (node->text)
            free(node->text);
    } else { 
        if (node->childs) {
            dyna_destroy(node->childs);
        }
        if (node->class_names) {
            dyna_destroy(node->class_names);
        }
        if (node->annotation) {
            free(node->annotation);
        }
    }
}

void ctxt_free_node(struct vtt_node *node)
{
    ctxt_free_node_inner(node);
    free(node);
}

/* https://www.w3.org/TR/webvtt1/#cue-text-parsing-rules */
static struct vtt_node *ctxt_parse_nodes(struct dyna *tokens)
{
    struct vtt_node node;
    struct vtt_node *root = calloc(1, sizeof(*root));
    struct vtt_node *current = root;
    current->parent = root;

    for (int i = 0; i < tokens->e_idx; i++) {
        struct ctxt_token *tok = dyna_elem(tokens, i);

        char *tn;
        switch (tok->type) {
        case TTOK_STRING:
            /* Move instead of copy */
            memset(&node, 0, sizeof(node));
            node.type = VNODE_TEXT;
            node.text = tok->ttok_string.value;
            tok->ttok_string.value = NULL;

            node.parent = current;
            if (current->childs == NULL) {
                current->childs = dyna_create_size_flags(sizeof(struct vtt_node), 3, DYNAFLAG_HEAPCOPY);
                dyna_set_free_fn(current->childs, ctxt_free_node_inner);
            }
            dyna_append(current->childs, &node);
            break;
        case TTOK_TAG_START:
            tn = tok->ttok_tag_start.tag_name;
            memset(&node, 0, sizeof(node));
            if (strcmp(tn, "c") == 0) {
                node.type = VNODE_CLASS;
            } else if (strcmp(tn, "i") == 0) {
                node.type = VNODE_ITALIC;
            } else if (strcmp(tn, "b") == 0) {
                node.type = VNODE_BOLD;
            } else if (strcmp(tn, "u") == 0) {
                node.type = VNODE_UNDERLINE;
            } else if (strcmp(tn, "ruby") == 0) {
                node.type = VNODE_RUBY;
            } else if (strcmp(tn, "rt") == 0) {
                node.type = VNODE_RUBY_TEXT;
            } else if (strcmp(tn, "v") == 0) {
                node.type = VNODE_VOICE;
                node.annotation = tok->ttok_tag_start.annotation;
                tok->ttok_tag_start.annotation = NULL;
            } else if (strcmp(tn, "lang") == 0) {
                assert(0 && "Lang tags are not supported");
                break;
            } else {
                continue;
            }
            node.class_names = tok->ttok_tag_start.classes;
            tok->ttok_tag_start.classes = NULL;
            node.childs = NULL;
            node.parent = current;

            if (current->childs == NULL) {
                current->childs = dyna_create_size_flags(sizeof(struct vtt_node), 3, DYNAFLAG_HEAPCOPY);
                dyna_set_free_fn(current->childs, ctxt_free_node_inner);
            }
            current = dyna_append(current->childs, &node);
            break;
        case TTOK_TAG_END:
            tn = tok->ttok_tag_end.tag_name;
            if (    (strcmp(tn, "c") == 0 && current->type == VNODE_CLASS) ||
                    (strcmp(tn, "i") == 0 && current->type == VNODE_ITALIC) ||
                    (strcmp(tn, "b") == 0 && current->type == VNODE_BOLD) ||
                    (strcmp(tn, "u") == 0 && current->type == VNODE_UNDERLINE) ||
                    (strcmp(tn, "ruby") == 0 && current->type == VNODE_RUBY) ||
                    (strcmp(tn, "rt") == 0 && current->type == VNODE_RUBY_TEXT) ||
                    (strcmp(tn, "v") == 0 && current->type == VNODE_VOICE)) {
                current = current->parent;
            } else if ((strcmp(tn, "lang") == 0 && current->type == VNODE_LANGUAGE)) {
                assert(0 && "Language tag not supported");
                return NULL;
            } else if ((strcmp(tn, "ruby") == 0 && current->type == VNODE_RUBY_TEXT)) {
                current = current->parent->parent;
            }
            break;
        case TTOK_TIMESTAMP:
            assert(0 && "Timestamp not supported");
            return NULL;
        }
    }
    
    root->parent = NULL;
    return root;
}


struct vtt_node *ctxt_parse(const char *txt)
{
    struct dyna *tokens = ctxt_tokenize(txt);

    #if 0
    char buf[1024];
    for (int i = 0; i < tokens->e_idx; i++) {
        ctxt_token_print(dyna_elem(tokens, i), sizeof(buf), buf);
        printf("%s\n\n", buf);
    }
#endif

    //struct dyna *nodes = ctxt_parse_nodes(tokens);
    struct vtt_node *root_node = ctxt_parse_nodes(tokens);
    dyna_destroy(tokens);

    //ctxt_print_node(root_node);

    //ctxt_free_node(root_node);
    return root_node;
}

static int ctxt_text_inner(const struct vtt_node *node, int size, char out[size], int idx)
{
    if (node->type == VNODE_TIMESTAMP)
        return 0;

    if (node->type == VNODE_TEXT) {
        if (node->parent && node->parent->type == VNODE_RUBY_TEXT)
            return 0; /* Skip including ruby text */
        int n = snprintf(&out[idx], size - idx, "%s", node->text);
        assert(n < size - idx);
        return n;
    }
    
    int n = 0;
    for (int i = 0; node->childs && i < node->childs->e_idx; i++) {
        n += ctxt_text_inner(dyna_elem(node->childs, i), size, out, idx + n);
    }

    return n;
}

int ctxt_text(const struct vtt_node *root, int size, char out[size])
{
    return ctxt_text_inner(root, size, out, 0);
}
