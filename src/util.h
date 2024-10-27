#ifndef _VTT2ASS_UTIL_H
#define _VTT2ASS_UTIL_H
#include "cuetext.h"
#include "ass.h"

#define S_IN_MS (1000)
#define M_IN_MS (S_IN_MS * 60)
#define H_IN_MS (M_IN_MS * 60)

#define ARRSIZE(x) (sizeof(x)/sizeof(*x))
#define SAFE_FREE(x) if (x) free(x);
#define STRINGIFY(x) #x

#define BP asm("int $3")

struct text_extents {
    double x_adv, y_adv, x_off, y_off, width, height;
};

void deref_free(void *arg);

int util_count_node_lines(const struct vtt_node *root);

void util_get_text_extents(const char *fontname, const char *text, int fs, struct text_extents *out_ex);
void util_get_text_extents_line(const char *fontname, const char *text, int text_len, unsigned int text_offset, int item_len, int fs, struct text_extents *out_ex);
void util_get_text_extents_lines(const char *fontname, const char *text, int fs, int ex_max_size, struct text_extents out_ex[ex_max_size], int *ex_len);
void util_combine_extents(int ex_len, const struct text_extents ex[ex_len], struct text_extents *out);

int util_utf8_ccount(int s_len, const char s[s_len]);
bool util_is_utf8_start(char chr);
void util_cue_pos_to_an7(const struct ass_cue_pos *pos, const struct text_extents *ext, struct ass_cue_pos *an7_pos);

uint32_t util_colorname_to_rgb(const char *name);

void util_init();


#endif /* _VTT2ASS_UTIL_H */
