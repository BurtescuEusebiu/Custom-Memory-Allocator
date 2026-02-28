# Memory Allocator

Custom low-level memory management library implementing `malloc`, `calloc`, `realloc`, and `free`.

## Features
- Manual memory management with **heap (`sbrk`)** and **mmap** for large allocations  
- Designed `struct block_meta` to track memory blocks, status, size, and neighbors  
- Supports **8-byte alignment**, block splitting, coalescing, and reuse to reduce fragmentation  
- Preallocation strategy to minimize syscalls and optimize small allocations  
- Handles large allocations safely with `mmap()` and frees memory correctly  

## Usage
- Include `osmem.h` and link `osmem.c` in your C projects  
- Provides standard allocation functions:
  - `os_malloc(size_t size)` – allocate memory  
  - `os_calloc(size_t nmemb, size_t size)` – allocate zero-initialized memory  
  - `os_realloc(void *ptr, size_t size)` – resize existing memory block  
  - `os_free(void *ptr)` – free allocated memory  

## Key Learnings
- Understanding memory alignment and padding for performance and atomicity  
- Reducing external fragmentation with coalescing and best-fit allocation  
- Efficient management of heap and mapped memory regions  
