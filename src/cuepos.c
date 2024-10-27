#include "cuepos.h"

#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/param.h>

const struct video_info *vinf = NULL;

void cuepos_set_video_info(const struct video_info *vi)
{
    vinf = vi;
}

/* https://www.w3.org/TR/webvtt1/#cue-computed-position */
static float cuepos_compute_pos(const struct cue *c)
{
    if (!IS_AUTO(c->position)) {
        return c->position;
    }

    switch (c->text_align) {
    case TEXT_ALIGN_START: // own
    case TEXT_ALIGN_LEFT:
        return 0;
    case TEXT_ALIGN_END: // own
    case TEXT_ALIGN_RIGHT:
        return 1;
    default:
        return 0.5;
    }
}

/* https://www.w3.org/TR/webvtt1/#cue-computed-line */
static float cuepos_compute_line(const struct cue *c)
{
    // 1
    if (IS_AUTO(c->line) == false && c->snap_to_lines == false && (c->line < 0.0f || c->line > 1.0f)) {
        return 1.0f;
    }

    // 2
    if (IS_AUTO(c->line) == false) {
        return c->line;
    }

    // 3
    if (c->snap_to_lines == false)
        return 1.0f;

    //assert(0 && "snap to line thing is half-baked lol");
    // This would basically return the integer number of lines currently showing
    return -1;
}

/* https://www.w3.org/TR/webvtt1/#webvtt-cue-position-alignment */
enum cue_pos_align cuepos_compute_pos_align(const struct cue *c)
{
    if (c->pos_align != POS_ALIGN_AUTO) {
        return c->pos_align;
    }

    switch (c->text_align) {
        case TEXT_ALIGN_LEFT:
            return POS_ALIGN_LINE_LEFT;
        case TEXT_ALIGN_RIGHT:
            return POS_ALIGN_LINE_RIGHT;
        case TEXT_ALIGN_START:
            return (c->base_direction == BDIR_LTR) ? POS_ALIGN_LINE_LEFT : POS_ALIGN_LINE_RIGHT;
        case TEXT_ALIGN_END:
            return (c->base_direction == BDIR_LTR) ? POS_ALIGN_LINE_RIGHT : POS_ALIGN_LINE_LEFT;
    }
    return POS_ALIGN_CENTER;
}

void cuepos_apply_cue_settings_mine(const struct cue *c, struct cuepos_box *op)
{
    memset(op, 0, sizeof(*op));

    float size = -100;
    float max_size = -100;
    /* In screen coords */
    float width = -100, height = -100;
    float xpos = -100, ypos = -100;
    /* In screen coords */
    float left = -100, top = -100;
    float full_dimension = -100;
    float step = -100;

    bool is_wdh = c->writing_direction == WD_HORIZONTAL;

    enum cue_pos_align comp_pos_align = cuepos_compute_pos_align(c);

    if (IS_AUTO(c->position))
        left = 0;
    else
        left = c->position;

    assert(fabs(c->line) <= 1.0);
    top = 0;
    if (IS_AUTO(c->line))
        height = 1;
    else
        switch (c->line_align) {
        case LINE_ALIGN_END:
            height = c->line;
            break;
        case LINE_ALIGN_CENTER:
            height = c->line * 2;
            break;
        default:
        case LINE_ALIGN_START:
            top = c->line;
            height = 1 - top;
            break;
        }

    switch (c->pos_align) {
    case POS_ALIGN_LINE_RIGHT:
        left = left - c->size;
        break;
    case POS_ALIGN_CENTER:
        left = left - c->size / 2.0f;
    default:
    case POS_ALIGN_LINE_LEFT:
        break;
    }

    op->left = left * vinf->width;
    op->top = top * vinf->height;
    op->width = c->size * vinf->width;
    op->height = height * vinf->height;
}

/* https://www.w3.org/TR/webvtt1/#apply-webvtt-cue-settings */
void cuepos_apply_cue_settings(const struct cue *c, struct cuepos_box *op)
{
    memset(op, 0, sizeof(*op));

    /* top, left, width, height are the css box sizes */
    float size = -100;
    float max_size = -100;
    /* In screen coords */
    float width = -100, height = -100;
    float xpos = -100, ypos = -100;
    /* In screen coords */
    float left = -100, top = -100;
    float full_dimension = -100;
    float step = -100;

    bool is_wdh = c->writing_direction == WD_HORIZONTAL;

    // 1

    //asm("int $3");
    enum cue_pos_align comp_pos_align = cuepos_compute_pos_align(c);
    float comp_pos = cuepos_compute_pos(c);
#if 0
    printf("Comp pos: %f   %s\n", comp_pos, str_cue_text_align[c->text_align]);
    printf("posalign: %s  comp posalign: %s - ", str_cue_pos_align[c->pos_align], str_cue_pos_align[comp_pos_align]);
    ass_write_text(stdout, c->text_node);
    printf("\n");
#endif
    switch (comp_pos_align) {
    case POS_ALIGN_LINE_LEFT:
        max_size = 1.0f - comp_pos;
        break;
    case POS_ALIGN_LINE_RIGHT:
        max_size = comp_pos;
        break;
    case POS_ALIGN_CENTER:
        if (comp_pos <= 0.5f)
            max_size = comp_pos * 2;
        else
            max_size = (1.0f - comp_pos) * 2;
        break;
    default:
        assert(0 && "Unknown comp_pos_align");
    }

    // 3
    if (c->size < max_size)
        size = c->size;
    else
        size = max_size;
    //asm("int $3");

    // 4
    if (is_wdh) {
        width = size * vinf->width;
        height = CUE_AUTO;
    } else {
        width = CUE_AUTO;
        height = size * vinf->height;
    }

    //asm("int $3");
    // 5
    float *xypos_ptr, *xypos_other_ptr;
    if (is_wdh) {
        xypos_ptr = &xpos;
        xypos_other_ptr = &ypos;
    } else {
        xypos_ptr = &ypos;
        xypos_other_ptr = &xpos;
    }
    switch (comp_pos_align) {
    case POS_ALIGN_LINE_LEFT:
        // xpos = 0.5
        *xypos_ptr = comp_pos;
        break;
    case POS_ALIGN_CENTER:
        *xypos_ptr = comp_pos - (size / 2.0f);
        break;
    case POS_ALIGN_LINE_RIGHT:
        *xypos_ptr = (comp_pos - size);
        break;
    default:
        assert(0 && "Not handled def case 1");
    }

    // 6
    float comp_line = cuepos_compute_line(c);
    //asm("int $3");
    if (c->snap_to_lines == false) {
        *xypos_other_ptr = comp_line;
        if (c->line_align == LINE_ALIGN_START && is_wdh) // mine
            *xypos_other_ptr = 1 - *xypos_other_ptr;
    } else {
        *xypos_other_ptr = 0;
    }

    // 7
    left = xpos * vinf->width;
    top = ypos * vinf->height;

    if (is_wdh) {
        if (IS_AUTO(width)) {
            width = size * vinf->width;
        }
        //asm("int $3");
        if (IS_AUTO(height)) {
            // mine
            if (c->line_align == LINE_ALIGN_START)
                height = vinf->height - top;
            else if (c->line_align == LINE_ALIGN_CENTER)
                height = MIN(vinf->height - top, top) * 2;
                //height = vinf->height / 2 - top;
            else if (c->line_align == LINE_ALIGN_END)
                height = top;

            //height = vinf->height - (top * fact); // kinda mine as well
            //printf("%f\n", top + height);
        }
    } else {
        if (IS_AUTO(height)) {
            assert(false);
        }
        //asm("int $3");
        if (IS_AUTO(width)) {
            // mine
            if (c->line_align == LINE_ALIGN_START)
                width = vinf->width - left;
            else if (c->line_align == LINE_ALIGN_CENTER)
                width = MIN(vinf->width - left, left) * 2;
            else if (c->line_align == LINE_ALIGN_END)
                width = left;

            //height = vinf->height - (top * fact); // kinda mine as well
            //printf("%f\n", top + height);
        }
    }

    // 8
    /* Obtain CSS boxes */

    // 9
    /* skipped */

    // 10
    if (c->snap_to_lines) {
        full_dimension = (is_wdh) ? vinf->height : vinf->width;
        /* not quite */
        //step = (is_wdh) ? op->fs : op->fs/2.5f;
        step = 0;
        if (step == 0) {
            goto done_pos;
        }
        /* We could do something like storing all visible
         * lines and calculate this based on the sizes of those */
        assert(0);

    } else {
        /* TODO: make this apply to all currently shown cues */
        float *shiftptr = (is_wdh) ? &top : &left;
        float *offptr = (is_wdh) ? &height : &width;
        switch (c->line_align) {
        case LINE_ALIGN_START: // mine
            if (is_wdh) {
                height = vinf->height - top;
                top = 0;
            } else if (c->writing_direction == WD_VERTICAL_GROW_RIGHT) {
                width = vinf->width - left;
                left = 0;
            }
            break;
        case LINE_ALIGN_CENTER:
            *shiftptr -= *offptr / 2;
            break;
        case LINE_ALIGN_END: // spec
            *shiftptr -= *offptr;
            break;
        }

        /* Skip overlap fixing here */
    }

done_pos:
    /*
    obox->size = size;
    obox->width = width;
    obox->height = height;
    obox->xpos = xpos;
    obox->ypos = ypos;
    obox->left = left;
    obox->top = top;
    obox->full_dimension = full_dimension;
    */

    /*
    float l;
    if (IS_AUTO(c->line)) {
        l = 1;
    } else {
        l = 1 - c->line;
    }
    op->posy = vinf->height - ypos - vinf->height * l;
    */

    //BP;
    op->left = left;
    op->top = top;
    op->width = width;
    op->height = height;
    //asm("int $3");
    return;
}
