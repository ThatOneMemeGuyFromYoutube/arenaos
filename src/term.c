/*
 * term.c - portable terminal backend for NT31.
 *
 *   POSIX (Linux/glibc/musl, macOS, BSDs): raw termios + select + VT100
 *   escape sequences + SGR mouse reporting.
 *   Windows (MinGW/MSVC, Win10+): ReadConsoleInput/ReadConsoleOutput API
 *   with ENABLE_VIRTUAL_TERMINAL_PROCESSING for the escape sequences.
 */
#include "nt.h"

#if defined(_WIN32)
#  include <windows.h>
#  include <stdio.h>
#  include <stdlib.h>
#  include <string.h>
#else
#  include <stdio.h>
#  include <stdlib.h>
#  include <string.h>
#  include <strings.h>
#  include <unistd.h>
#  include <errno.h>
#  include <signal.h>
#  include <sys/select.h>
#  include <sys/ioctl.h>
#  include <sys/time.h>
#  include <sys/types.h>
#  include <termios.h>
#  include <time.h>
#endif

typedef struct {
    uint32_t ch;
    uint8_t fg, bg, bold;
} tcell;

static tcell *back, *last;
static int W = 80, H = 24;
static int clip_on, clx0, cly0, clx1, cly1;
static int g_fast;

static void *xr(size_t n)
{
    void *p = realloc(NULL, n);
    if (!p) exit(1);
    return p;
}

/* ---------------- UTF-8 helpers ---------------- */
static int u8enc(uint32_t cp, char *o)
{
    if (cp < 0x80) { o[0] = (char)cp; return 1; }
    if (cp < 0x800) {
        o[0] = (char)(0xC0 | (cp >> 6));
        o[1] = (char)(0x80 | (cp & 63));
        return 2;
    }
    if (cp < 0x10000) {
        o[0] = (char)(0xE0 | (cp >> 12));
        o[1] = (char)(0x80 | ((cp >> 6) & 63));
        o[2] = (char)(0x80 | (cp & 63));
        return 3;
    }
    o[0] = (char)(0xF0 | (cp >> 18));
    o[1] = (char)(0x80 | ((cp >> 12) & 63));
    o[2] = (char)(0x80 | ((cp >> 6) & 63));
    o[3] = (char)(0x80 | (cp & 63));
    return 4;
}

static int u8dec(const unsigned char *s, uint32_t *cp)
{
    unsigned c = s[0];
    int n;
    if (c < 0x80) { *cp = c; return 1; }
    if ((c & 0xE0) == 0xC0) n = 2;
    else if ((c & 0xF0) == 0xE0) n = 3;
    else if ((c & 0xF8) == 0xF0) n = 4;
    else { *cp = 0x3F; return 1; }
    *cp = c & (0xFFu >> (n + 1));
    for (int i = 1; i < n; i++) {
        if ((s[i] & 0xC0) != 0x80) { *cp = 0x3F; return i; }
        *cp = (*cp << 6) | (s[i] & 63);
    }
    return n;
}

/* ---------------- small cross-platform utils ---------------- */
void set_fast(int f) { g_fast = f; }
int fast_mode(void) { return g_fast; }

int strci(const char *a, const char *b)
{
#if defined(_WIN32)
    return _stricmp(a, b);
#else
    return strcasecmp(a, b);
#endif
}

/* ---------------- drawing ---------------- */
static int inclip(int x, int y)
{
    return x >= clx0 && x < clx1 && y >= cly0 && y < cly1;
}

static void put(int x, int y, uint32_t ch, int fg, int bg, int bold)
{
    if (x < 0 || y < 0 || x >= W || y >= H) return;
    if (clip_on && !inclip(x, y)) return;
    tcell *c = &back[y * W + x];
    c->ch = ch; c->fg = (uint8_t)fg; c->bg = (uint8_t)bg; c->bold = (uint8_t)bold;
}

void term_begin(void)
{
    size_t n = (size_t)W * (size_t)H;
    if (!back) back = xr(n * sizeof(tcell));
    if (!last) last = xr(n * sizeof(tcell));
    for (int i = 0; i < W * H; i++) {
        back[i].ch = 0; back[i].fg = T_DESK_TXT; back[i].bg = T_DESK; back[i].bold = 0;
        last[i].ch = 0; last[i].fg = 0; last[i].bg = 0; last[i].bold = 0;
    }
}

int term_resize(int w, int h)
{
    if (w < 20) w = 20;
    if (h < 10) h = 10;
    if (w == W && h == H) return 0;
    W = w; H = h;
    term_begin();
    return 1;
}

void fill_all(int fg, int bg)
{
    for (int i = 0; i < W * H; i++) {
        back[i].ch = 0; back[i].fg = (uint8_t)fg; back[i].bg = (uint8_t)bg; back[i].bold = 0;
    }
}

void fill(int x, int y, int w, int h, int fg, int bg)
{
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++)
            put(i, j, 0, fg, bg, 0);
}

static void txtimpl(int x, int y, const char *s, int fg, int bg, int bold)
{
    const unsigned char *p = (const unsigned char *)s;
    for (int i = x; *p; i++) {
        uint32_t cp;
        int n = u8dec(p, &cp);
        put(i, y, cp, fg, bg, bold);
        p += n;
    }
}

void txt(int x, int y, const char *s, int fg, int bg) { txtimpl(x, y, s, fg, bg, 0); }
void txtb(int x, int y, const char *s, int fg, int bg) { txtimpl(x, y, s, fg, bg, 1); }

void glyph(int x, int y, uint32_t cp, int fg, int bg) { put(x, y, cp, fg, bg, 0); }

void frame3d(int x, int y, int w, int h, int fg, int bg, int raised)
{
    int lt = raised ? T_LT : T_DK;
    int dk = raised ? T_DK : T_LT;
    for (int i = x; i < x + w; i++) {
        put(i, y, 0, i == x ? lt : lt, bg, 0);              /* top: light */
        put(i, y + h - 1, 0, i == x + w - 1 ? dk : dk, bg, 0); /* bottom: dark */
    }
    for (int j = y; j < y + h; j++) {
        put(x, j, 0, j == y ? lt : (j == y + h - 1 ? dk : lt), bg, 0); /* left */
        put(x + w - 1, j, 0, j == y ? lt : (j == y + h - 1 ? dk : dk), bg, 0); /* right */
    }
    /* interior */
    fill(x + 1, y + 1, w - 2, h - 2, fg, bg);
}

void term_clip(int x, int y, int w, int h)
{
    clip_on = 1; clx0 = x; cly0 = y; clx1 = x + w; cly1 = y + h;
}
void term_clip_none(void) { clip_on = 0; }

/* ---------------- flush (diff-based) ---------------- */
static char outbuf[65536];
static int olen;

static void ow(const char *s, int n)
{
    if (olen + n >= (int)sizeof outbuf - 8) {
        fwrite(outbuf, 1, olen, stdout);
        olen = 0;
    }
    memcpy(outbuf + olen, s, n);
    olen += n;
}

static void osw(const char *s) { ow(s, (int)strlen(s)); }

void flush(void)
{
    char tmp[48];
    int cx = 0, cy = 0, a_fg = -1, a_bg = -1, a_b = -1;
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            tcell *c = &back[y * W + x];
            tcell *l = &last[y * W + x];
            if (c->ch == l->ch && c->fg == l->fg && c->bg == l->bg &&
                c->bold == l->bold)
                continue;
            if (x != cx || y != cy) {
                snprintf(tmp, sizeof tmp, "\x1b[%d;%dH", y + 1, x + 1);
                osw(tmp);
                cx = x; cy = y;
            }
            if (c->fg != a_fg || c->bg != a_bg || c->bold != a_b) {
                int fg = c->fg < 8 ? 30 + c->fg : 90 + (c->fg - 8);
                int bg = c->bg < 8 ? 40 + c->bg : 100 + (c->bg - 8);
                snprintf(tmp, sizeof tmp, "\x1b[%d;%d;%dm", fg, bg, c->bold ? 1 : 22);
                osw(tmp);
                a_fg = c->fg; a_bg = c->bg; a_b = c->bold;
            }
            if (c->ch) {
                char b[8];
                int n = u8enc(c->ch, b);
                ow(b, n);
            } else {
                ow(" ", 1);
            }
        }
    }
    osw("\x1b[?25l");
    if (olen) {
        fwrite(outbuf, 1, olen, stdout);
        olen = 0;
    }
    fflush(stdout);
    if (last && back) memcpy(last, back, (size_t)W * (size_t)H * sizeof(tcell));
}

int term_shot(const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) return 0;
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            char b[8];
            uint32_t cp = back[y * W + x].ch;
            int n = cp ? u8enc(cp, b) : (b[0] = ' ', 1);
            fwrite(b, 1, n, f);
        }
        fputc('\n', f);
    }
    fclose(f);
    return 1;
}

/* ================= POSIX ================= */
#if !defined(_WIN32)

static struct termios oldt;
static int have_tty;

static void sig_exit(int s)
{
    (void)s;
    exit(128 + s);
}

int term_init(void)
{
    have_tty = isatty(0) && isatty(1);
    if (have_tty) {
        if (tcgetattr(0, &oldt) == 0) {
            struct termios t = oldt;
            t.c_lflag &= ~(ICANON | ECHO | ISIG);
            t.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
            t.c_oflag &= ~(OPOST);
            t.c_cc[VMIN] = 1;
            t.c_cc[VTIME] = 0;
            tcsetattr(0, TCSANOW, &t);
        }
    }
    signal(SIGINT, sig_exit);
    signal(SIGTERM, sig_exit);
    signal(SIGHUP, sig_exit);
    signal(SIGQUIT, sig_exit);
    printf("\x1b[?25l\x1b[?1000h\x1b[?1002h\x1b[?1006h\x1b[?1049h\x1b[2J\x1b[H");
    fflush(stdout);
    term_size(&W, &H);
    term_begin();
    atexit(term_shutdown);
    return 1;
}

void term_shutdown(void)
{
    printf("\x1b[0m\x1b[?25h\x1b[?1002l\x1b[?1000l\x1b[?1049l");
    if (have_tty) tcsetattr(0, TCSANOW, &oldt);
    fflush(stdout);
}

void term_size(int *w, int *h)
{
    struct winsize ws;
    memset(&ws, 0, sizeof ws);
    if (ioctl(1, TIOCGWINSZ, &ws) == 0 && ws.ws_col >= 20 && ws.ws_row >= 10) {
        *w = ws.ws_col;
        *h = ws.ws_row;
        return;
    }
    *w = 80; *h = 24;
}

int is_tty(void) { return have_tty; }

void msleep(int ms)
{
    if (ms <= 0) return;
    struct timeval tv;
    tv.tv_sec = ms / 1000;
    tv.tv_usec = (long)(ms % 1000) * 1000L;
    select(0, NULL, NULL, NULL, &tv);
}

long long now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

void term_bell(void)
{
    putchar('\a');
    fflush(stdout);
}

/* ---- input: byte stream with a small escape-sequence parser ---- */
static unsigned char ib[512];
static int ibn, ibp;
static int pmode;      /* 0 raw, 1 esc, 2 CSI, 3 SS3, 4 CSI< mouse */
static int pv[3];
static int pn;

static void unget(void) { if (ibp > 0) ibp--; }

static int parse1(ev_t *ev)
{
    if (ibp >= ibn) return 0;
    unsigned char c = ib[ibp++];
    switch (pmode) {
    case 0:
        if (c == 0x1b) { pmode = 1; return 0; }
        if (c == 0x03) { ev->type = EV_QUIT; return 1; }
        if (c == 0x7f || c == 0x08) { ev->type = EV_KEY; ev->key = K_BACK; return 1; }
        if (c == '\r' || c == '\n') { ev->type = EV_KEY; ev->key = K_ENTER; return 1; }
        if (c == '\t') { ev->type = EV_KEY; ev->key = K_TAB; return 1; }
        if (c >= 0x20 && c < 0x7f) { ev->type = EV_CHAR; ev->ch = c; return 1; }
        return 0; /* ignore */
    case 1:
        if (c == '[') { pmode = 2; pn = 0; pv[0] = pv[1] = pv[2] = -1; return 0; }
        if (c == 'O') { pmode = 3; return 0; }
        pmode = 0; unget();
        ev->type = EV_KEY; ev->key = K_ESC;
        return 1;
    case 2:
        if (c >= '0' && c <= '9') {
            if (pn < 3) pv[pn] = pv[pn] < 0 ? (c - '0') : pv[pn] * 10 + (c - '0');
            return 0;
        }
        if (c == ';') {
            if (pv[pn] < 0) pv[pn] = 0;
            if (pn < 2) pn++;
            return 0;
        }
        if (c == '<') { pmode = 4; return 0; }
        {
            int p0 = pv[0] < 0 ? 0 : pv[0];
            pmode = 0;
            switch (c) {
            case 'A': ev->type = EV_KEY; ev->key = K_UP; return 1;
            case 'B': ev->type = EV_KEY; ev->key = K_DOWN; return 1;
            case 'C': ev->type = EV_KEY; ev->key = K_RIGHT; return 1;
            case 'D': ev->type = EV_KEY; ev->key = K_LEFT; return 1;
            case '~':
                switch (p0) {
                case 1: ev->type = EV_KEY; ev->key = K_HOME; return 1;
                case 3: ev->type = EV_KEY; ev->key = K_DEL; return 1;
                case 4: ev->type = EV_KEY; ev->key = K_END; return 1;
                case 5: ev->type = EV_KEY; ev->key = K_PGUP; return 1;
                case 6: ev->type = EV_KEY; ev->key = K_PGDN; return 1;
                case 21: ev->type = EV_KEY; ev->key = K_F10; return 1;
                default: return 0;
                }
            default: return 0;
            }
        }
    case 3:
        pmode = 0;
        switch (c) {
        case 'P': ev->type = EV_KEY; ev->key = K_F1; return 1;
        case 'Q': ev->type = EV_KEY; ev->key = K_F2; return 1;
        case 'R': ev->type = EV_KEY; ev->key = K_F3; return 1;
        case 'S': ev->type = EV_KEY; ev->key = K_F4; return 1;
        default: return 0;
        }
    case 4:
        if (c >= '0' && c <= '9') {
            if (pn < 3) pv[pn] = pv[pn] < 0 ? (c - '0') : pv[pn] * 10 + (c - '0');
            return 0;
        }
        if (c == ';') {
            if (pv[pn] < 0) pv[pn] = 0;
            if (pn < 2) pn++;
            return 0;
        }
        {
            pmode = 0;
            int b = pv[0] < 0 ? 0 : pv[0];
            int wheel = (b & 0x30) >> 4;
            int x = (pv[1] < 0 ? 1 : pv[1]) - 1;
            int y = (pv[2] < 0 ? 1 : pv[2]) - 1;
            ev->type = EV_MOUSE;
            ev->x = x; ev->y = y;
            if (c == 'M') {
                if (wheel == 1) ev->wheel = 1;
                else if (wheel == 2) ev->wheel = -1;
                else ev->btn = b & 3;
            } else if (c == 'm') {
                if (wheel) ev->wheel = 0;
                else ev->btn = -1 - (b & 3);
            } else return 0;
            return 1;
        }
    }
    return 0;
}

/* A lone ESC may be the first byte of a sequence that arrives in the next
 * read(); wait briefly before deciding it is a real Escape key. */
static long long esc_pending_ms;

int term_get(ev_t *ev, int ms)
{
    ev->type = 0; ev->ch = 0; ev->key = 0;
    ev->x = 0; ev->y = 0; ev->btn = 0; ev->wheel = 0;
    for (;;) {
        if (ibp < ibn) {
            int r = parse1(ev);
            if (r) return 1;
        }
        if (ibp > 0) {
            memmove(ib, ib + ibp, (size_t)(ibn - ibp));
            ibn -= ibp; ibp = 0;
        }
        /* no complete event yet */
        int t = ms;
        if (pmode == 1) {
            /* waiting to see whether '[' or 'O' follows the ESC */
            if (esc_pending_ms == 0)
                esc_pending_ms = now_ms() + 25;
            t = (int)(esc_pending_ms - now_ms());
            if (t <= 0) {
                pmode = 0;
                esc_pending_ms = 0;
                ev->type = EV_KEY;
                ev->key = K_ESC;
                return 1;
            }
            if (t > 25) t = 25;
        } else {
            esc_pending_ms = 0;
            if (!have_tty && t > 100) t = 100; /* don't stall on pipes */
        }
        fd_set fs;
        struct timeval tv;
        FD_ZERO(&fs);
        FD_SET(0, &fs);
        tv.tv_sec = t / 1000;
        tv.tv_usec = (long)(t % 1000) * 1000L;
        int s = select(1, &fs, NULL, NULL, &tv);
        if (s > 0) {
            ssize_t n = read(0, ib + ibn, sizeof ib - (size_t)ibn);
            if (n == 0) { ev->type = EV_QUIT; return 1; }
            if (n > 0) { ibn += (int)n; continue; }
            if (n < 0 && errno != EINTR && errno != EAGAIN) {
                ev->type = EV_QUIT; return 1;
            }
        } else if (s < 0 && errno != EINTR) {
            ev->type = EV_QUIT;
            return 1;
        }
        if (s == 0 && pmode != 1) return 0;
    }
}

#endif /* !WIN32 */

/* ================= Windows ================= */
#if defined(_WIN32)

static HANDLE hIn, hOut;
static DWORD oldInMode, oldOutMode;
static CONSOLE_CURSOR_INFO oldCur;
static int have_tty;

static BOOL WINAPI ctrl_handler(DWORD type)
{
    (void)type;
    exit(130);
}

int term_init(void)
{
    hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    hIn = GetStdHandle(STD_INPUT_HANDLE);
    DWORD m;
    have_tty = GetConsoleMode(hOut, &m) != 0;
    if (have_tty) {
        if (GetConsoleMode(hOut, &oldOutMode))
            SetConsoleMode(hOut, oldOutMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        if (GetConsoleMode(hIn, &oldInMode))
            SetConsoleMode(hIn, oldInMode | ENABLE_EXTENDED_FLAGS |
                           ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT);
        if (GetConsoleCursorInfo(hOut, &oldCur)) {
            CONSOLE_CURSOR_INFO ci = oldCur;
            ci.bVisible = FALSE;
            SetConsoleCursorInfo(hOut, &ci);
        }
        printf("\x1b[2J\x1b[H");
        fflush(stdout);
    }
    SetConsoleCtrlHandler(ctrl_handler, TRUE);
    term_size(&W, &H);
    term_begin();
    atexit(term_shutdown);
    return 1;
}

void term_shutdown(void)
{
    if (have_tty) {
        CONSOLE_CURSOR_INFO ci = oldCur;
        SetConsoleCursorInfo(hOut, &ci);
        SetConsoleMode(hOut, oldOutMode);
        SetConsoleMode(hIn, oldInMode);
        printf("\x1b[0m");
        fflush(stdout);
    }
}

void term_size(int *w, int *h)
{
    CONSOLE_SCREEN_BUFFER_INFO bi;
    if (GetConsoleScreenBufferInfo(hOut, &bi)) {
        *w = bi.srWindow.Right - bi.srWindow.Left + 1;
        *h = bi.srWindow.Bottom - bi.srWindow.Top + 1;
        return;
    }
    *w = 80; *h = 24;
}

int is_tty(void) { return have_tty; }

void msleep(int ms)
{
    if (ms > 0) Sleep(ms);
}

long long now_ms(void)
{
    return (long long)GetTickCount64();
}

void term_bell(void)
{
    putchar('\a');
    fflush(stdout);
}

int term_get(ev_t *ev, int ms)
{
    ev->type = 0; ev->ch = 0; ev->key = 0;
    ev->x = 0; ev->y = 0; ev->btn = 0; ev->wheel = 0;
    if (!have_tty) {
        /* piped stdin on Windows: fall back to stdin text */
        if (ms > 0) Sleep(ms);
        int c = fgetc(stdin);
        if (c == EOF) { ev->type = EV_QUIT; return 1; }
        if (c == '\r' || c == '\n') { ev->type = EV_KEY; ev->key = K_ENTER; return 1; }
        ev->type = EV_CHAR; ev->ch = c;
        return 1;
    }
    DWORD wr = WaitForSingleObject(hIn, ms);
    if (wr != WAIT_OBJECT_0) return 0;
    INPUT_RECORD rec[256];
    DWORD n = 0;
    if (!ReadConsoleInput(hIn, rec, 256, &n)) return 0;
    for (DWORD i = 0; i < n; i++) {
        if (rec[i].dwEventType == KEY_EVENT) {
            KEY_EVENT_RECORD *k = &rec[i].Event.KeyEvent;
            if (!k->bKeyDown) continue;
            switch (k->wVirtualKeyCode) {
            case VK_UP: ev->type = EV_KEY; ev->key = K_UP; return 1;
            case VK_DOWN: ev->type = EV_KEY; ev->key = K_DOWN; return 1;
            case VK_LEFT: ev->type = EV_KEY; ev->key = K_LEFT; return 1;
            case VK_RIGHT: ev->type = EV_KEY; ev->key = K_RIGHT; return 1;
            case VK_RETURN: ev->type = EV_KEY; ev->key = K_ENTER; return 1;
            case VK_ESCAPE: ev->type = EV_KEY; ev->key = K_ESC; return 1;
            case VK_TAB: ev->type = EV_KEY; ev->key = K_TAB; return 1;
            case VK_BACK: ev->type = EV_KEY; ev->key = K_BACK; return 1;
            case VK_DELETE: ev->type = EV_KEY; ev->key = K_DEL; return 1;
            case VK_HOME: ev->type = EV_KEY; ev->key = K_HOME; return 1;
            case VK_END: ev->type = EV_KEY; ev->key = K_END; return 1;
            case VK_PRIOR: ev->type = EV_KEY; ev->key = K_PGUP; return 1;
            case VK_NEXT: ev->type = EV_KEY; ev->key = K_PGDN; return 1;
            case VK_F1: ev->type = EV_KEY; ev->key = K_F1; return 1;
            case VK_F2: ev->type = EV_KEY; ev->key = K_F2; return 1;
            case VK_F3: ev->type = EV_KEY; ev->key = K_F3; return 1;
            case VK_F4: ev->type = EV_KEY; ev->key = K_F4; return 1;
            case VK_F10: ev->type = EV_KEY; ev->key = K_F10; return 1;
            default: break;
            }
            if (k->UnicodeChar >= 0x20 && k->UnicodeChar < 0x7f) {
                ev->type = EV_CHAR;
                ev->ch = (int)k->UnicodeChar;
                return 1;
            }
            continue;
        }
        if (rec[i].dwEventType == MOUSE_EVENT) {
            MOUSE_EVENT_RECORD *m = &rec[i].Event.MouseEvent;
            SHORT mx = (SHORT)(m->dwMousePosition & 0xFFFF);
            SHORT my = (SHORT)((m->dwMousePosition >> 16) & 0xFFFF);
            ev->type = EV_MOUSE;
            ev->x = (int)mx; ev->y = (int)my;
            if (m->dwEventFlags & MOUSE_WHEELED) {
                ev->wheel = (SHORT)m->dwButtonData > 0 ? 1 : -1;
                return 1;
            }
            if (m->dwEventFlags & MOUSE_MOVED) {
                ev->btn = M_MOVE;
                return 1;
            }
            if (m->dwEventFlags & RIGHTDOWN) { ev->btn = M_RIGHT; return 1; }
            if (m->dwEventFlags & RIGHTUP) { ev->btn = -1 - M_RIGHT; return 1; }
            if (m->dwEventFlags & MIDDLEDOWN) { ev->btn = M_MID; return 1; }
            if (m->dwEventFlags & MIDDLEUP) { ev->btn = -1 - M_MID; return 1; }
            if (m->dwEventFlags & LEFTDOWN) { ev->btn = M_LEFT; return 1; }
            if (m->dwEventFlags & LEFTUP) { ev->btn = -1 - M_LEFT; return 1; }
            continue;
        }
    }
    return 0;
}

#endif /* WIN32 */
