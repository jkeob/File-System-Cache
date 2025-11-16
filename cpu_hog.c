#include <stdio.h>

int main(void) {
    while (1) {
        // simple busy-work
        double x = 1.0001;
        for (int i = 0; i < 1000000; i++)
            x *= 1.000001;
    }
    return 0;
}
