/*
 * kernel.c - a tiny fake NT kernel: boot loader screen, kernel init log,
 * splash, memory accounting, and a process table for flavor.
 */
#include "nt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TOTAL_MB 16

static int used_kb;
static int pid_next;

typedef struct {
    int id;
    char name[24];
    int on;
} proc_t;
static proc_t procs[32];
static int nproc;

void proc_add(const char *name)
{
    if (nproc >= 32) return;
    procs[nproc].id = pid_next;
    snprintf(procs[nproc].name, sizeof procs[nproc].name, "%s", name);
    procs[nproc].on = 1;
    nproc++;
}

void proc_drop(const char *name)
{
    for (int i = 0; i < nproc; i++) {
        if (procs[i].on && strci(procs[i].name, name) == 0) {
            procs[i].on = 0;
            return;
        }
    }
}

int proc_find(const char *name)
{
    for (int i = 0; i < nproc; i++)
        if (procs[i].on && strci(procs[i].name, name) == 0)
            return i;
    return -1;
}

int proc_count(void)
{
    int n = 0;
    for (int i = 0; i < nproc; i++) n += procs[i].on;
    return n;
}

const char *proc_name_at(int i)
{
    for (int k = 0; k < nproc; k++)
        if (procs[k].on && --i < 0)
            return procs[k].name;
    return 0;
}

int proc_id_at(int i)
{
    for (int k = 0; k < nproc; k++)
        if (procs[k].on && --i < 0)
            return procs[k].id;
    return 0;
}

void mem_take(int kb) { used_kb += kb; if (used_kb > TOTAL_MB * 1024) used_kb = TOTAL_MB * 1024; }
void mem_release(int kb) { used_kb -= kb; if (used_kb < 0) used_kb = 0; }

int mem_total_x10(void) { return TOTAL_MB * 10; }
int mem_free_x10(void)
{
    int free_kb = TOTAL_MB * 1024 - used_kb;
    return free_kb * 10 / 1024;
}

static void bl(const char *s)
{
    printf("%s\r\n", s);
    fflush(stdout);
}

/* the boot sequence, as best as memory allows */
void kernel_boot(void)
{
    srand((unsigned)(now_ms() / 1000 ^ 0x9321u));
    used_kb = 4300;
    pid_next = 4;
    procs[0].id = 4;  snprintf(procs[0].name, 24, "System");   procs[0].on = 1;
    procs[1].id = 8;  snprintf(procs[1].name, 24, "Idle");     procs[1].on = 1;
    procs[2].id = 12; snprintf(procs[2].name, 24, "Win386");   procs[2].on = 1;
    procs[3].id = 16; snprintf(procs[3].name, 24, "csrss");    procs[3].on = 1;
    procs[4].id = 20; snprintf(procs[4].name, 24, "services"); procs[4].on = 1;
    procs[5].id = 24; snprintf(procs[5].name, 24, "Pm");       procs[5].on = 1;
    nproc = 6;
    pid_next = 28;

    int f = fast_mode() ? 0 : 1;
    fill_all(C_BLACK, C_BLACK);
    flush();

    bl("Microsoft(R) Windows NT(TM) 3.1");
    if (f) msleep(350);
    bl("");
    bl("RAM detected ......... 16.0 MB");
    if (f) msleep(120);
    bl("Video ................ VGA  640x480  16 colors");
    if (f) msleep(120);
    bl("Keyboard ............. US");
    if (f) msleep(120);
    bl("Mouse ................ Microsoft Bus mouse");
    if (f) msleep(250);
    bl("");
    bl("Starting Windows NT...");
    if (f) msleep(400);
    bl("");
    bl("Object Manager ....... ok");
    if (f) msleep(80);
    bl("Process Manager ...... ok");
    if (f) msleep(80);
    bl("Memory Manager ....... ok");
    if (f) msleep(80);
    bl("I/O Manager .......... ok");
    if (f) msleep(80);
    bl("Plug and Play ........ ok");
    if (f) msleep(80);
    bl("Registry ............. ok");
    if (f) msleep(200);
    bl("");
    bl("Loading driver: VDMDD    ok");
    if (f) msleep(80);
    bl("Loading driver: KEYB     ok");
    if (f) msleep(80);
    bl("Loading driver: MOUSE    ok");
    if (f) msleep(80);
    bl("Loading driver: VIDEOPRT ok");
    if (f) msleep(80);
    bl("Loading driver: WIN32K   ok");
    if (f) msleep(200);
    bl("");
    bl("Win32 Subsystem ...... ok");
    if (f) msleep(300);

    /* navy splash */
    fill_all(T_TITLE_TX, T_TITLE);
    flush();
    term_bell();
    const char *art[] = {
        " ____   ____  ____  _   _ ____  __  __ _   _ _  ___ _  __   ",
        "|  _ \\ / ___||  _ \\| | | |  _ \\|  \\/  | | | | |/ / | |/ /   ",
        "| | | | |  _ | |_) | |_| | | | | |\\/| | | | | ' /| | ' /    ",
        "| |_| | |_| ||  _ <|  _  | |_| | |  | | |_| | . \\| | . \\    ",
        "|_____/ \\____||_| \\_\\_| |_|\\____/|_|  |_|\\___/|_|\\_\\_|_|\\_\\  ",
    };
    int sw, sh;
    term_size(&sw, &sh);
    (void)sh;
    for (int i = 0; i < 5; i++)
        txtb((sw - 54) / 2, 6 + i, art[i], T_TITLE_TX, T_TITLE);
    txt((sw - 11) / 2, 13, "Version 3.1", T_TITLE_TX, T_TITLE);
    txt((sw - 38) / 2, 15, "Copyright (c) Microsoft Corp. 1981-1993.", T_TITLE_TX, T_TITLE);
    flush();
    msleep(fast_mode() ? 150 : 1500);

    fill_all(T_DESK_TXT, T_DESK);
    flush();
}
