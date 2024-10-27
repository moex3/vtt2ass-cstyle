#ifndef _VTT2ASS_STACK_H
#define _VTT2ASS_STACK_H

#include <stddef.h>
#include <stdint.h>

#define stack_init(var, v_item_size, v_max_size) \
    char stackbuff_##var[v_item_size * v_max_size]; \
    struct stack var = { .buff = stackbuff_##var, .e_cap = v_max_size, .e_size = v_item_size }; 

struct stack {
    char *buff;
    size_t e_size; /* size of 1 element */
    int64_t e_cap; /* The maximum amount of elements that can be stored */
    int64_t e_idx; /* The current index of the next location to place elements in */
};

void stack_push(struct stack *stack, void *val);
void *stack_top(struct stack *stack);
void stack_pop(struct stack *stack);
int stack_count(struct stack *stack);

#endif /* _VTT2ASS_STACK_H */
