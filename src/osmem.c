// SPDX-License-Identifier: BSD-3-Clause

#include "osmem.h"

#include <limits.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "block_meta.h"

#define SIZE_MAX 4294967295
#define PAGE_SIZE (4 * 1024)
#define MMAP_THRESHOLD (128 * 1024)
#define PREALLOC_SIZE (128 * 1024)
struct prealloc_block {
	void *start;
	size_t size;
	size_t used;
};

struct block_meta *list;
struct prealloc_block prealloc = {NULL, 0, 0};

size_t aligned_size(size_t size) { return (size + 7) & ~7; }

void *os_malloc(size_t size)
{
	if (size == 0)
		return NULL;

	size_t aligned_payload_size = aligned_size(size);

	size_t total_size = sizeof(struct block_meta) + aligned_payload_size;

	struct block_meta *current = list;
	struct block_meta *best_fit = NULL;

	// coalesce

	while (current && current->next)
		current = current->next;
	while (current) {
		if (current->status == 0 && current->prev && current->prev->status == 0) {
			struct block_meta *prev_block = current->prev;

			current->size += prev_block->size + sizeof(struct block_meta);
			current->prev = prev_block->prev;

			if (prev_block->prev)
				prev_block->prev->next = current;
			else
				list = current;
		} else {
			current = current->prev;
		}
	}

	// search
	current = list;
	while (current) {
		if (current->status == 0 && current->size >= aligned_payload_size) {
			if ((best_fit == NULL) || abs((int)(current->size - aligned_payload_size)) <
										abs((int)(best_fit->size - aligned_payload_size))) {
				best_fit = current;
			}
		}
		current = current->next;
	}
	if (best_fit) {
		current = best_fit;
		char *payload = (char *)(current + 1);
		size_t mis = (size_t)payload % 8;

		if (mis != 0)
			payload += (8 - mis);

		size_t remaining_size = current->size - aligned_payload_size;

		if (remaining_size >= sizeof(struct block_meta) + 8) {
			struct block_meta *new_block =
				(struct block_meta *)((char *)current + sizeof(struct block_meta) +
									aligned_payload_size);
			new_block->size = remaining_size - sizeof(struct block_meta);
			new_block->status = 0;
			new_block->prev = current->prev;
			new_block->next = current;

			if (current->prev)
				current->prev->next = new_block;


			current->prev = new_block;

			if (list == current)
				list = new_block;

			current->size = aligned_payload_size;
		}

		current->status = 1;

		return (void *)payload;
	}
		// expand
		current = list;
		if (current && current->prev == NULL && current->status == 0 &&
			(current->size < aligned_payload_size)) {
			size_t new_size = aligned_payload_size;
			size_t curr_size = current->size;

			if (curr_size < new_size) {
				if (prealloc.size > prealloc.used + aligned_payload_size + 32) {
					current->size = aligned_payload_size;
					current->status = 1;
					prealloc.used += aligned_payload_size + 32;
					return (void *)(current + 1);
				}
				size_t size_diff = new_size - curr_size;
				void *new_p = sbrk(size_diff);

				if (new_p == (void *)-1)
					return NULL;

				current->size = new_size;
			}
			current->status = 1;
			char *payload = (char *)(current + 1);
			size_t mis = (size_t)payload % 8;

			if (mis != 0)
				payload += (8 - mis);

			return (void *)payload;
		}
		if (aligned_payload_size + 32 < MMAP_THRESHOLD) {
			if (prealloc.start == NULL) {
				prealloc.start = sbrk(PREALLOC_SIZE);
				if (prealloc.start == (void *)-1)
					return NULL;

				prealloc.size = PREALLOC_SIZE;
				prealloc.used = 0;
			}
			current = NULL;
			if (prealloc.used + total_size > prealloc.size) {
				current = (struct block_meta *)sbrk(total_size);
			} else {
				current = (struct block_meta *)(prealloc.start + prealloc.used);
				prealloc.used += total_size;
			}
			char *payload = (char *)(current + 1);
			size_t mis = (size_t)payload % 8;

			if (mis != 0)
				payload += (8 - mis);

			current->size = aligned_payload_size;
			current->status = 1;
			current->prev = NULL;
			current->next = list;

			if (list)
				list->prev = current;

			list = current;

			return (void *)payload;
		}
			current = (struct block_meta *)mmap(
				NULL, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

			if (current == MAP_FAILED)
				return NULL;

			char *payload = (char *)(current + 1);
			size_t mis = (size_t)payload % 8;

			if (mis != 0)
				payload += (8 - mis);

			current->size = aligned_payload_size;
			current->status = 2;
			current->prev = NULL;
			current->next = list;

			return (void *)payload;
}
void os_free(void *ptr)
{
	if (ptr == NULL)
		return;

	struct block_meta *current = (struct block_meta *)((char *)ptr - sizeof(struct block_meta));

	if (current->status == 1) {
		current->status = 0;
	} else if (current->status == 2) {
		current->status = 0;
		munmap(current, current->size + sizeof(struct block_meta));
	}
}

void *os_calloc(size_t nmemb, size_t size)
{
	if (nmemb == 0 || size == 0)
		return NULL;

	if (size > SIZE_MAX / nmemb)
		return NULL;

	size_t aligned_payload_size = aligned_size(nmemb * size);
	size_t total_size = sizeof(struct block_meta) + aligned_payload_size;

	struct block_meta *current = list;
	struct block_meta *best_fit = NULL;

	if (aligned_payload_size + 32 < PAGE_SIZE) {
		// coalesce
		while (current && current->next)
			current = current->next;

		while (current) {
			if (current->status == 0 && current->prev && current->prev->status == 0) {
				struct block_meta *prev_block = current->prev;

				current->size += prev_block->size + sizeof(struct block_meta);
				current->prev = prev_block->prev;

				if (prev_block->prev)
					prev_block->prev->next = current;
				else
					list = current;

			} else {
				current = current->prev;
			}
		}

		// search
		current = list;
		while (current) {
			if (current->status == 0 && current->size >= aligned_payload_size)
				if ((best_fit == NULL) || (current->size <= best_fit->size))
					best_fit = current;

			current = current->next;
		}

		if (best_fit) {
			current = best_fit;
			char *payload = (char *)(current + 1);

			memset(payload, 0, aligned_payload_size);

			size_t remaining_size = current->size - aligned_payload_size;

			if (remaining_size >= sizeof(struct block_meta) + 8) {
				struct block_meta *new_block =
					(struct block_meta *)((char *)current + sizeof(struct block_meta) +
										aligned_payload_size);
				new_block->size = remaining_size - sizeof(struct block_meta);
				new_block->status = 0;
				new_block->prev = current->prev;
				new_block->next = current;

				if (current->prev)
					current->prev->next = new_block;

				current->prev = new_block;

				if (list == current)
					list = new_block;

				current->size = aligned_payload_size;
			}

			current->status = 1;
			return (void *)payload;
		}

		// expand
		current = list;
		if (current && current->prev == NULL && current->status == 0 &&
			(current->size < aligned_payload_size)) {
			size_t new_size = aligned_payload_size;
			size_t curr_size = current->size;

			if (curr_size < new_size) {
				if (prealloc.size > prealloc.used + aligned_payload_size + 32) {
					current->size = aligned_payload_size;
					current->status = 1;
					prealloc.used += aligned_payload_size - curr_size;
					memset((void *)(current + 1), 0, aligned_payload_size);
					return (void *)(current + 1);
				}
				size_t size_diff = new_size - curr_size;
				void *new_p = sbrk(size_diff);

				if (new_p == (void *)-1)
					return NULL;

				current->size = new_size;
			}
			current->status = 1;

			char *payload = (char *)(current + 1);
			size_t mis = (size_t)payload % 8;

			if (mis != 0)
				payload += (8 - mis);

			memset((void *)payload, 0, nmemb * size);
			return (void *)payload;
		}

		if (prealloc.start == NULL) {
			prealloc.start = sbrk(PREALLOC_SIZE);
			if (prealloc.start == (void *)-1)
				return NULL;

			prealloc.size = PREALLOC_SIZE;
			prealloc.used = 0;
		}

		if (prealloc.used + total_size > prealloc.size) {
			current = (struct block_meta *)sbrk(total_size);
		} else {
			current = (struct block_meta *)(prealloc.start + prealloc.used);
			prealloc.used += total_size;
		}
	} else {
		current = (struct block_meta *)mmap(NULL, total_size, PROT_READ | PROT_WRITE,
											MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (current == MAP_FAILED)
			return NULL;

		current->status = 2;
	}

	char *payload = (void *)(current + 1);

	memset(payload, 0, nmemb * size);

	current->size = aligned_payload_size;
	if (aligned_payload_size + 32 < PAGE_SIZE) {
		current->status = 1;
		current->prev = NULL;
		current->next = list;

		if (list)
			list->prev = current;
		list = current;
	}

	return (void *)payload;
}

void *os_realloc(void *ptr, size_t size)
{
	if (ptr == NULL)
		return os_malloc(size);

	if (size == 0) {
		os_free(ptr);
		return NULL;
	}

	struct block_meta *current = (struct block_meta *)ptr - 1;
	size_t size_ptr = ((struct block_meta *)ptr - 1)->size;

	if (current->status == 0)
		return NULL;

	size_t aligned_payload_size = aligned_size(size);

	// status == 2
	if (current->status == 2) {
		struct block_meta *best_fit = NULL;
		struct block_meta *next_block = list;

		while (next_block) {
			if (next_block->status == 0 && next_block->size >= aligned_payload_size)
				if (!best_fit || next_block->size < best_fit->size)
					best_fit = next_block;

			next_block = next_block->next;
		}

		if (best_fit) {
			size_t remaining_size = best_fit->size - aligned_payload_size;

			// split
			if (remaining_size >= sizeof(struct block_meta) + 8) {
				struct block_meta *new_block =
					(struct block_meta *)((char *)best_fit + sizeof(struct block_meta) +
										aligned_payload_size);
				new_block->size = remaining_size - sizeof(struct block_meta);
				new_block->status = 0;
				new_block->prev = best_fit;
				new_block->next = best_fit->next;

				if (best_fit->next)
					best_fit->next->prev = new_block;

				best_fit->size = aligned_payload_size;
				best_fit->next = new_block;
			}

			memcpy((void *)(best_fit + 1), ptr, size_ptr);
			munmap(current, current->size + sizeof(struct block_meta));
			best_fit->status = 1;
			return (void *)(best_fit + 1);
		}

		void *new_block = os_malloc(size);

		if (new_block == NULL)
			return NULL;

		if (size_ptr > size)
			size_ptr = size;

		memcpy(new_block, ptr, size_ptr);
		munmap(current, current->size + sizeof(struct block_meta));
		return new_block;
	}

	// status == 1
	if (current->status == 1) {
		if (current->prev == NULL) {
			// expand
			if (prealloc.start != NULL && prealloc.size > prealloc.used + aligned_payload_size) {
				memcpy(prealloc.start + prealloc.used, ptr, size_ptr);
				prealloc.used += (aligned_payload_size - current->size);
				current->size = aligned_payload_size;
				return ptr;
			}
			current = (struct block_meta *)ptr - 1;
			if (current && current->prev == NULL && current->size < aligned_payload_size &&
				aligned_payload_size + 32 < MMAP_THRESHOLD) {
				size_t new_size = aligned_payload_size;
				size_t curr_size = current->size;

				if (curr_size < new_size) {
					size_t size_diff = new_size - curr_size;
					void *new_p = sbrk(size_diff);

					if (new_p == (void *)-1)
						return NULL;

					current->size = new_size;
				}

				char *payload = (char *)(current + 1);

				memcpy(payload, ptr, size_ptr);
				os_free(ptr);
				current->status = 1;
				return (void *)payload;
			}
		}
		// coalesce
		current->status = 0;
		while (current) {
			if (current->status == 0 && current->prev && current->prev->status == 0) {
				struct block_meta *prev_block = current->prev;

				current->size += prev_block->size + sizeof(struct block_meta);
				current->prev = prev_block->prev;

				if (prev_block->prev)
					prev_block->prev->next = current;
				else
					list = current;
			} else {
				current = current->prev;
			}
		}
		current = (struct block_meta *)ptr - 1;
		current->status = 1;
		// split
		if (current->size >= aligned_payload_size) {
			size_t remaining_size = current->size - aligned_payload_size;

			if (remaining_size >= sizeof(struct block_meta) + 8) {
				struct block_meta *new_block =
					(struct block_meta *)((char *)current + sizeof(struct block_meta) +
										aligned_payload_size);
				new_block->size = remaining_size - sizeof(struct block_meta);
				new_block->next = current;
				new_block->prev = current->prev;

				if (current->prev)
					current->prev->next = new_block;

				current->prev = new_block;

				if (list == current)
					list = new_block;

				current->size = aligned_payload_size;
				new_block->status = 0;
			}
			return (void *)(current + 1);
		}
	}

	// search
	current = list;
	struct block_meta *best_fit = NULL;

	while (current) {
		if (current->status == 0 && current->size >= aligned_payload_size)
			if ((best_fit == NULL) || (current->size <= best_fit->size))
				best_fit = current;
		current = current->next;
	}
	if (best_fit) {
		current = best_fit;
		char *payload = (char *)(current + 1);
		size_t mis = (size_t)payload % 8;

		if (mis != 0)
			payload += (8 - mis);

		size_t remaining_size = current->size - aligned_payload_size;

		if (remaining_size >= sizeof(struct block_meta) + 8) {
			struct block_meta *new_block =
				(struct block_meta *)((char *)current + sizeof(struct block_meta) +
									aligned_payload_size);
			new_block->size = remaining_size - sizeof(struct block_meta);
			new_block->status = 0;
			new_block->prev = current->prev;
			new_block->next = current;

			if (current->prev)
				current->prev->next = new_block;

			current->prev = new_block;

			if (list == current)
				list = new_block;

			current->size = aligned_payload_size;
		}

		memmove((void *)payload, ptr, size_ptr);
		os_free(ptr);
		best_fit->status = 1;
		return (void *)payload;
	}
	void *new_ptr = os_malloc(size);

	if (!new_ptr)
		return NULL;

	memcpy(new_ptr, ptr, size_ptr);
	os_free(ptr);
	return new_ptr;
}

