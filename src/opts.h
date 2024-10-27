#ifndef _VTT2ASS_OPTS_H
#define _VTT2ASS_OPTS_H
#include <stdbool.h>

/* global options */
extern bool opts_srt;
extern bool opts_ass;
extern const char *opts_infile;

/* ass options */
extern const char *opts_ass_outfile;
extern int opts_ass_vid_w, opts_ass_vid_h;
extern int opts_ass_border_size;
extern const char *opts_ass_fontfile;
extern bool opts_ass_debug_boxes;

/* srt options */
extern const char *opts_srt_outfile;

int opts_parse(int argc, const char **argv);

#endif /* _VTT2ASS_OPTS_H */
