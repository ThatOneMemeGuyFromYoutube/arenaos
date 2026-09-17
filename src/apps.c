/*
 * apps.c - the Win32 applications of the NT31 recreation:
 * Console, Notepad, Calculator, Clock, Minesweeper, Character Map,
 * Control Panel, and the About box.
 */
#include "nt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ==================== Console (MS-DOS prompt) ==================== */
typedef struct {
    char lines[24][80];
    int r;
    char cmd[96];
    char hist[16][96];
    int nhist, hsel;
} con_t;

static void con_out(con_t *c, int nrows, const char *s)
{
    int n = (int)strlen(s);
    if (n > 78) n = 78;
    if (c->r >= nrows - 1) {
        memmove(c->lines, c->lines + 1, (size_t)(nrows - 1) * 80);
        c->r = nrows - 2;
    }
    memset(c->lines[c->r], 0, 80);
    memcpy(c->lines[c->r], s, (size_t)n);
    c->r++;
    if (c->r >= nrows) c->r = nrows - 1;
}

static void con_win(const char *arg, win_t *w)
{
    if (strci(arg, "notepad") == 0) launch_app(APP_NOTEPAD, 0);
    else if (strci(arg, "calc") == 0) launch_app(APP_CALC, 0);
    else if (strci(arg, "clock") == 0) launch_app(APP_CLOCK, 0);
    else if (strci(arg, "winmine") == 0) launch_app(APP_MINES, 0);
    else if (strci(arg, "charmap") == 0) launch_app(APP_CHARMAP, 0);
    else if (strci(arg, "cpl") == 0) launch_app(APP_CPL, 0);
    else if (strci(arg, "about") == 0) launch_app(APP_ABOUT, 0);
    else {
        con_t *c = w->ctx;
        con_out(c, 24, "WIN: unknown application");
    }
    (void)w;
}

static void con_exec(win_t *w, con_t *c)
{
    char *line = c->cmd;
    char arg[96] = "";
    char *sp = strchr(line, ' ');
    if (sp) {
        *sp = 0;
        sp++;
        while (*sp == ' ') sp++;
        snprintf(arg, sizeof arg, "%s", sp);
    }
    if (line[0]) {
        if (c->nhist < 16) {
            snprintf(c->hist[c->nhist], 96, "%s", line);
            c->nhist++;
        }
        c->hsel = c->nhist;
    }
    if (line[0] == 0) return;
    if (strci(line, "help") == 0) {
        const char *h[] = {
            "Available commands:",
            "  CLS      Clear the screen",
            "  CD       Change directory",
            "  DATE     Show the date",
            "  DIR      List a directory",
            "  ECHO     Display a message",
            "  EXIT     Close the console",
            "  MEM      Show memory status",
            "  SET      Show environment",
            "  TASK     List running processes",
            "  TIME     Show the time",
            "  VER      Show the version",
            "  WIN      Start a Windows application",
            "         (WIN notepad|calc|clock|winmine|charmap|cpl)",
            0 };
        for (int i = 0; h[i]; i++) con_out(c, 24, h[i]);
    } else if (strci(line, "cls") == 0) {
        memset(c->lines, 0, sizeof c->lines);
        c->r = 0;
    } else if (strci(line, "ver") == 0) {
        con_out(c, 24, "Windows NT [Version 3.10.103]");
    } else if (strci(line, "dir") == 0) {
        const char *d[] = {
            " Volume in drive C is NT31",
            " Volume Serial Number is 31-07",
            "",
            " Directory of C:\\",
            "",
            "WINDOWS    <DIR>  07-27-93  4:22p",
            "DOS6       <DIR>  07-27-93  4:23p",
            "AUTOEXEC   BAT           132  07-27-93  4:22p",
            "CONFIG     BAT            84  07-27-93  4:22p",
            "COMMAND    COM         53088  07-27-93  4:22p",
            "NTDETECT   COM          3328  07-27-93  4:22p",
            "      4 file(s)         56592 bytes",
            "      2 dir(s)",
            "  13421772 bytes free",
            0 };
        for (int i = 0; d[i]; i++) con_out(c, 24, d[i]);
    } else if (strci(line, "task") == 0) {
        con_out(c, 24, "  PID   NAME");
        con_out(c, 24, "----   ----------");
        for (int i = 0; i < proc_count(); i++) {
            char s[40];
            snprintf(s, sizeof s, " %4d   %s", proc_id_at(i), proc_name_at(i));
            con_out(c, 24, s);
        }
    } else if (strci(line, "mem") == 0) {
        char s[64];
        int f = mem_free_x10();
        snprintf(s, sizeof s, "Memory: %d.%d MB free, %d MB total",
                 f / 10, f % 10, mem_total_x10() / 10);
        con_out(c, 24, s);
    } else if (strci(line, "date") == 0 || strci(line, "time") == 0) {
        time_t t = time(0);
        struct tm *lt = localtime(&t);
        char s[64];
        if (strci(line, "date") == 0)
            snprintf(s, sizeof s, "The current date is %02d-%02d-%04d",
                     lt->tm_mon + 1, lt->tm_mday, lt->tm_year + 1900);
        else
            snprintf(s, sizeof s, "The current time is %02d:%02d:%02d",
                     lt->tm_hour, lt->tm_min, lt->tm_sec);
        con_out(c, 24, s);
    } else if (strci(line, "set") == 0) {
        const char *e[] = {
            "COMSPEC=C:\\DOS6\\COMMAND.COM",
            "PROMPT=$p$g",
            "TEMP=C:\\TEMP",
            "WINNT=C:\\",
            0 };
        for (int i = 0; e[i]; i++) con_out(c, 24, e[i]);
    } else if (strci(line, "cd") == 0) {
        if (arg[0] == '\\' || arg[0] == '/')
            con_out(c, 24, "You are already in C:\\");
        else
            con_out(c, 24, "The directory was not found.");
    } else if (strci(line, "md") == 0 || strci(line, "rd") == 0) {
        con_out(c, 24, "Access is denied.");
    } else if (strci(line, "echo") == 0) {
        con_out(c, 24, arg[0] ? arg : "");
    } else if (strci(line, "win") == 0) {
        con_win(arg, w);
    } else if (strci(line, "notepad") == 0) {
        launch_app(APP_NOTEPAD, 0);
    } else if (strci(line, "calc") == 0) {
        launch_app(APP_CALC, 0);
    } else if (strci(line, "clock") == 0) {
        launch_app(APP_CLOCK, 0);
    } else if (strci(line, "winmine") == 0) {
        launch_app(APP_MINES, 0);
    } else if (strci(line, "exit") == 0) {
        close_win(w);
        return;
    } else {
        char s[160];
        snprintf(s, sizeof s, "'%s' is not recognized as an internal or", line);
        con_out(c, 24, s);
        con_out(c, 24, "external command, program, or batch file.");
    }
}

static void con_start(win_t *w)
{
    con_t *c = calloc(1, sizeof *c);
    if (!c) return;
    memset(c->lines, 0, sizeof c->lines);
    w->ctx = c;
}

static void con_stop(win_t *w)
{
    free(w->ctx);
    w->ctx = 0;
}

static void con_draw(win_t *w, int x, int y, int cw, int ch)
{
    con_t *c = w->ctx;
    int nrows = ch < 24 ? ch : 24;
    fill(x, y, cw, ch, C_WHITE, C_BLACK);
    for (int i = 0; i < nrows; i++)
        txt(x, y + i, c->lines[i], C_WHITE, C_BLACK);
    if (c->r < nrows) {
        char prompt[112];
        snprintf(prompt, sizeof prompt, "C:\\> %s", c->cmd);
        txt(x, y + c->r, prompt, C_WHITE, C_BLACK);
        int col = (int)strlen(prompt);
        if ((now_ms() / 500) & 1)
            glyph(x + col, y + c->r, 0x2588u, C_WHITE, C_BLACK);
    }
}

static int con_key(win_t *w, int key, int ch)
{
    con_t *c = w->ctx;
    if (ch >= 32 && ch < 127) {
        int n = (int)strlen(c->cmd);
        if (n < 90) {
            c->cmd[n] = (char)ch;
            c->cmd[n + 1] = 0;
        }
        c->hsel = c->nhist;
        return 1;
    }
    switch (key) {
    case K_BACK: {
        int n = (int)strlen(c->cmd);
        if (n) c->cmd[n - 1] = 0;
        return 1;
    }
    case K_ENTER:
        con_exec(w, c);
        c->cmd[0] = 0;
        return 1;
    case K_UP:
        if (c->nhist && c->hsel > 0) {
            c->hsel--;
            snprintf(c->cmd, sizeof c->cmd, "%s", c->hist[c->hsel]);
        }
        return 1;
    case K_DOWN:
        if (c->hsel < c->nhist) {
            c->hsel++;
            if (c->hsel >= c->nhist) c->cmd[0] = 0;
            else snprintf(c->cmd, sizeof c->cmd, "%s", c->hist[c->hsel]);
        }
        return 1;
    case K_DEL:
    case K_HOME:
    case K_END:
    case K_TAB:
    case K_PGUP:
    case K_PGDN:
        return 1;
    default:
        return 0; /* arrows / Esc move or close the window */
    }
}

static int con_mouse(win_t *w, int x, int y, int btn, int moved)
{
    (void)w; (void)x; (void)y; (void)btn; (void)moved;
    return 1; /* DOS apps do not know about mice */
}

static int con_tick(win_t *w)
{
    (void)w;
    return 1; /* cursor blink */
}

/* ==================== Notepad ==================== */
typedef struct {
    char ln[64][256];
    int nln;
    int li, ci, top;
} np_t;

static int np_len(np_t *n, int i) { return (int)strlen(n->ln[i]); }

static void np_ins_ch(np_t *n, int ch)
{
    char *l = n->ln[n->li];
    int len = np_len(n, n->li);
    if (len >= 250) return;
    memmove(l + n->ci + 1, l + n->ci, (size_t)(len - n->ci) + 1);
    l[n->ci] = (char)ch;
    n->ci++;
}

static void np_back(np_t *n)
{
    if (n->ci > 0) {
        char *l = n->ln[n->li];
        memmove(l + n->ci - 1, l + n->ci, (size_t)(np_len(n, n->li) - n->ci) + 1);
        n->ci--;
    } else if (n->li > 0) {
        int len = np_len(n, n->li - 1);
        memcpy(n->ln[n->li - 1] + len, n->ln[n->li], (size_t)np_len(n, n->li) + 1);
        memmove(n->ln + n->li - 1, n->ln + n->li,
                (size_t)(n->nln - n->li) * 256);
        n->nln--;
        n->ci = len;
        n->li--;
    }
}

static void np_enter(np_t *n)
{
    if (n->nln >= 63) return;
    char rest[256];
    char *l = n->ln[n->li];
    memcpy(rest, l + n->ci, (size_t)(np_len(n, n->li) - n->ci) + 1);
    l[n->ci] = 0;
    memmove(n->ln + n->li + 1, n->ln + n->li,
            (size_t)(n->nln - n->li) * 256);
    n->nln++;
    memcpy(n->ln[n->li + 1], rest, sizeof rest);
    n->li++;
    n->ci = 0;
}

static void np_delfwd(np_t *n)
{
    int len = np_len(n, n->li);
    if (n->ci < len) {
        char *l = n->ln[n->li];
        memmove(l + n->ci, l + n->ci + 1, (size_t)(len - n->ci) + 1);
    } else if (n->li < n->nln - 1) {
        char *l = n->ln[n->li];
        if (len + np_len(n, n->li + 1) < 250)
            memcpy(l + len, n->ln[n->li + 1], (size_t)np_len(n, n->li + 1) + 1);
        memmove(n->ln + n->li, n->ln + n->li + 1,
                (size_t)(n->nln - n->li - 1) * 256);
        n->nln--;
    }
}

static const char *readme[] = {
    "Windows NT 3.1",
    "",
    "Welcome to your new operating system.",
    "",
    "This recreation is written in portable C",
    "and runs entirely inside your terminal.",
    "",
    "Try the Console:  WIN CLOCK",
    "or play Minesweeper from the",
    "Program group.",
    0
};

static void np_start(win_t *w)
{
    np_t *n = calloc(1, sizeof *n);
    if (!n) return;
    if (w->param == 1) {
        for (int i = 0; readme[i] && i < 64; i++) {
            snprintf(n->ln[i], 256, "%s", readme[i]);
            n->nln++;
        }
    } else {
        n->ln[0][0] = 0;
        n->nln = 1;
    }
    w->ctx = n;
}

static void np_stop(win_t *w)
{
    free(w->ctx);
    w->ctx = 0;
}

static void np_draw(win_t *w, int x, int y, int cw, int ch)
{
    np_t *n = w->ctx;
    fill(x, y, cw, ch, T_CLI_TX, T_CLI);
    if (n->li < n->top) n->top = n->li;
    if (n->li >= n->top + ch) n->top = n->li - ch + 1;
    for (int row = 0; row < ch; row++) {
        int i = n->top + row;
        if (i >= n->nln) break;
        txt(x, y + row, n->ln[i], T_CLI_TX, T_CLI);
    }
    int row = n->li - n->top;
    if (row >= 0 && row < ch && ((now_ms() / 500) & 1)) {
        int clen = np_len(n, n->li);
        char cell = n->ci < clen ? n->ln[n->li][n->ci] : ' ';
        glyph(x + n->ci, y + row, (uint32_t)(unsigned char)cell,
              T_CLI_TX, T_SEL);
    }
}

static int np_key(win_t *w, int key, int ch)
{
    np_t *n = w->ctx;
    if (ch >= 32 && ch < 127) {
        np_ins_ch(n, ch);
        return 1;
    }
    switch (key) {
    case K_BACK:
        np_back(n);
        return 1;
    case K_DEL:
        np_delfwd(n);
        return 1;
    case K_ENTER:
        np_enter(n);
        return 1;
    case K_LEFT:
        if (n->ci > 0) n->ci--;
        else if (n->li > 0) {
            n->li--;
            n->ci = np_len(n, n->li);
        }
        return 1;
    case K_RIGHT:
        if (n->ci < np_len(n, n->li)) n->ci++;
        else if (n->li < n->nln - 1) {
            n->li++;
            n->ci = 0;
        }
        return 1;
    case K_UP:
        if (n->li > 0) {
            n->li--;
            if (n->ci > np_len(n, n->li)) n->ci = np_len(n, n->li);
        }
        return 1;
    case K_DOWN:
        if (n->li < n->nln - 1) {
            n->li++;
            if (n->ci > np_len(n, n->li)) n->ci = np_len(n, n->li);
        }
        return 1;
    case K_HOME:
        n->ci = 0;
        return 1;
    case K_END:
        n->ci = np_len(n, n->li);
        return 1;
    case K_PGUP: {
        int rows = w->h - 3;
        n->li = n->li > rows ? n->li - rows : 0;
        return 1;
    }
    case K_PGDN: {
        int rows = w->h - 3;
        if (n->li + rows < n->nln) n->li += rows;
        else n->li = n->nln - 1;
        if (n->ci > np_len(n, n->li)) n->ci = np_len(n, n->li);
        return 1;
    }
    case K_TAB:
        np_ins_ch(n, ' ');
        return 1;
    default:
        return 0;
    }
}

static int np_mouse(win_t *w, int x, int y, int btn, int moved)
{
    (void)moved;
    if (btn != M_LEFT) return 1;
    np_t *n = w->ctx;
    int li = n->top + y;
    if (li < 0) li = 0;
    if (li >= n->nln) li = n->nln - 1;
    n->li = li;
    n->ci = x;
    if (n->ci > np_len(n, n->li)) n->ci = np_len(n, n->li);
    return 1;
}

static int np_tick(win_t *w)
{
    (void)w;
    return 1;
}

/* ==================== Calculator ==================== */
typedef struct {
    double acc;
    char disp[24];
    int op;    /* 0 = none, otherwise '+', '-', '*', '/' */
    int fresh; /* next digit starts a new number */
    int err;
} calc_t;

static const char *calc_labels[17] = {
    "C", "CE", "\xC2\xB1", "\xC3\xB7",
    "7", "8", "9", "\xC3\x97",
    "4", "5", "6", "\xE2\x88\x92",
    "1", "2", "3", "+", "="
};

static double calc_apply(int op, double a, double b)
{
    switch (op) {
    case '+': return a + b;
    case '-': return a - b;
    case '*': return a * b;
    case '/': return b == 0.0 ? 0.0 : a / b;
    default: return b;
    }
}

static void calc_reset(calc_t *c)
{
    c->acc = 0;
    c->disp[0] = '0';
    c->disp[1] = 0;
    c->op = 0;
    c->fresh = 1;
    c->err = 0;
}

static void calc_digit(calc_t *c, int d)
{
    if (c->err) calc_reset(c);
    if (c->fresh || strcmp(c->disp, "0") == 0) {
        c->disp[0] = (char)('0' + d);
        c->disp[1] = 0;
    } else {
        int n = (int)strlen(c->disp);
        if (n < 15) {
            c->disp[n] = (char)('0' + d);
            c->disp[n + 1] = 0;
        }
    }
    c->fresh = 0;
}

static void calc_dot(calc_t *c)
{
    if (c->err) calc_reset(c);
    if (c->fresh) {
        snprintf(c->disp, sizeof c->disp, "0.");
    } else if (!strchr(c->disp, '.')) {
        int n = (int)strlen(c->disp);
        if (n < 15) {
            c->disp[n] = '.';
            c->disp[n + 1] = 0;
        }
    }
}

static void calc_op(calc_t *c, int op)
{
    if (c->err) return;
    double cur = strtod(c->disp, 0);
    if (c->op && !c->fresh)
        c->acc = calc_apply(c->op, c->acc, cur);
    else
        c->acc = cur;
    c->op = op;
    c->fresh = 1;
    snprintf(c->disp, sizeof c->disp, "%.10g", c->acc);
}

static void calc_eq(calc_t *c)
{
    if (c->err) return;
    double cur = strtod(c->disp, 0);
    if (c->op == '/') {
        if (cur == 0.0) {
            c->err = 1;
            snprintf(c->disp, sizeof c->disp, "Error");
            return;
        }
    }
    double r = calc_apply(c->op, c->acc, cur);
    c->op = 0;
    c->fresh = 1;
    if (c->op == 0 && r == (long long)r && r < 1e12)
        snprintf(c->disp, sizeof c->disp, "%lld", (long long)r);
    else
        snprintf(c->disp, sizeof c->disp, "%.10g", r);
}

static void calc_do(calc_t *c, const char *lbl)
{
    if (strcmp(lbl, "C") == 0) {
        calc_reset(c);
    } else if (strcmp(lbl, "CE") == 0) {
        c->disp[0] = '0';
        c->disp[1] = 0;
        c->fresh = 1;
    } else if (strcmp(lbl, "\xC2\xB1") == 0) {
        double v = strtod(c->disp, 0);
        snprintf(c->disp, sizeof c->disp, "%.10g", -v);
    } else if (strcmp(lbl, "\xC3\xB7") == 0)
        calc_op(c, '/');
    else if (strcmp(lbl, "\xC3\x97") == 0)
        calc_op(c, '*');
    else if (strcmp(lbl, "\xE2\x88\x92") == 0)
        calc_op(c, '-');
    else if (strcmp(lbl, "+") == 0)
        calc_op(c, '+');
    else if (strcmp(lbl, "=") == 0)
        calc_eq(c);
    else if (strcmp(lbl, ".") == 0)
        calc_dot(c);
    else if (lbl[0] >= '0' && lbl[0] <= '9' && lbl[1] == 0)
        calc_digit(c, lbl[0] - '0');
}

static int calc_btn(win_t *w, int px, int py)
{
    (void)w;
    if (py >= 4 && py <= 13) {
        int r = (py - 4) / 3;
        if (r >= 0 && r <= 3) {
            int c = (px - 1) / 5;
            if (c >= 0 && c <= 3) return r * 4 + c;
        }
    }
    return -1;
}

static void calc_start(win_t *w)
{
    calc_t *c = calloc(1, sizeof *c);
    if (!c) return;
    calc_reset(c);
    w->ctx = c;
}

static void calc_stop(win_t *w)
{
    free(w->ctx);
    w->ctx = 0;
}

static void calc_draw(win_t *w, int x, int y, int cw, int ch)
{
    calc_t *c = w->ctx;
    fill(x, y, cw, ch, T_CLI_TX, T_CLI);
    frame3d(x + 1, y + 1, cw - 2, 2, T_CLI_TX, T_CLI, 0);
    int n = (int)strlen(c->disp);
    int dx = x + cw - 2 - n;
    if (dx < x + 2) dx = x + 2;
    txt(dx, y + 1, c->disp, T_CLI_TX, T_CLI);
    for (int r = 0; r < 4; r++) {
        for (int c2 = 0; c2 < 4; c2++) {
            int id = r * 4 + c2;
            int bx = x + 1 + c2 * 5, by = y + 4 + r * 3;
            frame3d(bx, by, 4, 2, T_FACE_TXT, T_FACE, 1);
            const char *l = calc_labels[id];
            int lw = (int)strlen(l);
            txt(bx + (4 - (lw < 2 ? lw : 2)) / 2, by, l, T_FACE_TXT, T_FACE);
        }
    }
    /* bottom row: 0 (wide), ., = */
    {
        int by = y + 4 + 4 * 3;
        frame3d(x + 1, by, 9, 2, T_FACE_TXT, T_FACE, 1);
        txt(x + 1 + (9 - 1) / 2, by, "0", T_FACE_TXT, T_FACE);
        frame3d(x + 11, by, 4, 2, T_FACE_TXT, T_FACE, 1);
        txt(x + 11 + (4 - 1) / 2, by, ".", T_FACE_TXT, T_FACE);
        frame3d(x + 16, by, 4, 2, T_FACE_TXT, T_FACE, 1);
        txt(x + 16 + (4 - 1) / 2, by, "=", T_FACE_TXT, T_FACE);
    }
}

static int calc_key(win_t *w, int key, int ch)
{
    calc_t *c = w->ctx;
    if (ch >= '0' && ch <= '9') {
        calc_digit(c, ch - '0');
        return 1;
    }
    if (ch == '.') {
        calc_dot(c);
        return 1;
    }
    if (ch == '+') {
        calc_op(c, '+');
        return 1;
    }
    if (ch == '-') {
        calc_op(c, '-');
        return 1;
    }
    if (ch == '*') {
        calc_op(c, '*');
        return 1;
    }
    if (ch == '/') {
        calc_op(c, '/');
        return 1;
    }
    if (ch == 'c' || ch == 'C') {
        calc_reset(c);
        return 1;
    }
    if (ch == 'e' || ch == 'E') {
        c->disp[0] = '0';
        c->disp[1] = 0;
        c->fresh = 1;
        return 1;
    }
    if (key == K_ENTER) {
        calc_eq(c);
        return 1;
    }
    if (key == K_BACK) {
        if (!c->fresh && !c->err) {
            int n = (int)strlen(c->disp);
            if (n <= 1) {
                calc_reset(c);
            } else if (c->disp[n - 1] == '.') {
                c->disp[n - 1] = 0;
            } else {
                c->disp[n - 1] = 0;
                if (strcmp(c->disp, "0") != 0 && c->disp[0] != '-')
                    calc_reset(c);
            }
        }
        return 1;
    }
    return 0;
}

static int calc_mouse(win_t *w, int x, int y, int btn, int moved)
{
    (void)moved;
    if (btn != M_LEFT) return 1;
    calc_t *c = w->ctx;
    int id = calc_btn(w, x, y);
    if (id >= 0 && id < 16) {
        calc_do(c, calc_labels[id]);
        return 1;
    }
    /* bottom row: 0 (wide), ., = */
    if (y >= 16 && y <= 17) {
        if (x >= 1 && x <= 10)
            calc_digit(c, 0);
        else if (x >= 11 && x <= 14)
            calc_dot(c);
        else if (x >= 16 && x <= 19)
            calc_eq(c);
    }
    return 1;
}

static int calc_tick(win_t *w)
{
    (void)w;
    return 0;
}

/* ==================== Clock ==================== */
static void clk_draw(win_t *w, int x, int y, int cw, int ch)
{
    (void)w;
    fill(x, y, cw, ch, T_CLI_TX, T_CLI);
    time_t t = time(0);
    struct tm *lt = localtime(&t);
    char s[32];
    snprintf(s, sizeof s, "%02d:%02d:%02d", lt->tm_hour, lt->tm_min, lt->tm_sec);
    txtb(x + (cw - 8) / 2, y + 1, s, T_SEL_TX, T_SEL);
    static const char *dow[7] = { "Sunday", "Monday", "Tuesday", "Wednesday",
                                  "Thursday", "Friday", "Saturday" };
    static const char *mon[12] = { "January", "February", "March", "April",
                                   "May", "June", "July", "August",
                                   "September", "October", "November", "December" };
    char d[48];
    snprintf(d, sizeof d, "%s, %s %d, %d", dow[lt->tm_wday], mon[lt->tm_mon],
             lt->tm_mday, lt->tm_year + 1900);
    txt(x + (cw - (int)strlen(d)) / 2, y + 3, d, T_CLI_TX, T_CLI);
}

static int clk_tick(win_t *w)
{
    (void)w;
    return 1;
}

/* ==================== Minesweeper ==================== */
#define MW 9
#define MH 9
#define MMINES 10

typedef struct {
    unsigned char st[81];  /* 0 hidden, 1 open, 2 flag */
    unsigned char val[81]; /* 0-8 neighbors, 9 = mine */
    int flags;
    int state;             /* 0 play, 1 win, 2 lose */
    int started, t0, timer;
    int cx, cy;
} mines_t;

static void mines_new(win_t *w)
{
    mines_t *m = w->ctx;
    memset(m->st, 0, 81);
    memset(m->val, 0, 81);
    m->flags = 0;
    m->state = 0;
    m->started = 0;
    m->timer = 0;
    m->cx = 4;
    m->cy = 4;
    int placed = 0;
    while (placed < MMINES) {
        int i = rand() % 81;
        if (m->val[i]) continue;
        m->val[i] = 9;
        placed++;
    }
    for (int yy = 0; yy < MH; yy++)
        for (int xx = 0; xx < MW; xx++) {
            int i = yy * MW + xx;
            if (m->val[i] == 9) continue;
            int cnt = 0;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    int nx = xx + dx, ny = yy + dy;
                    if (nx < 0 || ny < 0 || nx >= MW || ny >= MH) continue;
                    if (m->val[ny * MW + nx] == 9) cnt++;
                }
            m->val[i] = (unsigned char)cnt;
        }
}

static void mines_reveal(win_t *w, int x, int y)
{
    mines_t *m = w->ctx;
    if (m->state != 0) return;
    if (x < 0 || y < 0 || x >= MW || y >= MH) return;
    int i = y * MW + x;
    if (m->st[i] != 0) return;
    if (!m->started) {
        m->started = 1;
        m->t0 = (int)now_ms();
    }
    if (m->val[i] == 9) {
        m->state = 2;
        for (int k = 0; k < 81; k++)
            if (m->val[k] == 9) m->st[k] = 1;
        term_bell();
        return;
    }
    int q[81], nh = 0;
    q[nh++] = i;
    while (nh) {
        int j = q[--nh];
        if (m->st[j] == 2) continue;
        if (m->st[j] == 1) continue;
        m->st[j] = 1;
        int xx = j % MW, yy = j / MW;
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) {
                if (!dx && !dy) continue;
                int nx = xx + dx, ny = yy + dy;
                if (nx < 0 || ny < 0 || nx >= MW || ny >= MH) continue;
                int k = ny * MW + nx;
                if (m->st[k] == 0 && m->val[k] == 0)
                    q[nh++] = k;
            }
    }
    int open = 0;
    for (int k = 0; k < 81; k++)
        if (m->st[k] == 1) open++;
    if (open == 81 - MMINES) {
        m->state = 1;
        for (int k = 0; k < 81; k++)
            if (m->val[k] == 9) m->st[k] = 2;
        term_bell();
    }
}

static const int numcol[9] = {
    C_BLACK, C_BLUE, C_GREEN, C_RED, C_BR_BLACK,
    C_BR_MAGENTA, C_CYAN, C_BLACK, C_BR_BLACK
};

static void mines_draw(win_t *w, int x, int y, int cw, int ch)
{
    mines_t *m = w->ctx;
    fill(x, y, cw, ch, T_CLI_TX, T_CLI);
    /* header boxes */
    char s[16];
    frame3d(x + 1, y + 1, 5, 3, T_CLI_TX, T_CLI, 0);
    snprintf(s, sizeof s, "%3d", MMINES - m->flags);
    txt(x + 3, y + 2, s, T_CLI_TX, T_CLI);
    frame3d(x + 7, y + 1, 5, 3, T_FACE_TXT, T_FACE, 1);
    const char *face = m->state == 2 ? "X)" : (m->state == 1 ? ":D" : ":)");
    txt(x + 9, y + 2, face, T_CLI_TX, T_FACE);
    frame3d(x + 13, y + 1, 5, 3, T_CLI_TX, T_CLI, 0);
    snprintf(s, sizeof s, "%3d", m->timer);
    txt(x + 15, y + 2, s, T_CLI_TX, T_CLI);
    /* board */
    frame3d(x + 1, y + 5, 19, 11, T_CLI_TX, T_CLI, 0);
    for (int j = 0; j < MH; j++)
        for (int i = 0; i < MW; i++) {
            int idx = j * MW + i;
            int px = x + 2 + i * 2, py = y + 6 + j;
            int cur = (i == m->cx && j == m->cy && m->state == 0);
            if (m->st[idx] == 2) {
                glyph(px, py, 0x2731u, cur ? T_SEL_TX : C_RED,
                      cur ? T_SEL : T_CLI);
            } else if (m->st[idx] == 1) {
                if (m->val[idx] == 9)
                    glyph(px, py, 0x25CFu, C_BLACK, C_RED);
                else if (m->val[idx] == 0)
                    glyph(px, py, 0, cur ? T_SEL_TX : T_CLI_TX,
                          cur ? T_SEL : T_CLI);
                else
                    glyph(px, py, (uint32_t)('0' + m->val[idx]),
                          cur ? T_SEL_TX : numcol[m->val[idx]],
                          cur ? T_SEL : T_CLI);
            } else {
                glyph(px, py, 0x2588u, cur ? T_SEL_TX : T_FACE,
                      cur ? T_SEL : T_CLI);
            }
        }
    /* footer */
    txt(x + 1, y + 16, "Left: reveal  Right: flag", T_CLI_TX, T_CLI);
    txt(x + 1, y + 17, "Arrows+F: keys  R: reset", T_CLI_TX, T_CLI);
}

static int mines_key(win_t *w, int key, int ch)
{
    mines_t *m = w->ctx;
    switch (key) {
    case K_LEFT:
        if (m->cx > 0) m->cx--;
        return 1;
    case K_RIGHT:
        if (m->cx < MW - 1) m->cx++;
        return 1;
    case K_UP:
        if (m->cy > 0) m->cy--;
        return 1;
    case K_DOWN:
        if (m->cy < MH - 1) m->cy++;
        return 1;
    case K_ENTER:
        mines_reveal(w, m->cx, m->cy);
        return 1;
    case K_DEL:
        return 1;
    default:
        break;
    }
    if (ch == 'f' || ch == 'F') {
        int i = m->cy * MW + m->cx;
        if (m->st[i] == 0) {
            m->st[i] = 2;
            m->flags++;
        } else if (m->st[i] == 2) {
            m->st[i] = 0;
            m->flags--;
        }
        return 1;
    }
    if (ch == 'r' || ch == 'R') {
        mines_new(w);
        return 1;
    }
    return 0;
}

static int mines_mouse(win_t *w, int x, int y, int btn, int moved)
{
    (void)moved;
    mines_t *m = w->ctx;
    if (btn == M_LEFT) {
        if (x >= 7 && x < 12 && y >= 1 && y < 4) {
            mines_new(w);
            return 1;
        }
        int i = x - 2, j = y - 6;
        if (i >= 0 && i < MW && j >= 0 && j < MH && (i % 2) == 0)
            mines_reveal(w, i / 2, j);
        return 1;
    }
    if (btn == -1 - M_RIGHT) {
        int i = x - 2, j = y - 6;
        if (i >= 0 && i < MW && j >= 0 && j < MH && (i % 2) == 0) {
            int idx = j * MW + i / 2;
            if (m->st[idx] == 0) {
                m->st[idx] = 2;
                m->flags++;
            } else if (m->st[idx] == 2) {
                m->st[idx] = 0;
                m->flags--;
            }
        }
        return 1;
    }
    return 1;
}

static int mines_tick(win_t *w)
{
    mines_t *m = w->ctx;
    if (m->started && m->state == 0) {
        int t = (int)((now_ms() - m->t0) / 1000);
        if (t > 999) t = 999;
        m->timer = t;
        return 1;
    }
    return 0;
}

static void mines_start(win_t *w)
{
    mines_t *m = calloc(1, sizeof *m);
    if (!m) return;
    w->ctx = m;
    mines_new(w);
}

static void mines_stop(win_t *w)
{
    free(w->ctx);
    w->ctx = 0;
}

/* ==================== Character Map ==================== */
typedef struct {
    int sel; /* 0..127 */
} cma_t;

static void cma_start(win_t *w)
{
    cma_t *c = calloc(1, sizeof *c);
    if (!c) return;
    c->sel = 0x25A0 - 0x2500;
    w->ctx = c;
}

static void cma_stop(win_t *w)
{
    free(w->ctx);
    w->ctx = 0;
}

static void cma_draw(win_t *w, int x, int y, int cw, int ch)
{
    cma_t *c = w->ctx;
    fill(x, y, cw, ch, T_CLI_TX, T_CLI);
    txt(x + 1, y + 1, "Box Drawing  (U+2500 - U+257F)", T_CLI_TX, T_CLI);
    for (int r = 0; r < 8; r++)
        for (int c2 = 0; c2 < 16; c2++) {
            int idx = r * 16 + c2;
            int cp = 0x2500 + idx;
            int sel = idx == c->sel;
            glyph(x + 2 + c2 * 2, y + 3 + r, (uint32_t)cp,
                  sel ? T_SEL_TX : T_CLI_TX, sel ? T_SEL : T_CLI);
        }
    char info[40];
    snprintf(info, sizeof info, "Selected: U+%04X", 0x2500 + c->sel);
    txt(x + 1, y + 12, info, T_CLI_TX, T_CLI);
    txt(x + 1, y + 13, "Arrows: move    Enter: beep", T_CLI_TX, T_CLI);
}

static int cma_key(win_t *w, int key, int ch)
{
    (void)ch;
    cma_t *c = w->ctx;
    switch (key) {
    case K_LEFT:
        if (c->sel > 0) c->sel--;
        return 1;
    case K_RIGHT:
        if (c->sel < 127) c->sel++;
        return 1;
    case K_UP:
        if (c->sel >= 16) c->sel -= 16;
        return 1;
    case K_DOWN:
        if (c->sel <= 111) c->sel += 16;
        return 1;
    case K_ENTER:
        term_bell();
        return 1;
    default:
        return 0;
    }
}

static int cma_mouse(win_t *w, int x, int y, int btn, int moved)
{
    (void)moved;
    if (btn != M_LEFT) return 1;
    cma_t *c = w->ctx;
    if (x >= 2 && x < 32 && y >= 3 && y < 11 && (x - 2) % 2 == 0) {
        int cc = (x - 2) / 2, rr = y - 3;
        if (cc >= 0 && cc < 16 && rr >= 0 && rr < 8)
            c->sel = rr * 16 + cc;
    }
    return 1;
}

static int cma_tick(win_t *w)
{
    (void)w;
    return 0;
}

/* ==================== Control Panel ==================== */
typedef struct {
    int sel;
} cpl_t;

static const char *cpl_items[6] = {
    "Mouse", "Keyboard", "Display", "Sound", "Network", "Date/Time"
};
static const char *cpl_det[5][4] = {
    { "Buttons: 3", "Speed: 4/10", "Dbl-click: Med", "" },
    { "Repeat delay: 1", "Repeat rate: 20", "Beep: on", "" },
    { "VGA 640x480", "16 colors", "Gray desktop", "" },
    { "Scheme: NT", "Startup: none", "Volume: 5", "" },
    { "Bindings:", " NetBIOS", " IPX/SPX", "" },
};

static void cpl_start(win_t *w)
{
    cpl_t *c = calloc(1, sizeof *c);
    if (!c) return;
    c->sel = (w->param >= 0 && w->param < 6) ? w->param : 0;
    w->ctx = c;
}

static void cpl_stop(win_t *w)
{
    free(w->ctx);
    w->ctx = 0;
}

static void cpl_draw(win_t *w, int x, int y, int cw, int ch)
{
    cpl_t *c = w->ctx;
    fill(x, y, cw, ch, T_CLI_TX, T_CLI);
    frame3d(x + 1, y + 1, 14, 8, T_CLI_TX, T_CLI, 0);
    for (int i = 0; i < 6; i++) {
        int sel = i == c->sel;
        if (sel) fill(x + 2, y + 1 + i, 12, 1, T_SEL_TX, T_SEL);
        txt(x + 3, y + 1 + i, cpl_items[i], sel ? T_SEL_TX : T_CLI_TX,
            sel ? T_SEL : T_CLI);
    }
    frame3d(x + 17, y + 1, 14, 8, T_CLI_TX, T_CLI, 0);
    if (c->sel == 5) {
        time_t t = time(0);
        struct tm *lt = localtime(&t);
        char d[48], tt[24];
        snprintf(d, sizeof d, "Date: %02d/%02d/%04d", lt->tm_mon + 1,
                 lt->tm_mday, lt->tm_year + 1900);
        snprintf(tt, sizeof tt, "Time: %02d:%02d:%02d", lt->tm_hour,
                 lt->tm_min, lt->tm_sec);
        txt(x + 18, y + 1, d, T_CLI_TX, T_CLI);
        txt(x + 18, y + 2, tt, T_CLI_TX, T_CLI);
    } else {
        for (int i = 0; i < 4; i++)
            txt(x + 18, y + 1 + i, cpl_det[c->sel][i], T_CLI_TX, T_CLI);
    }
    /* buttons */
    frame3d(x + 6, y + 11, 8, 2, T_FACE_TXT, T_FACE, 1);
    txt(x + 8, y + 11, "OK", T_FACE_TXT, T_FACE);
    frame3d(x + 16, y + 11, 8, 2, T_FACE_TXT, T_FACE, 1);
    txt(x + 17, y + 11, "Cancel", T_FACE_TXT, T_FACE);
}

static int cpl_key(win_t *w, int key, int ch)
{
    (void)ch;
    cpl_t *c = w->ctx;
    switch (key) {
    case K_UP:
        if (c->sel > 0) c->sel--;
        return 1;
    case K_DOWN:
        if (c->sel < 5) c->sel++;
        return 1;
    case K_ENTER:
        term_bell();
        close_win(w);
        return 1;
    case K_ESC:
        close_win(w);
        return 1;
    case K_LEFT:
    case K_RIGHT:
        return 1;
    default:
        return 0;
    }
}

static int cpl_mouse(win_t *w, int x, int y, int btn, int moved)
{
    (void)moved;
    if (btn != M_LEFT) return 1;
    cpl_t *c = w->ctx;
    if (x >= 1 && x < 15 && y >= 1 && y < 7) {
        c->sel = y - 1;
        return 1;
    }
    if (y >= 11 && y < 13) {
        if (x >= 6 && x < 14) term_bell();
        close_win(w);
        return 1;
    }
    return 1;
}

static int cpl_tick(win_t *w)
{
    (void)w;
    return 0;
}

/* ==================== About ==================== */
static void about_draw(win_t *w, int x, int y, int cw, int ch)
{
    (void)w; (void)cw; (void)ch;
    fill(x, y, 34, 13, T_CLI_TX, T_CLI);
    txtb(x + 2, y + 1, "Microsoft Windows NT", T_CLI_TX, T_CLI);
    txt(x + 2, y + 2, "Version 3.1 (build 103)", T_CLI_TX, T_CLI);
    txt(x + 2, y + 4, "Copyright (c) Microsoft Corp. 1981-1993.", T_CLI_TX, T_CLI);
    txt(x + 2, y + 6, "Computer name:   ARENA", T_CLI_TX, T_CLI);
    txt(x + 2, y + 7, "Logged on as:    Administrator", T_CLI_TX, T_CLI);
    char m[32];
    int f = mem_free_x10();
    snprintf(m, sizeof m, "Memory: %d.%d MB free", f / 10, f % 10);
    txt(x + 2, y + 8, m, T_CLI_TX, T_CLI);
    txt(x + 2, y + 10, "This product is licensed to: You", T_CLI_TX, T_CLI);
}

/* ==================== registry ==================== */
static const app_t apps[APP_MAX] = {
    [APP_CONSOLE] = { "Console", GLY_CONSOLE, 70, 20, C_BLACK,
                      con_start, con_stop, con_draw, con_key, con_mouse, con_tick },
    [APP_NOTEPAD] = { "Notepad", GLY_NOTEPAD, 52, 16, T_CLI,
                      np_start, np_stop, np_draw, np_key, np_mouse, np_tick },
    [APP_CALC] = { "Calculator", GLY_CALC, 20, 18, T_CLI,
                   calc_start, calc_stop, calc_draw, calc_key, calc_mouse, calc_tick },
    [APP_CLOCK] = { "Clock", GLY_CLOCK, 32, 6, T_CLI,
                    0, 0, clk_draw, 0, 0, clk_tick },
    [APP_MINES] = { "Minesweeper", GLY_MINES, 20, 19, T_CLI,
                    mines_start, mines_stop, mines_draw, mines_key, mines_mouse, mines_tick },
    [APP_CHARMAP] = { "Character Map", GLY_CHARMAP, 34, 15, T_CLI,
                      cma_start, cma_stop, cma_draw, cma_key, cma_mouse, cma_tick },
    [APP_CPL] = { "Control Panel", GLY_CPL, 32, 15, T_CLI,
                  cpl_start, cpl_stop, cpl_draw, cpl_key, cpl_mouse, cpl_tick },
    [APP_ABOUT] = { "About Windows NT", GLY_CHARMAP, 34, 13, T_CLI,
                    0, 0, about_draw, 0, 0, 0 },
};

const app_t *app_by_id(int id)
{
    if (id < 0 || id >= APP_MAX) return 0;
    return &apps[id];
}
