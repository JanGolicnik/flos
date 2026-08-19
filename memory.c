#define FLOS_MEMORY
#include "base.c"

struct {
    BumpAllocator _frame;
    Allocator* frame;
    Allocator* stable;
} memory;

void memory_init(void) {
    memory.stable = nullptr;
    memory._frame = (BumpAllocator){ MRW_BUMP_IMPL };
    memory.frame = (Allocator*)&memory._frame;
}
