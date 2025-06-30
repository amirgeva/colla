#pragma once

#include <stddef.h>

// Before calling any allocations, call init to set the heap
// The library does not take ownership and will not free this heap
// since there's no way of knowing how it got allocated.
void	colla_init(void* heap, size_t size);

// Clear and remove the heap.  Any remaining allocations are lost
void	colla_deinit();

// Allocate size bytes
void*	colla_alloc(size_t size);

// Free a block
void	colla_free(void* ptr);

// Allocate a new size bytes block and copy existing data to new block
// If the new block is smaller, some data is lost
// If the new block is larger, extra memory is uninitialized
void*	colla_realloc(void* ptr, size_t size);

// Print statistics about currect state
void	colla_print_stats();

// Returns 0 if the internal state is inconsistent
// This is only meaningful is the CMake DIAGNOSTICS option is active
int		colla_verify();

// Get number of empty blocks.  Indication of memory fragmentation
int		colla_empty_blocks();

