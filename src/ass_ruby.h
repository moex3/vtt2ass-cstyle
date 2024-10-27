#ifndef _VTT2ASS_ASS_RUBY_H
#define _VTT2ASS_ASS_RUBY_H
#include "dyna.h"
#include "ass.h"
#include "cuetext.h"
#include "cuepos.h"


void ass_ruby_write(const struct cue *c, const struct ass_cue_pos *olpos, const struct ass_params *ap);

#endif /* _VTT2ASS_ASS_RUBY_H */
