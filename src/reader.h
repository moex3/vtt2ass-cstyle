#ifndef _VTT2ASS_READER_H
#define _VTT2ASS_READER_H
#include <stdint.h>

int rdr_init(const char *filename);
int rdr_free();

int rdr_getc();
int rdr_peek();
int64_t rdr_readn(int64_t n, char out[n]);
/* Returns number of items peeked (can be less than n), or -1 on error */
int64_t rdr_peekn(int64_t n, char out[n]);

/* Peeks until a newline character, or EOF.
 * The newline is not copied to out
 * Assumes that we are at the start of the line
 * Return EOF in case of EOF
 * Returns the characters copied to out, will 0 terminate */
/* opt_skipcount will contain the value, such that rdr_skip(opt_skipcount)
 * will skip over the entire line */
int64_t rdr_line_peek(int64_t n, char out[n], int64_t *opt_skipcount);

/* Skip n characters */
void rdr_skip(int64_t n);
/* Skips the current line */
void rdr_skip_line(void);
void rdr_skip_whitespace(void);

int64_t rdr_pos();
const char *rdr_curptr();

#endif /* _VTT2ASS_READER_H */
