#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
    FILE *f = fopen("bigfile.bin", "rb");
    if (!f) {
        perror("open");
        return 1;
    }

    // Inflate memory footprint so this process ranks higher
    char *inflate = malloc(200 * 1024 * 1024); // 200MB
    if (inflate) inflate[0] = 1;

    char buffer[4096];
    size_t total = 0;

    while (1) {
        size_t n = fread(buffer, 1, sizeof(buffer), f);
        if (n == 0) break;

        total += n;

        // Strong CPU burn to increase impact score
        for (volatile int x = 0; x < 100000000; x++);

        // Slow enough to observe cache rising
        usleep(5000);
    }

    printf("Read %zu bytes.\n", total);
    fclose(f);
    return 0;
}
