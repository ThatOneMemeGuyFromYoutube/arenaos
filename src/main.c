/*
 * main.c - entry point and the one true event loop of NT31.
 */
#include "nt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    int fast = 0;
    for (int i = 1; i < argc; i++)
        if (strcmp(argv[i], "--fast") == 0 || strcmp(argv[i], "--no-boot") == 0)
            fast = 1;
    if (getenv("NT31_BOOTFAST")) fast = 1;
    set_fast(fast);

    if (!term_init())
        return 1;

    int W, H;
    term_size(&W, &H);
    if (W < 60 || H < 17) {
        printf("Terminal too small: need at least 60x17 (got %dx%d).\r\n", W, H);
        printf("Make the window bigger and try again.\r\n");
        fflush(stdout);
        msleep(fast ? 100 : 1500);
        term_shutdown();
        return 1;
    }

    kernel_boot();
    desktop_init();

    const char *shot = getenv("NT31_SHOT");
    if (shot) {
        desktop_frame();
        flush();
        term_shot(shot);
        term_shutdown();
        return 0;
    }

    long long last_tick = now_ms();
    for (;;) {
        term_size(&W, &H);
        term_resize(W, H);

        long long t_in = now_ms();
        ev_t ev;
        int got = term_get(&ev, 100);
        long long t_out = now_ms();
        if (t_out - t_in > 250)
            fprintf(stderr, "main: term_get took %lld ms (got=%d type=%d btn=%d) at %.3f\n",
                    t_out - t_in, got, ev.type, ev.btn, now_ms() / 1000.0);
        if (got && ev.type == EV_QUIT) {
            if (!desktop_quit())
                continue;
            shutdown_os(); /* no return */
        }
        if (got)
            desktop_event(&ev);

        long long now = now_ms();
        if (now - last_tick >= 1000) {
            last_tick = now;
            desktop_tick();
        }
        desktop_frame();
        flush();
    }
    return 0;
}
