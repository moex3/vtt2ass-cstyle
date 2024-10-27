#include "dyna.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>

static void *dyna_elem_(const struct dyna *d, int64_t idx)
{
    if (d->flags & DYNAFLAG_HEAPCOPY) {
        return ((uint8_t*)d->data) + (sizeof(void*) * idx);
    }
    return ((uint8_t*)d->data) + (d->e_size * idx);
}

struct dyna *dyna_create(size_t e_size)
{
    return dyna_create_size_flags(e_size, 64, DYNAFLAG_NONE);
}

struct dyna *dyna_create_size(size_t e_size, size_t init_size)
{
    return dyna_create_size_flags(e_size, init_size, DYNAFLAG_NONE);
}

struct dyna *dyna_create_size_flags(size_t e_size, size_t init_size, enum dyna_flags flags)
{
    size_t delem_size = e_size;
    struct dyna *d = calloc(1, sizeof(*d));
    if (d == NULL)
        return NULL;

    d->e_cap = init_size;
    d->e_size = e_size;
    d->flags = flags;
    if (flags & DYNAFLAG_HEAPCOPY)
        delem_size = sizeof(void*);

    d->data = reallocarray(NULL, d->e_cap, delem_size);
    if (d->data == NULL) {
        free(d);
        return NULL;
    }

    return d;
}


void dyna_destroy(struct dyna *dyna)
{
    if (dyna->e_free_fn) {
        for (int i = 0; i < dyna->e_idx; i++) {
            void *elem = dyna_elem(dyna, i);
            dyna->e_free_fn(elem);
            if (dyna->flags & DYNAFLAG_HEAPCOPY)
                free(elem);
        }
    }

    if (dyna->data)
        free(dyna->data);
    if (dyna)
        free(dyna);
}

void dyna_set_free_fn(struct dyna *d, dyna_free_fn fn)
{
    d->e_free_fn = fn;
}

static int dyna_grow(struct dyna *d)
{
    int new_cap = d->e_cap * 2;
    size_t delem_size = (d->flags & DYNAFLAG_HEAPCOPY) ? sizeof(void*) : d->e_size;
    void *new_data = reallocarray(d->data, new_cap, delem_size);
    if (new_data == NULL)
        return -1;

    d->e_cap = new_cap;
    d->data = new_data;
    return 0;
}

void *dyna_append(struct dyna *d, void *elem)
{
    void *dst = dyna_emplace(d);
    memcpy(dst, elem, d->e_size);
    return dst;
}

void *dyna_emplace(struct dyna *d)
{
    if (d->e_idx >= d->e_cap) {
        int e = dyna_grow(d);
        assert(e == 0);
    }
    void *ptr = dyna_elem_(d, d->e_idx);

    if (d->flags & DYNAFLAG_HEAPCOPY) {
        void *hptr = malloc(d->e_size);
        assert(hptr);

        *(void**)ptr = hptr;
        ptr = hptr;
    }
    d->e_idx++;
    return ptr;
}

void *dyna_elem(const struct dyna *d, int64_t idx)
{
    if (idx >= d->e_idx)
        return NULL;
    void *elemptr = dyna_elem_(d, idx);

    if (d->flags & DYNAFLAG_HEAPCOPY) {
        return *(void**)elemptr;
    }
    return elemptr;
}

