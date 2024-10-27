#include "reader.h"

#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/param.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

static const uint8_t *file_data = NULL;
static int64_t file_index = 0, file_size = 0;

int rdr_init(const char *filename)
{
    int en, fd = -1;
    void *mm = MAP_FAILED;
    struct stat fs;

    en = stat(filename, &fs);
    if (en != 0) {
        perror("stat() on filename");
        return -1;
    }

    fd = open(filename, O_RDONLY, 0);
    if (fd == -1) {
        perror("open() on filename");
        return -1;
    }
    
    mm = mmap(NULL, fs.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mm == MAP_FAILED) {
        perror("mmap()");
        return -1;
    }
    close(fd);

    file_index = 0;
    file_size = fs.st_size;
    file_data = mm;

    //printf("---\n%.30s\n---\n", mm);

    return 0;
}

int rdr_free()
{
    int en = munmap((void*)file_data, file_size);
    if (en != 0) {
        perror("munmap()");
        return -1;
    }
    file_index = file_size = 0;
    file_data = NULL;
    return 0;
}

int rdr_peek()
{
    if (file_index >= file_size)
        return EOF;
    int c = file_data[file_index];
    if (c == '\r') {
        if (file_index >= file_size)
            return c;
        if (file_data[file_index + 1] == '\n') {
            /* Convert \r\n to \n */
            file_index++;
            return '\n';
        }
    }
}

void rdr_skip(int64_t n)
{
    file_index += n;
}

int64_t rdr_peekn(int64_t n, char out[n])
{
    int64_t to_read = MIN(file_size - file_index, n);
    if (to_read == 0)
        return EOF;

    int64_t offset = 0;
    int64_t i = 0;
    for (; i < n && file_index + i + offset - 1 < file_size; i++) {
        out[i] = file_data[file_index + i + offset];
        if (out[i] == '\r') {
            if (file_data[file_index + i + offset + 1] == '\n') {
                out[i] = '\n';
                offset++;
            }
        }
    }
    if (i == 0)
        return EOF;

    out[i-1] = '\0';
    return i;
}

int rdr_getc()
{
    int c = rdr_peek();
    file_index++;
    return c;
}

int64_t rdr_readn(int64_t n, char out[n])
{
    int64_t read = rdr_peekn(n, out);
    rdr_skip(read);
    return read;
}

int64_t rdr_pos()
{
    return file_index;
}

const char *rdr_curptr()
{
    return (char*)&file_data[file_index];
}

int64_t rdr_line_peek(int64_t n, char out[n], int64_t *opt_skipcount)
{
    if (rdr_peek() == EOF)
        return EOF;
    int64_t rem_len = file_size - file_index;
    assert(rem_len > 0);

    const uint8_t *end = memchr(file_data + file_index, '\n', rem_len);
    if (end == NULL) {
        /* File is not newline terminated, copy until the end */
        size_t copy_amount = MIN(n - 1, rem_len);
        memcpy(out, file_data + file_index, copy_amount);
        out[copy_amount] = '\0';
        if (opt_skipcount)
            *opt_skipcount = copy_amount;
        return copy_amount;
    }

    if (end == file_data + file_index + -1) {
        /* Empty line */
        printf("empty\n");
    }

    /* Newline found, copy until that */
    if (opt_skipcount)
        *opt_skipcount = (end + 1) - (file_data + file_index);
    if (*(end - 1) == '\r')
        end--;
    ssize_t copy_amount = MIN(n - 1, (end) - (file_data + file_index));
    if (copy_amount <= 0) {
        out[0] = '\0';
        return 0;
    }
    memcpy(out, file_data + file_index, copy_amount);
    out[copy_amount] = '\0';
    return copy_amount;
}

void rdr_skip_line(void)
{
    uint8_t *endl = memchr(file_data + file_index, '\n', file_size - file_index);
    if (endl == NULL) {
        /* No endl termination, skip until EOF */
        file_index = file_size;
        return;
    }

    file_index += endl - (file_data + file_index) + 1;
}

void rdr_skip_whitespace(void)
{
    int c = rdr_peek();
    if (c == EOF)
        return;
    while (isspace(c)) {
        rdr_skip(1);
        c = rdr_peek();
    }
}
