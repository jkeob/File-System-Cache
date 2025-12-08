#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
    // Inflate memory so this process shows up clearly in Top 30
    char *inflate = malloc(200 * 1024 * 1024); // 200MB RSS
    if (inflate) inflate[0] = 1;

    FILE *f = fopen("bigfile.bin", "rb");
    if (!f) {
        perror("open bigfile.bin");
        return 1;
    }

    char buffer[4096];
    size_t total = 0;

    while (1) {
        size_t n = fread(buffer, 1, sizeof(buffer), f);
        if (n == 0) break;

        total += n;

        // Mild CPU burn to push ranking score higher
        for (volatile int x = 0; x < 10000000; x++);

        // Pause slightly so cache residency increases slowly and visibly
        usleep(5000);
    }

    printf("Finished reading %zu bytes.\n", total);
    fclose(f);
    return 0;
}
