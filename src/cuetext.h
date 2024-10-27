#ifndef _VTT2ASS_CUETEXT_H
#define _VTT2ASS_CUETEXT_H
#include <stdint.h>
#include "dyna.h"

#define TEXT_TOKEN_DEF(ex_compl) \
    ex_compl(TTOK_STRING, ttok_string, { char *value; /* free */ }) \
    ex_compl(TTOK_TAG_START, ttok_tag_start, { char *tag_name; struct dyna *classes; char *annotation; /* free */ }) \
    ex_compl(TTOK_TAG_END, ttok_tag_end, { char *tag_name; /* free */ }) \
    ex_compl(TTOK_TIMESTAMP, ttok_timestamp, { float value; }) \

#define ex_compl(n, ...) n,
enum ctxt_token_type {
    TEXT_TOKEN_DEF(ex_compl)
};
#undef ex_compl

#define ex_compl(tok_name, struct_name, ...) \
    struct struct_name __VA_ARGS__ struct_name;
struct ctxt_token {
    enum ctxt_token_type type;
    union {
        TEXT_TOKEN_DEF(ex_compl)
    };
};
#undef ex_compl

#define VTT_NODE_TYPE_DEF(ex) \
    ex(VNODE_ROOT) \
    ex(VNODE_CLASS) \
    ex(VNODE_ITALIC) \
    ex(VNODE_BOLD) \
    ex(VNODE_UNDERLINE) \
    ex(VNODE_RUBY) \
    ex(VNODE_RUBY_TEXT) \
    ex(VNODE_VOICE) \
    ex(VNODE_LANGUAGE) \
\
    /* Leaves */ \
    ex(VNODE_TEXT) \
    ex(VNODE_TIMESTAMP) \

#define ex(n) n,
enum vtt_node_type {
    VTT_NODE_TYPE_DEF(ex)
};
#undef ex

/* WebVTT Internal Node Object */
struct vtt_node {
    enum vtt_node_type type;

    /* Applicable class names, or NULL */
    struct dyna *class_names;

    /* annotation string, only for VNODE_VOICE, can be NULL */
    char *annotation;

    /* language tag not supported */

    struct vtt_node *parent;
    union {
        /* The ordered list of child WebVTT Node Objects */
        struct dyna *childs; /* struct vtt_node */

        /* Only in the case of VNODE_TEXT */
        char *text;

        /* Only in the case of VNODE_TIMESTAMP (unsupported) */
        int64_t timestamp;
    };

};

struct vtt_node *ctxt_parse(const char *txt);
struct dyna *ctxt_tokenize(const char *txt);
void ctxt_free_node(struct vtt_node *node);

int ctxt_text(const struct vtt_node *root, int size, char out[size]);
void ctxt_token_print(const struct ctxt_token *tok, int len, char out[len]);
void ctxt_print_node(const struct vtt_node *root, int n, char out[n]);

#endif /* _VTT2ASS_CUETEXT_H */
