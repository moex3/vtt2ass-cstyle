#ifndef _VTT2ASS_PARSER_H
#define _VTT2ASS_PARSER_H
#include "dyna.h"
#include <math.h>
#include <stdbool.h>

#include "cuetext.h"
#include "cuestyle.h"

enum cue_writing_direction {
    WD_HORIZONTAL = 0, /* def */
    WD_VERTICAL_GROW_LEFT,
    WD_VERTICAL_GROW_RIGHT,
};

enum cue_line_align {
    LINE_ALIGN_START = 0, /* def */
    LINE_ALIGN_CENTER,
    LINE_ALIGN_END,
};

enum cue_pos_align {
    POS_ALIGN_AUTO = 0, /* def */
    POS_ALIGN_LINE_LEFT,
    POS_ALIGN_CENTER,
    POS_ALIGN_LINE_RIGHT,
};

enum cue_text_align {
    TEXT_ALIGN_CENTER = 0, /* def */
    TEXT_ALIGN_START,
    TEXT_ALIGN_END,
    TEXT_ALIGN_LEFT,
    TEXT_ALIGN_RIGHT,
};

enum cue_base_direction {
    BDIR_LTR = 0, /* default */
    BDIR_RTL,
};

extern const char *str_cue_pos_align[];
extern const char *str_cue_text_align[];

#define CUE_AUTO NAN
#define IS_AUTO(f) (isnan(f))
struct cue {
    char *ident; /* def NULL */
    /* true if 'lines' is an integer, false if it is a percentage. def true */
    bool snap_to_lines;

    /* This is the offset of the cue box in video percentage
     * from the sides, in the way different from the writing_direction */
    float line; /* def AUTO */
    /* Where to calculate the line percentage at.
     * start: left/top side
     * center: middle
     * end: right/bottom side */
    enum cue_line_align line_align; /* def START */

    /* https://www.w3.org/TR/webvtt1/#webvtt-cue-position */
    /* Specifies the indent from the sides of the viewport
     * in the same direction as the writing_direction */
    float position; /* def AUTO */
    /* Where to calculate the position at:
     * line-left: left/top side
     * center: middle
     * line-right: right/bottom side */
    enum cue_pos_align pos_align; /* def AUTO */

    /* The size of the box in the writing_direction */
    float size; /* def 100 */
    enum cue_writing_direction writing_direction; /* def HORIZONTAL */

    /* Sets the alignment within the cue box
     * start and end can mean different things based on the base_direction */
    enum cue_text_align text_align; /* 'align:' def CENTER */

    struct vtt_node *text_node; /* root node of the text nodes, def NULL */

    enum cue_base_direction base_direction; /* Text writing direction, def BDIR_LTR */

    int64_t time_start, time_end; /* start and end time in ms */
};

/* return -1 on error */
int prs_parse_tokens(struct dyna *tokens, struct dyna **cues, struct dyna **styles);

void prs_cue2str(int size, char out_str[size], const struct cue *cue);

#endif /* _VTT2ASS_PARSER_H */
