# Memory Allocator

A custom low-level memory management library implementing `malloc`, `calloc`, `realloc`, and `free` for Linux.  
This project explores manual memory management, heap/mmap interactions, and optimization techniques for allocation efficiency and fragmentation reduction.

---

## Features
- Implements **manual memory allocation functions**:
  - `os_malloc(size_t size)` – allocate memory blocks  
  - `os_calloc(size_t nmemb, size_t size)` – allocate zero-initialized memory  
  - `os_realloc(void *ptr, size_t size)` – resize existing memory blocks  
  - `os_free(void *ptr)` – free allocated memory blocks
- **Heap and mmap-based allocation**:
  - Small allocations handled via `sbrk()`  
  - Large allocations (>=128 KB) handled via `mmap()`  
- **Memory block tracking** using `struct block_meta`:
  - Stores size, status (free/allocated/mmap), and linked list pointers  
  - Enables **block splitting**, **coalescing**, and **best-fit allocation**
- **Memory alignment**:
  - All allocations aligned to 8 bytes for 64-bit systems  
  - Ensures atomicity and efficient memory access
- **Fragmentation reduction**:
  - External fragmentation minimized by merging adjacent free blocks  
  - Internal fragmentation reduced by splitting oversized blocks
- **Heap preallocation**:
  - Allocates a 128 KB chunk on first request to reduce frequent syscalls  
  - Future small allocations served from preallocated heap
- Safe handling of edge cases:
  - `NULL` pointers, zero-size allocations, and block reuse checks  
  - Reallocation expands blocks when possible or moves content safely

---

## Architecture & Implementation
- Maintains a **doubly linked list** of memory blocks (`struct block_meta`)  
- **Best-fit strategy**: chooses the smallest available block that fits the requested size  
- **Coalescing**: merges consecutive free blocks before allocation or reallocation  
- **Splitting**: divides larger free blocks to satisfy smaller allocation requests  
- Supports both **heap-based** and **mmap-based** allocations for flexibility  
- Designed for **robust, efficient, and portable memory management** on Linux

---

## Key Learnings
- Deep understanding of **manual memory management** and OS-level system calls (`sbrk`, `mmap`, `munmap`)  
- Experience with **memory alignment, fragmentation, and block reuse strategies**  
- Practical knowledge of low-level **C pointer manipulation, struct layouts, and linked lists**  
- Optimizing **allocation performance** while ensuring safe memory access

---
