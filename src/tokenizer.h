#ifndef _VTT2ASS_TOKENIZER_H
#define _VTT2ASS_TOKENIZER_H
#include "dyna.h"

#define TOKEN_DEF(ex_simpl, ex_compl) \
    ex_simpl(TOK_EOF) \
    ex_simpl(TOK_FILE_MAGIC) /* 1st characters are WEBVTT */ \
    ex_simpl(TOK_NOTE) /* is a note */ \
    ex_compl(TOK_IDENT, ident, { char *str; /*free*/ }) /* An indentifier before a timestamp line */ \
    ex_compl(TOK_TIMESTAMP, timestamp, { int64_t ms; }) /* A timestamp line, ms is the miliseconds since the beginning */ \
    ex_simpl(TOK_ARROW) /* --> */ \
    ex_compl(TOK_CUE_SETTING, cue_setting, { char *key, *value; /* free both */ }) /* A timestamp line, ms is the miliseconds since the beginning */ \
    ex_compl(TOK_CUE_TEXT, cue_text, { char *str; /* free */ }) /* Text contents of a cue (one line) */ \
    ex_compl(TOK_STYLE_SELECTOR, style_selector, { char *str; /* free */ }) /* The selector string of the STYLE element */ \
    ex_simpl(TOK_STYLE_OPEN_BRACE) /* '{' */ \
    ex_simpl(TOK_STYLE_CLOSE_BRACE) /* '}' */ \
    ex_compl(TOK_STYLE_KEYVAL, style_keyval, { char *key, *value; /* free */ }) /* The selector string of the STYLE element */ \

#define ex_simpl(n) n,
#define ex_compl(n, ...) n,
enum token_type {
    TOKEN_DEF(ex_simpl, ex_compl)
};
#undef ex_simpl
#undef ex_compl

#define ex_simpl(tok_name)
#define ex_compl(tok_name, struct_name, ...) \
    struct struct_name __VA_ARGS__ struct_name;
struct token {
    enum token_type type;
    union {
        TOKEN_DEF(ex_simpl, ex_compl)
    };
};
#undef ex_simpl
#undef ex_compl


/* Needs to have rdr_init() called before this */
struct dyna *tok_tokenize();

char *tok_2str(struct token *tok, int maxn, char out[maxn]);
const char *tok_type2str(enum token_type type);

#endif /* _VTT2ASS_TOKENIZER_H */
