#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#define CHUNK_SIZE (1024 * 1024)     // 1 MB chunks
#define CPU_SPIN   50000000          // Controls CPU hog — big number = 100% CPU

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <file>\n", argv[0]);
        return 1;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    // Tell kernel NOT to aggressively preload everything
    posix_fadvise(fd, 0, 0, POSIX_FADV_RANDOM);

    printf("Starting slow demo reader...\n");

    uint8_t *buf = malloc(CHUNK_SIZE);
    if (!buf) {
        perror("malloc");
        close(fd);
        return 1;
    }

    while (1) {
        lseek(fd, 0, SEEK_SET);

        ssize_t n;
        while ((n = read(fd, buf, CHUNK_SIZE)) > 0) {

            // CPU-heavy work to spike CPU usage
            volatile double x = 1.0;
            for (long i = 0; i < CPU_SPIN; i++) {
                x = x * 1.0000001;
            }

            // Delay slows caching (prevents instant 100%)
            usleep(5000);   // 50ms pause per MB
        }
    }

    free(buf);
    close(fd);
    return 0;
}
