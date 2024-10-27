#include "stack.h"

#include <assert.h>
#include <string.h>

void stack_push(struct stack *stack, void *val)
{
    stack->e_idx++;
    assert(stack->e_idx < stack->e_cap);
    void *dst = stack_top(stack);
    memcpy(dst, val, stack->e_size);
}

void *stack_top(struct stack *stack)
{
    return stack->buff + (stack->e_idx * stack->e_size);
}

void stack_pop(struct stack *stack)
{
    assert(stack->e_idx > 0);
    stack->e_idx--;
}

int stack_count(struct stack *stack)
{
    return stack->e_idx;
}

