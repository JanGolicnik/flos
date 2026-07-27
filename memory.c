#define FLOS_MEMORY
#include "base.c"

struct {
    BumpAllocator _frame;
    Allocator* frame;
    Allocator* stable;
} memory;

void memory_init(void) {
    memory.stable = nullptr;
    memory._frame = bump_allocator_create();
    memory.frame = (Allocator*)&memory._frame;
}
