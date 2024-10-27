#ifndef _VTT2ASS_SRT_H
#define _VTT2ASS_SRT_H
#include "parser.h"
#include "cuestyle.h"
#include "dyna.h"

int srt_write(struct dyna *cues, struct dyna *cstyles, const char *fname);

#endif /* _VTT2ASS_SRT_H */
