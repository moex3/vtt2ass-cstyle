#ifndef _VTT2ASS_DYNA_H
#define _VTT2ASS_DYNA_H
#include <stdint.h>
#include <stddef.h>

#define DYNA_ELEM(type, dyna, i) ((type)dyna_elem(dyna, i))

enum dyna_flags {
    DYNAFLAG_NONE       = 0u,       /* Works normally, aka copy e_size bytes to data */
    DYNAFLAG_HEAPCOPY   = 1u << 0,  /* Heap alloc e_size bytes, and copy the pointer to data */
};

typedef void(*dyna_free_fn)(void *elem);

struct dyna {
    size_t e_size; /* size of 1 element */
    int64_t e_cap; /* The maximum amount of elements that can be stored */
    int64_t e_idx; /* The current index of the next location to place elements in */
    dyna_free_fn e_free_fn; /* This function will be called for each element on dyna_destroy */
    enum dyna_flags flags; /* Had to add this because I messed up somewhere lol */

    void* data; /* The memory location where the element array is stored */
};

struct dyna *dyna_create(size_t e_size);
struct dyna *dyna_create_size(size_t e_size, size_t init_size);
struct dyna *dyna_create_size_flags(size_t e_size, size_t init_size, enum dyna_flags flags);
void dyna_destroy(struct dyna *dyna);
void dyna_set_free_fn(struct dyna *dyna, dyna_free_fn fn);

void *dyna_append(struct dyna *dyna, void *elem);
/* Just returns the pointer to the next element, and increments e_idx, asserts on error */
void *dyna_emplace(struct dyna *dyna);
void *dyna_elem(const struct dyna *dyna, int64_t idx);

#endif /* _VTT2ASS_DYNA_H */
