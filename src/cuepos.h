#ifndef _VTT2ASS_CUEPOS_H
#define _VTT2ASS_CUEPOS_H

#include "parser.h"

struct video_info {
    int width, height;
};
extern const struct video_info *vinf;
void cuepos_set_video_info(const struct video_info *vi);

struct cuepos_box {
    int left, top, width, height;
};

/* https://www.w3.org/TR/webvtt1/#apply-webvtt-cue-settings */
void cuepos_apply_cue_settings(const struct cue *c, struct cuepos_box *op);

enum cue_pos_align cuepos_compute_pos_align(const struct cue *c);

#endif /* _VTT2ASS_CUEPOS_H */
