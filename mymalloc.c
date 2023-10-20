/**
 * Homework 4 - CS 446/646
 * File: mymalloc.c
 * Author: JoJo Petersky
 * Last Revision: 2023-10-19
 */
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <string.h>
#include <sys/mman.h>

#define MBLOCK_HEADER_SZ offsetof(mblock_t, payload)
#define HEAP_CONTRACT_THRESHOLD 4096

typedef struct _mblock_t {
    struct _mblock_t * prev;
    struct _mblock_t * next;
    size_t size;
    size_t end_size;
    int status; // 1: Allocated, 0: Free
    void * payload;
} mblock_t;

typedef struct _mlist_t {
    mblock_t * head;
} mlist_t;

// Global memory list
static mlist_t memList = {NULL};

mblock_t * findLastMemlistBlock();
mblock_t * findFreeBlockOfSize(size_t size);
void splitBlockAtSize(mblock_t * block, size_t newSize);
void coallesceBlockPrev(mblock_t * freedBlock);
void coallesceBlockNext(mblock_t * freedBlock);
mblock_t * growHeapBySize(size_t size);
void * mymalloc(size_t size);
void * mycalloc(size_t num, size_t size);
void * myrealloc(void *ptr, size_t size);
void myfree(void * ptr);
size_t alignToWord(size_t size);
void handleError(const char* errorMessage);
void printMemList(const mblock_t* headptr);

void test_small_allocations();
long get_total_ram();
void test_large_allocations();
void test_alternate_allocations();
void test_repeated_reallocation();
void test_boundary();
void test_corruption();
void test_memory_leaks();
void test_out_of_memory();
void test_alignment();

int main(int argc, char* argv[]) {
    printMemList(memList.head);
    void * p1 = mymalloc(10);
    void * p2 = mymalloc(100);
    void * p3 = mymalloc(200);
    void * p4 = mymalloc(500);
    myfree(p3); p3 = NULL;
    myfree(p2); p2 = NULL;
    void * p5 = mymalloc(150);
    void * p6 = mymalloc(500);
    myfree(p4); p4 = NULL;
    myfree(p5); p5 = NULL;
    myfree(p6); p6 = NULL;
    myfree(p1); p1 = NULL;
    printMemList(memList.head);
    test_small_allocations();
    printMemList(memList.head);
    test_large_allocations();
    printMemList(memList.head);
    test_alternate_allocations();
    printMemList(memList.head);
    test_repeated_reallocation();
    printMemList(memList.head);
    test_boundary();
    printMemList(memList.head);
    test_corruption();
    printMemList(memList.head);
    test_memory_leaks();
    printMemList(memList.head);
    test_out_of_memory();
    printMemList(memList.head);
    test_alignment();
    
    return 0;
}

mblock_t * findLastMemlistBlock() {
    mblock_t *ptr = memList.head;
    if (!ptr) return NULL;
    while (ptr->next) {
        ptr = ptr->next;
    }
    return ptr;
}

mblock_t * findFreeBlockOfSize(size_t size) {
    mblock_t *ptr = memList.head;
    while (ptr) {
        if (ptr->status == 0 && ptr->size >= size) {
            return ptr;
        }
        ptr = ptr->next;
    }
    return NULL;
}

void splitBlockAtSize(mblock_t * block, size_t newSize) {
    block->end_size = newSize;
    mblock_t *newBlock = (mblock_t *)((char *)block->payload + newSize + sizeof(size_t));  // Adjusted for boundary tag
    newBlock->end_size = newBlock->size;
    newBlock->size = block->size - newSize - MBLOCK_HEADER_SZ;
    newBlock->status = 0; // free
    newBlock->next = block->next;
    newBlock->prev = block;

    block->next = newBlock;
    block->size = newSize;
}

void coallesceBlockPrev(mblock_t * freedBlock) {
    if (freedBlock->prev && freedBlock->prev->status == 0) {
        freedBlock->prev->size += MBLOCK_HEADER_SZ + freedBlock->size;
        freedBlock->prev->next = freedBlock->next;
        if (freedBlock->next) {
            freedBlock->next->prev = freedBlock->prev;
        }
    }
}

void coallesceBlockNext(mblock_t * freedBlock) {
    if (freedBlock->next && freedBlock->next->status == 0) {
        freedBlock->size += MBLOCK_HEADER_SZ + freedBlock->next->size;
        freedBlock->next = freedBlock->next->next;
        if (freedBlock->next) {
            freedBlock->next->prev = freedBlock;
        }
    }
}

mblock_t * growHeapBySize(size_t size) {
    size_t actualSize = size + MBLOCK_HEADER_SZ + sizeof(size_t);  // Including boundary tag size
    mblock_t *block = (mblock_t *)mmap(NULL, actualSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (block == MAP_FAILED) {
        handleError("mmap failed.");
        return NULL;
    }

    block->size = size;
    block->status = 0; // free
    block->next = NULL;
    block->prev = findLastMemlistBlock();

    if (!memList.head) {
        memList.head = block;
    } else if (block->prev) {
        block->prev->next = block;
    }

    block->end_size = size;
        
    return block;
}

void * mymalloc(size_t size) {
    size = alignToWord(size);
    if (size <= 0) {
        return NULL;
    }

    mblock_t *block = findFreeBlockOfSize(size);
    if (!block) {
        block = growHeapBySize(size);
        if (!block) {
            return NULL; // Allocation failed
        }
    }
    // Check that the size of the block can accommodate the request
    if (size > block->size) {
        handleError("Allocation size exceeds mapped memory.");
        return NULL;
    }

    if (block->size > size + MBLOCK_HEADER_SZ) {
        splitBlockAtSize(block, size);
    }

    block->status = 1; // allocated
    // Set the boundary tag
    size_t *boundaryTag = (size_t *)((char *)block + MBLOCK_HEADER_SZ + block->size);
    *boundaryTag = block->size;
    
    return block->payload;
}


void * mycalloc(size_t num, size_t size) {
    size_t total_size = num * size;
    void *ptr = mymalloc(total_size);
    if (ptr) {
        memset(ptr, 0, total_size);
    }
    return ptr;
}

void * myrealloc(void *ptr, size_t size) {
    if (!ptr) {
        return mymalloc(size);
    }

    mblock_t *block = (mblock_t *)(((char *)ptr) - MBLOCK_HEADER_SZ);
    
    // If the size is the same, just return the original pointer
    if (block->size == size) {
        return ptr;
    }
    
    // If the new size is smaller, shrink the block and possibly split it
    if (block->size > size) {
        splitBlockAtSize(block, size);
        return ptr;
    }
    
    if (size <= 0) {
        handleError("Invalid size requested for reallocation.");
        return NULL;
    }
    // Check if the next block is free and large enough
    if (block->next && block->next->status == 0 && block->size + block->next->size + MBLOCK_HEADER_SZ >= size) {
        // Merge blocks
        block->size += MBLOCK_HEADER_SZ + block->next->size;
        block->next = block->next->next;
        if (block->next) {
            block->next->prev = block;
        }
        
        // Possibly split after merging
        if (block->size > size + MBLOCK_HEADER_SZ) {
            splitBlockAtSize(block, size);
        }
        return ptr;
    }
    
    // If we reached here, we need to allocate a new block, copy data, and free the old block
    void *new_ptr = mymalloc(size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, block->size);  // Copy old data to new location
        myfree(ptr);                        // Free old block
    }
    
    return new_ptr;
}

void myfree(void * ptr) {
    if (!ptr) {
        return;
    }
    
    mblock_t *block = (mblock_t *)(((char *)ptr) - MBLOCK_HEADER_SZ);
    // Error handling: check if the boundary tags match
    size_t *boundaryTag = (size_t *)((char *)block + MBLOCK_HEADER_SZ + block->size);
    if (block->size != *boundaryTag) {
        handleError("Invalid pointer or block corrupted. Terminating.");
        abort();
    }

    if(block->status == 0) {
        handleError("Attempted to free an already freed block.");
        return;
    }

    coallesceBlockPrev(block);
    coallesceBlockNext(block);
    mblock_t * lastBlock = findLastMemlistBlock();
    if (block == lastBlock && block->status == 0 && block->size > HEAP_CONTRACT_THRESHOLD) {
        munmap(block, block->size + MBLOCK_HEADER_SZ);  // Using munmap to free the memory
        if (block->prev) {
            block->prev->next = NULL;
        } else {
            memList.head = NULL;
        }
    }
}

size_t alignToWord(size_t size) {
    return (size + sizeof(void*) - 1) & ~(sizeof(void*) - 1);
}

void handleError(const char* errorMessage) {
    fprintf(stderr, "Error: %s\n", errorMessage);
    // Uncomment the line below to terminate on errors
    // abort();
}

void printMemList(const mblock_t* headptr) {
    const mblock_t* p = headptr;
  
    size_t i = 0;
    while(p != NULL) {
        printf("[%ld] p: %p\n", i, p);
        printf("[%ld] p->size: %ld\n", i, p->size);
        printf("[%ld] p->status: %s\n", i, p->status > 0 ? "allocated" : "free");
        printf("[%ld] p->prev: %p\n", i, p->prev);
        printf("[%ld] p->next: %p\n", i, p->next);
        printf("___________________________\n");
        ++i;
        p = p->next;
    }
    printf("===========================\n");
}
// Test small allocations with consecutive allocations of size 1
void test_small_allocations() {
    const int alloc_count = 1000;
    void *allocs[alloc_count];
    for (int i = 0; i < alloc_count; ++i) {
        allocs[i] = mymalloc(1);
        if (!allocs[i]) {
            printf("Failed to allocate block number: %d\n", i + 1);
            break;
        }
    }
    // Free the allocated memory
    for (int i = 0; i < alloc_count; ++i) {
        if (allocs[i]) {
            myfree(allocs[i]);
        }
    }
    printf("Small allocations test done.\n");
}
// Helper function to send RAM total to test_large_allocations()
long get_total_ram() {
    FILE* meminfo = fopen("/proc/meminfo", "r");
    if (!meminfo) {
        perror("fopen");
        return -1;
    }

    char line[256];
    while (fgets(line, sizeof(line), meminfo)) {
        long totalram;
        if (sscanf(line, "MemTotal: %ld kB", &totalram) == 1) {
            fclose(meminfo);
            return totalram * 1024; // Convert to bytes
        }
    }
    
    fclose(meminfo);
    return -1; // If we couldn't find the MemTotal line
}
// Test large allocations by allocating larger chunks of memory close to the system's limit
void test_large_allocations() {
    long total_ram = get_total_ram();
    if (total_ram < 0) {
        printf("Failed to get total RAM.\n");
        return;
    }
    // Calculate close to the system's total RAM while leaving some room
    unsigned long target_alloc = total_ram - (1UL << 28); // Subtracting 256MB for safety

    void *p = mymalloc(target_alloc);
    if (p) {
        printf("Large allocation of %lu bytes succeeded.\n", target_alloc);
        myfree(p);
    } else {
        printf("Large allocation of %lu bytes failed.\n", target_alloc);
    }
}
// Test alternating allocation and deallocation through a few blocks
void test_alternate_allocations() {
    void *p1 = mymalloc(10);
    myfree(p1);
    void *p2 = mymalloc(20);
    myfree(p2);
    void *p3 = mymalloc(30);
    myfree(p3);
    printf("Alternate allocations test done.\n");
}
// Test repeated reallocation by reallocating a block to both larger and smaller sizes
void test_repeated_reallocation() {
    void *p = mymalloc(10);
    for (int i = 0; i < 100; i++) {
        size_t newSize = (i % 2 == 0) ? 10 * (i + 1) : 10;
        p = myrealloc(p, newSize);
        if (!p) {
            printf("Reallocation failed at iteration: %d\n", i + 1);
            break;
        }
    }
    myfree(p);
    printf("Repeated reallocation test done.\n");
}
// Test boundary by allocating just one byte more than a threshold then deallocate
void test_boundary() {
    void *p = mymalloc(HEAP_CONTRACT_THRESHOLD + 1);
    myfree(p);
    printf("Boundary test done.\n");
}
// Test for corruption by freeing a pointer that wasn't returned by mymalloc/already been freed
void test_corruption() {
    int x;
    myfree(&x);
    void *p = mymalloc(10);
    myfree(p);
    myfree(p);
    printf("Corruption test done.\n");
}
// Allocate several blocks and deallocate some of them using valgrind to check for memory leaks
void test_memory_leaks() {
    for (int i = 0; i < 1000; ++i) {
        void *p = mymalloc(10);
        if (i % 2 == 0) {
            myfree(p);
        }
    }
    printf("Memory leaks test done (check with valgrind).\n");
}

// Allocate memory in a loop until allocation fails to test the allocator in out-of-memory scenarios
void test_out_of_memory() {
    while (1) {
        void *p = mymalloc(1000);
        if (!p) {
            printf("Out of memory!\n");
            break;
        }
    }
    printf("Out of memory test done.\n");
}
// Test alignment to ensure that the memory returned by mymalloc is suitably aligned
void test_alignment() {
    void *p = mymalloc(10);
    if ((uintptr_t)p % sizeof(void*) == 0) {
        printf("Memory is aligned.\n");
    } else {
        printf("Memory is not aligned!\n");
    }
    myfree(p);
}
