#include <colla.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <stdint.h>
#include <time.h>

void* buffers[1024];
int total_buffers=0;

void erase_buffer(int index)
{
	if (index>=0 && index<total_buffers)
	{
		for (int i=index;i<(total_buffers-1);++i)
		{
			buffers[i] = buffers[i+1];
		}
		--total_buffers;
	}
}

int get_random(int n)
{
	return rand()%n; // NOLINT
}

#define SIZE (8*1024*1024)
uint8_t static_heap[SIZE];

int run_test()
{
	srand(time(NULL)); // NOLINT
	colla_init(static_heap,SIZE);
	void* buffer=NULL;
	uint32_t size=0;
	int index=0;
	for (int iter=0;iter<1000;++iter)
	{
		if (!colla_verify()) return 1;
		int action = get_random(4);
		switch (action)
		{
			case 0:
			{
				size = get_random(32768);
				buffer = colla_alloc(size);
				if (!buffer)
				{
					fprintf(stderr, "Failed to allocate buffer\n");
					return 1;
				}
				buffers[total_buffers++] = buffer;
			}
				break;
			case 1:
				if (total_buffers>0)
				{
					index = get_random(total_buffers);
					buffer = buffers[index];
					size=get_random(32768);
					buffer = colla_realloc(buffer, size);
					if (!buffer)
					{
						fprintf(stderr, "Failed to reallocate buffer\n");
						return 1;
					}
					buffers[index] = buffer;
				}
				break;
			case 2:
				if (total_buffers>0)
				{
					index = get_random(total_buffers);
					colla_free(buffers[index]);
					erase_buffer(index);
				}
				break;
			case 3:
			{
				/* Small buffers */
				size = get_random(64);
				buffer = colla_alloc(size);
				buffers[total_buffers++] = buffer;
			}
				break;
			default:
				break;
		}
	}
	printf("%d Empty blocks\n",colla_empty_blocks());
	printf("Freeing %d remaining blocks.\n",total_buffers);
	for (index=0;index<total_buffers;++index)
	{
		colla_free(buffers[index]);
	}
	if (!colla_verify()) return 1;
	colla_print_stats();
	printf("All tests completed successfully\n");
	return 0;
}

int main(int argc ,char* argv[])
{
	return run_test();
}
