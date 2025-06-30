#include "colla.h"
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#pragma pack(1)

static uint8_t *colla_heap = NULL;
static size_t colla_heap_size = 0;

// Used for diagnostics tests
typedef struct allocated_block
{
	uint8_t *ptr;
	uint32_t size;
	uint32_t offset;
} AllocatedBlock;

typedef struct empty_block
{
	uint32_t size;
	uint32_t next;
} EmptyBlock;

/*
   Variable Header
 Given a buffer with a variable length header before it, read up to 4 bytes
 stopping when the MSB of the byte is 0
 So, if we have a user_size buffer of 17281 bytes
 This is   0100001110000001 in binary.  We divide it into 7 bit groups:
                0000001 0000111 0000001
 and then store the 3 bytes so that
 [ptr-1] = 10000001  (Note the MSB 1 to indicate more bytes to come)
 [ptr-2] = 10000111  (Same...)
 [ptr-3] = 00000001  (MSB=0 - No more bytes)
*/

// Given a pointer to a user buffer, decode both the user buffer size and the header size
static void decode_size(void *ptr, uint32_t *header_size, uint32_t *user_size)
{
	*header_size = 0;
	*user_size = 0;
	uint8_t *byte_ptr = (uint8_t *) ptr;
	for (int i = 0; i < 4; ++i)
	{
		++(*header_size);
		uint8_t byte = *(--byte_ptr);
		*user_size <<= 7;
		*user_size |= (byte & 0x7F);
		if ((byte & 0x80) == 0) break;
	}
}

// Calculate the required number of bytes to encode the size
static uint32_t get_header_size(uint32_t user_size)
{
	if (user_size < (1 << 7)) return 1;
	if (user_size < (1 << 14)) return 2;
	if (user_size < (1 << 21)) return 3;
	return 4;
}

// Given the user_size, encode the size into the prefix header
static void fill_header(void *ptr, uint32_t user_size)
{
	uint8_t *byte_ptr = (uint8_t *) ptr;
	uint32_t n = get_header_size(user_size);
	for (uint32_t i = 0; i < n; ++i)
	{
		uint8_t byte = (user_size >> 7 * (n - i - 1)) & 0x7F;
		if (i != (n - 1)) byte |= 0x80;
		*(--byte_ptr) = byte;
	}
}


#ifdef _DIAGNOSTICS_

AllocatedBlock diag_blocks[65536];
int total_diag_blocks = 0;

static void erase_diag_block(int index)
{
	if (index<0 || index>=total_diag_blocks) return;
	--total_diag_blocks;
	for (int i=index;i<total_diag_blocks;++i)
	{
		diag_blocks[i]=diag_blocks[i+1];
	}
}
#endif
uint32_t colla_first_empty = 0;
uint32_t colla_total_allocated_bytes = 0;
uint32_t colla_total_allocated_blocks = 0;

static EmptyBlock *block(uint32_t offset)
{
	return (EmptyBlock *) (&colla_heap[offset]);
}

static void unite_blocks(uint32_t offset)
{
	if (offset < colla_heap_size)
	{
		EmptyBlock* b = block(offset);
		if (b->next < colla_heap_size)
		{
			if ((offset + b->size) == b->next)
			{
				EmptyBlock* nb = block(b->next);
				b->next = nb->next;
				b->size += nb->size;
			}
		}
	}
}

static void add_empty_block(uint32_t offset, uint32_t size)
{
	block(offset)->size = size;
	const uint32_t limit = colla_heap_size;
	uint32_t last = colla_first_empty;
	if (last > offset)
	{
		block(offset)->next = colla_first_empty;
		colla_first_empty = offset;
		unite_blocks(offset);
	} else
	{
		while (last < limit)
		{
			uint32_t next = block(last)->next;
			if (next > offset)
			{
				block(offset)->next = next;
				block(last)->next = offset;
				unite_blocks(offset);
				unite_blocks(last);
				break;
			}
			last = next;
		}
	}
}


static int get_empty_block_count()
{
	int res = 0;
	uint32_t offset = colla_first_empty;
	while (offset < colla_heap_size)
	{
		res++;
		offset = block(offset)->next;
	}
	return res;
}


int colla_verify()
{
#ifdef _DIAGNOSTICS_
    size_t total=0;
	uint8_t* memory=(uint8_t*)malloc(colla_heap_size);
	memset(memory,0,colla_heap_size);
	for (int i=0;i<total_diag_blocks;++i)
	{
		for (uint32_t j=0;j<diag_blocks[i].size;++j)
		{
			memory[diag_blocks[i].offset+j]=1;
			++total;
		}
	}
    uint32_t offset = colla_first_empty;
    while (offset < colla_heap_size)
    {
        EmptyBlock* b = block(offset);
        if (b->next < offset)
        {
        	free(memory);
            return 0;
        }
        for (uint32_t i=0;i<b->size;++i)
        {
            memory[offset+i]=1;
            ++total;
        }
        offset = b->next;
    }
	if (total < colla_heap_size)
	{
      	free(memory);
		return 0;
	}
	for (int i=0;i<colla_heap_size;++i)
	{
		if (memory[i]==0)
		{
			free(memory);
			return 0;
		}
	}
#endif
	return 1;
}

static uint32_t extract_empty_block(uint32_t size)
{
	uint32_t last = colla_heap_size;
	uint32_t offset = colla_first_empty;
	while (offset < colla_heap_size && block(offset)->size < size)
	{
		last = offset;
		offset = block(offset)->next;
	}
	if (offset >= colla_heap_size) return offset;
	uint32_t next = block(offset)->next;
	if (last < colla_heap_size)
	{
		block(last)->next = next;
	} else
	{
		colla_first_empty = next;
	}
	return offset;
}

void*	colla_alloc(size_t user_size)
{
	if (!colla_heap) return NULL;
	const uint32_t header_size = get_header_size(user_size);
	uint32_t size = header_size + user_size;
	if (size < sizeof(EmptyBlock))
	{
		size = sizeof(EmptyBlock);
		user_size = size - header_size;
	}
	uint32_t offset = extract_empty_block(size);
	if (offset >= colla_heap_size)
	{
		// Out of memory
		return NULL;
	}
	EmptyBlock* empty_block = block(offset);
	const uint32_t left = empty_block->size - size;
	const uint32_t left_offset = offset + size;
	if (left < sizeof(EmptyBlock))
	{
		size = empty_block->size;
		user_size = size - header_size;
	} else
	{
		add_empty_block(left_offset, left);
		empty_block->size = size;
	}
	++colla_total_allocated_blocks;
	colla_total_allocated_bytes += size;
	uint8_t *res = (uint8_t *)empty_block;
	res += header_size;
	fill_header(res, user_size);
#ifdef _DIAGNOSTICS_
	{
		AllocatedBlock* b=&diag_blocks[total_diag_blocks++];
		b->ptr=res;
		b->size=size;
		b->offset=offset;
	}
#endif
	return res;
}

void colla_free(void* ptr)
{
	uint32_t user_size, header_size;
	decode_size(ptr, &header_size, &user_size);
	uint8_t *byte_ptr = (uint8_t *) ptr;
	uint32_t offset = (byte_ptr - colla_heap);
	offset -= header_size;
	uint32_t size = header_size + user_size;
	if (offset >= colla_heap_size)
	{
#ifdef _DIAGNOSTICS_
            fprintf(stderr, "Releasing invalid block\n");
#endif
		return;
	}
#ifdef _DIAGNOSTICS_
        {
            const int n = total_diag_blocks;
            int selected = n;
            for (int i=0;i<n;++i)
            {
                if (diag_blocks[i].offset == offset)
                {
                    selected=i;
                    break;
                }
            }
            if (selected == n)
            {
            	fprintf(stderr, "Releasing invalid block\n");
                return;
            }
            if (diag_blocks[selected].size != size)
            {
            	fprintf(stderr, "Releasing invalid block size\n");
            }
			erase_diag_block(selected);
        }
#endif
	colla_total_allocated_bytes -= size;
	--colla_total_allocated_blocks;
	add_empty_block(offset, size);
}


void colla_init(void *heap, size_t size)
{
	colla_heap_size = size;
	colla_heap = heap;
	colla_first_empty = 0;
	EmptyBlock *b = block(0);
	b->size = size;
	b->next = size;
}

void	colla_deinit()
{
	colla_heap_size = 0;
	colla_heap = NULL;
}

void*	colla_realloc(void* ptr, size_t size)
{
	if (!colla_heap) return NULL;
	void* new_block = colla_alloc(size);
	uint32_t header_size, old_size;
	decode_size(ptr, &header_size, &old_size);
	memcpy(new_block, ptr, size>old_size?old_size:size);
	colla_free(ptr);
	return new_block;
}

void	colla_print_stats()
{
	printf("Total blocks: %d\nTotal Bytes: %d\n",
			colla_total_allocated_blocks,
			colla_total_allocated_bytes);
}

int	colla_empty_blocks()
{
	return get_empty_block_count();
}

