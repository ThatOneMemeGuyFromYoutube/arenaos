/*
 * nt.h - Windows NT 3.1, recreated from memory in portable C.
 *
 * Everything in this project is a from-scratch re-imagining of the
 * 1993 operating system, drawn on top of a plain text terminal.
 * The code is strictly C11 + (POSIX or Win32) so that it compiles
 * with musl, glibc, macOS, MinGW and MSVC via preprocessor directives.
 */
#ifndef NT31_NT_H
#define NT31_NT_H

#include <stdint.h>
#include <stddef.h>

/* ---------------- ANSI 16-color palette ---------------- */
enum {
    C_BLACK = 0, C_RED, C_GREEN, C_YELLOW, C_BLUE, C_MAGENTA, C_CYAN, C_WHITE,
    C_BR_BLACK, C_BR_RED, C_BR_GREEN, C_BR_YELLOW, C_BR_BLUE, C_BR_MAGENTA,
    C_BR_CYAN, C_BR_WHITE
};

/* NT 3.1 chrome colors (gray desktop, navy title bars) */
#define T_DESK      C_BR_BLACK
#define T_DESK_TXT  C_WHITE
#define T_FACE      C_BR_BLACK   /* window face gray */
#define T_FACE_TXT  C_WHITE      /* text on face */
#define T_LT        C_BR_WHITE   /* 3-d light edge   */
#define T_DK        C_BLACK      /* 3-d dark edge    */
#define T_TITLE     C_BLUE       /* active title navy */
#define T_TITLE_TX  C_BR_WHITE
#define T_TITLE_OFF C_BR_BLACK   /* inactive title  */
#define T_TITLE_OFT C_WHITE
#define T_SEL       C_BLUE       /* selection navy   */
#define T_SEL_TX    C_BR_WHITE
#define T_CLI       C_WHITE      /* app client bg    */
#define T_CLI_TX    C_BLACK      /* app client fg    */

/* ---------------- events ---------------- */
enum { EV_NONE = 0, EV_CHAR, EV_KEY, EV_MOUSE, EV_QUIT };
enum {
    K_UP = 0, K_DOWN, K_LEFT, K_RIGHT, K_ENTER, K_ESC, K_TAB, K_BACK, K_DEL,
    K_HOME, K_END, K_PGUP, K_PGDN, K_F1, K_F2, K_F3, K_F4, K_F10
};
#define M_LEFT  0
#define M_MID   1
#define M_RIGHT 2
#define M_MOVE  16    /* mouse move while a button is held */
typedef struct {
    int type;
    int ch;          /* EV_CHAR: printable char */
    int key;         /* EV_KEY */
    int x, y;        /* EV_MOUSE: cell coords */
    int btn;         /* 0..2 press, -1..-3 release, M_MOVE = drag */
    int wheel;       /* EV_MOUSE wheel: +1 up, -1 down */
} ev_t;

/* ---------------- terminal layer (src/term.c) ---------------- */
int  term_init(void);
void term_shutdown(void);
void term_size(int *w, int *h);
int  term_resize(int w, int h);   /* 1 if size changed */
void term_begin(void);
void fill_all(int fg, int bg);
void fill(int x, int y, int w, int h, int fg, int bg);
void txt(int x, int y, const char *s, int fg, int bg);
void txtb(int x, int y, const char *s, int fg, int bg);
void glyph(int x, int y, uint32_t cp, int fg, int bg);
void frame3d(int x, int y, int w, int h, int fg, int bg, int raised);
void term_clip(int x, int y, int w, int h);
void term_clip_none(void);
void flush(void);
int  term_get(ev_t *ev, int timeout_ms); /* 0 = no event */
void term_bell(void);
void msleep(int ms);
long long now_ms(void);
int  is_tty(void);
int  term_shot(const char *path);  /* dump current frame as text */
void set_fast(int f);
int  fast_mode(void);
int  strci(const char *a, const char *b); /* case-insensitive strcmp */

/* ---------------- fake kernel (src/kernel.c) ---------------- */
void kernel_boot(void);
int  mem_free_x10(void);           /* free MB * 10 */
int  mem_total_x10(void);          /* total MB * 10 */
void mem_take(int kb);
void mem_release(int kb);
void proc_add(const char *name);
void proc_drop(const char *name);
int  proc_count(void);
const char *proc_name_at(int i);
int  proc_id_at(int i);
int  proc_find(const char *name);

/* ---------------- window manager / shell (src/win.c) ---------------- */
typedef struct win win_t;
typedef struct app app_t;

struct app {
    const char *name;
    uint32_t glyph;
    int cw, ch;                    /* default client size */
    int bg;                        /* client background */
    void (*start)(win_t *w);
    void (*stop)(win_t *w);
    void (*draw)(win_t *w, int x, int y, int cw, int ch);
    int  (*key)(win_t *w, int key, int ch);   /* 1 = handled */
    int  (*mouse)(win_t *w, int x, int y, int btn, int moved);
    int  (*tick)(win_t *w);                /* once per second, 1 = redraw */
};

struct win {
    int id;
    int x, y, w, h;                /* outer frame, screen cells */
    int mdi;                       /* 1 = Program Manager child */
    int vis, focus, min;
    char title[64];
    const app_t *app;
    void *ctx;
    int param;
    int sel_icon;
    win_t *next;                   /* z-order */
};

void   desktop_init(void);
void   desktop_shutdown(void);
void   desktop_event(const ev_t *ev);
void   desktop_frame(void);
int    desktop_tick(void);
int    desktop_quit(void);         /* handles Ctrl-C; 1 = really quit */
win_t *launch_app(int app_id, int param);
void   close_win(win_t *w);
win_t *win_at(int x, int y);       /* topmost window at cell (incl. PM) */
void   dialog_msg(const char *title, const char *msg);
int    dialog_ask(const char *title, const char *msg); /* 1 = Yes */
void   shutdown_os(void);          /* no return */

/* ---------------- applications (src/apps.c) ---------------- */
enum {
    APP_CONSOLE = 0, APP_NOTEPAD, APP_CALC, APP_CLOCK, APP_MINES,
    APP_CHARMAP, APP_CPL, APP_ABOUT, APP_MAX
};
const app_t *app_by_id(int id);

/* ---------------- icon glyphs (Box Drawing & friends) ---------------- */
#define GLY_PROGRAM  0x25A0u  /* square          */
#define GLY_FAV      0x2605u  /* star            */
#define GLY_START    0x25B6u  /* play triangle   */
#define GLY_SETUPG   0x2699u  /* gear            */
#define GLY_CONSOLE  0x2593u  /* dark block      */
#define GLY_NOTEPAD  0x2592u  /* light block     */
#define GLY_CALC     0x25A6u  /* grid            */
#define GLY_CLOCK    0x25D4u  /* clock face      */
#define GLY_MINES    0x2691u  /* flag            */
#define GLY_CHARMAP  0x0041u  /* 'A'             */
#define GLY_CPL      0x25C6u  /* diamond         */
#define GLY_SETUP    0x25A3u  /* filled square   */
#define GLY_README   0x2592u
#define GLY_MAIL     0x2709u  /* envelope        */
#define GLY_MSDN     0x25C7u
#define GLY_NET      0x25CFu  /* filled circle   */
#define GLY_DISP     0x2597u
#define GLY_PRINTER  0x25A3u

#endif /* NT31_NT_H */
