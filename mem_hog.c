// mem_hog.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int main() {
    const size_t CHUNK_SIZE_MB = 200; // each allocation = 200 MB
    const size_t CHUNK_SIZE = CHUNK_SIZE_MB * 1024 * 1024;
    const int DELAY_SEC = 1; // wait between allocations

    printf(" Memory Hog starting... allocating %zu MB chunks.\n", CHUNK_SIZE_MB);

    void **blocks = NULL;
    size_t block_count = 0;

    while (1) {
        void *p = malloc(CHUNK_SIZE);
        if (!p) {
            printf(" Allocation failed at ~%zu MB total.\n", block_count * CHUNK_SIZE_MB);
            break;
        }

        memset(p, 0, CHUNK_SIZE); // actually touch the memory
        blocks = realloc(blocks, sizeof(void*) * (block_count + 1));
        blocks[block_count++] = p;

        printf(" Allocated %zu MB (total ~%zu MB)\n", CHUNK_SIZE_MB, block_count * CHUNK_SIZE_MB);
        sleep(DELAY_SEC);
    }

    pause(); // keep process alive (so inspector can read it)
    return 0;
}
