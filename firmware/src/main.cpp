#include <pico/stdio.h>
#include <pico/stdlib.h>
#include <pico/time.h>

#include <cstdio>

int main() {
    stdio_init_all();
    while (true) {
        printf("hello world\n");
        sleep_ms(1000);
    }
}
