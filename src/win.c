/*
 * win.c - window manager + Program Manager shell for the NT31 recreation.
 *
 * A global z-ordered list of windows. Program Manager is the bottom
 * "window" filling the screen; group windows are its MDI children;
 * applications open as top-level windows.
 */
#include "nt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int dbg(void)
{
    static int d = -1;
    if (d < 0) d = getenv("NT31_DEBUG") != 0;
    return d;
}
#define DLOG(...) \
    do { if (dbg()) fprintf(stderr, __VA_ARGS__); } while (0)

/* ---------------- z-order list ---------------- */
static win_t *zhead, *ztail, *focus_w, *mdi_focus;
static win_t pmw;
static int next_id;
static win_t *drag_w;
static int drag_dx, drag_dy;
static int menu_open, menu_sel;
static int last_click_g;
static long long last_click_t;

static void zlink(win_t *w)
{
    w->next = 0;
    if (!zhead) zhead = ztail = w;
    else { ztail->next = w; ztail = w; }
}

static void zunlink(win_t *w)
{
    win_t *p = zhead;
    while (p && p->next != w) p = p->next;
    if (p)
        p->next = w->next;
    else if (zhead == w)
        zhead = w->next;
    w->next = 0;
    if (ztail == w) {
        ztail = zhead;
        while (ztail && ztail->next) ztail = ztail->next;
    }
}

static void zraise(win_t *w)
{
    if (w->mdi) {
        /* keep MDI children below every top-level window */
        win_t *top = 0;
        for (win_t *t = zhead; t; t = t->next)
            if (!t->mdi && t != &pmw) { top = t; break; }
        zunlink(w);
        if (!top) { zlink(w); return; }
        win_t *p = zhead;
        while (p && p->next != top) p = p->next;
        if (p) { w->next = p->next; p->next = w; }
        else { w->next = zhead; zhead = w; }
        return;
    }
    zunlink(w);
    zlink(w);
}

/* ---------------- window pool ---------------- */
static win_t pool[28];

static win_t *alloc_win(void)
{
    for (int i = 0; i < 28; i++)
        if (pool[i].id == 0) {
            memset(&pool[i], 0, sizeof pool[i]);
            pool[i].id = next_id++;
            return &pool[i];
        }
    return 0;
}

/* ---------------- geometry helpers ---------------- */
static int W_ = 80, H_ = 24;

static void WGet(void)
{
    int w, h;
    term_size(&w, &h);
    W_ = w; H_ = h;
}

/* PM client area (where MDI children live) */
#define PM_CX 1
#define PM_CY 3
static int pm_cw(void) { return W_ - 2; }
static int pm_ch(void) { return H_ - 6; }

static void move_win(win_t *w, int nx, int ny)
{
    int lo_x, hi_x, lo_y, hi_y;
    if (w->mdi) {
        lo_x = PM_CX; hi_x = PM_CX + pm_cw() - w->w;
        lo_y = PM_CY; hi_y = PM_CY + pm_ch() - w->h;
    } else {
        lo_x = 1; hi_x = W_ - w->w;
        lo_y = 1; hi_y = H_ - 3;
    }
    if (hi_x < lo_x) hi_x = lo_x;
    if (hi_y < lo_y) hi_y = lo_y;
    if (nx < lo_x) nx = lo_x;
    if (nx > hi_x) nx = hi_x;
    if (ny < lo_y) ny = lo_y;
    if (ny > hi_y) ny = hi_y;
    w->x = nx; w->y = ny;
}

/* ---------------- group (MDI child) data ---------------- */
typedef struct {
    uint32_t gl;
    const char *lab;
    int app;      /* app id, or -1 for message */
    int param;
    const char *msg;
} icon_t;

static icon_t grp_program[] = {
    { GLY_CONSOLE, "Console", APP_CONSOLE, 0, 0 },
    { GLY_NOTEPAD, "Notepad", APP_NOTEPAD, 0, 0 },
    { GLY_CALC, "Calculator", APP_CALC, 0, 0 },
    { GLY_CLOCK, "Clock", APP_CLOCK, 0, 0 },
    { GLY_MINES, "Minesweeper", APP_MINES, 0, 0 },
    { GLY_CHARMAP, "Character Map", APP_CHARMAP, 0, 0 },
    { GLY_CPL, "Control Panel", APP_CPL, 0, 0 },
    { GLY_SETUP, "Windows Setup", -1, 0,
      "Windows NT 3.1 is fully configured.\nThere is nothing to set up." },
};
static icon_t grp_fav[] = {
    { GLY_README, "Read Me", APP_NOTEPAD, 1, 0 },
    { GLY_MSDN, "MSDN", -1, 0,
      "MSDN Windows NT 3.1\n\nThis machine was shipped without the\nMSDN CD-ROM." },
    { GLY_NET, "Internet", -1, 0,
      "The Internet is a collection of\ninterconnected networks.\n\nAsk your network administrator\nfor more information." },
};
static icon_t grp_start[] = {
    { GLY_MAIL, "Mail", -1, 0,
      "You have no mail.\n\n(POSTNOTES is not supported\nin this build.)" },
};
static icon_t grp_setup[] = {
    { GLY_DISP, "Display", APP_CPL, 2, 0 },
    { GLY_NET, "Network", APP_CPL, 4, 0 },
    { GLY_PRINTER, "Add Printer", -1, 0,
      "No printers were detected\non this computer." },
};

static struct {
    const char *name;
    icon_t *icons;
    int n;
} groups[4] = {
    { "Program", grp_program, 8 },
    { "Favorites", grp_fav, 3 },
    { "Startup", grp_start, 1 },
    { "Windows Setup", grp_setup, 3 },
};
static const uint32_t group_gly[4] = { GLY_PROGRAM, GLY_FAV, GLY_START, GLY_SETUPG };

/* icon slot geometry (absolute screen coords) */
static int slot_x(win_t *g, int i) { return g->x + 2 + (i % 4) * 12; }
static int slot_y(win_t *g, int i) { return g->y + 3 + (i / 4) * 6; }

static int hit_icon(win_t *g, int ex, int ey)
{
    int n = groups[g->param].n;
    for (int i = 0; i < n; i++)
        if (ex >= slot_x(g, i) && ex < slot_x(g, i) + 12 &&
            ey >= slot_y(g, i) && ey < slot_y(g, i) + 6)
            return i;
    return -1;
}

static void icon_act(win_t *g, int i)
{
    icon_t *ic = &groups[g->param].icons[i];
    DLOG("  icon_act group=%s icon=%d app=%d\n",
         groups[g->param].name, i, ic->app);
    if (ic->app >= 0) launch_app(ic->app, ic->param);
    else if (ic->msg) dialog_msg(ic->lab, ic->msg);
}

/* ---------------- menus ---------------- */
static const char *bar_items[] = { "System", "File", "Window", "Help" };
static const int bar_x[] = { 2, 11, 18, 27 };
static const char *sys_items[] = {
    "Change Password...", "Control Panel...", "Logon User...",
    "About Windows...", "", "Exit Windows NT...", 0 };
static const char *file_items[] = { "Print...", "Close", 0 };
static const char *win_items[] = {
    "Cascade", "Tile", "Arrange Icons", "Next Window", "Restore Minimized", 0 };
static const char *help_items[] = { "Help Topics...", "About Windows...", 0 };

static const char **bar_menu(int b)
{
    switch (b) {
    case 0: return sys_items;
    case 1: return file_items;
    case 2: return win_items;
    default: return help_items;
    }
}

static void arrange(int tile);
static void next_window(void);
static void restore_min(void);
static void pwd_modal(void);

static void menu_act(int bar, int idx)
{
    switch (bar) {
    case 0:
        switch (idx) {
        case 0: pwd_modal(); break;
        case 1: launch_app(APP_CPL, 0); break;
        case 2: dialog_msg("Logon User",
                           "You are logged on as:\n\n  Administrator\n  (domain: ARENA)");
            break;
        case 3: launch_app(APP_ABOUT, 0); break;
        case 5:
            if (dialog_ask("Exit Windows NT",
                           "Are you sure you want to exit Windows NT?"))
                shutdown_os();
            break;
        }
        break;
    case 1:
        switch (idx) {
        case 0: dialog_msg("Print", "No printer is installed on this computer."); break;
        case 1:
            if (dialog_ask("Exit Windows NT",
                           "Are you sure you want to exit Windows NT?"))
                shutdown_os();
            break;
        }
        break;
    case 2:
        switch (idx) {
        case 0: arrange(0); break;
        case 1: arrange(1); break;
        case 2: term_bell(); break;
        case 3: next_window(); break;
        case 4: restore_min(); break;
        }
        break;
    case 3:
        switch (idx) {
        case 0:
            dialog_msg("Help Topics",
                       "  Left click ... select / activate\n"
                       "  Double click . open icon\n"
                       "  Mouse drag ... move window\n"
                       "  Arrows ....... move window / select\n"
                       "  Enter ........ activate\n"
                       "  Tab .......... cycle group windows\n"
                       "  Esc .......... close window\n"
                       "  F10 .......... open menu bar\n"
                       "  Ctrl+C ....... quit");
            break;
        case 1: launch_app(APP_ABOUT, 0); break;
        }
        break;
    }
}

/* ---------------- dialog primitives ---------------- */
static void dbox(int x, int y, int w, int h, const char *title)
{
    frame3d(x, y, w, h, T_CLI_TX, T_CLI, 1);
    fill(x + 1, y + 1, w - 2, 1, T_TITLE_TX, T_TITLE);
    if (title)
        txt(x + (w - (int)strlen(title)) / 2, y + 1, title, T_TITLE_TX, T_TITLE);
}

static void dbtn(int x, int y, const char *s, int sel)
{
    int w = 8, h = 2;
    if (sel) fill(x, y, w, h, T_SEL_TX, T_SEL);
    else fill(x, y, w, h, T_FACE_TXT, T_FACE);
    frame3d(x, y, w, h, T_FACE_TXT, T_FACE, !sel);
    txt(x + (w - (int)strlen(s)) / 2, y, s, sel ? T_SEL_TX : T_FACE_TXT,
        sel ? T_SEL : T_FACE);
}

static void tick_apps(void)
{
    for (win_t *w = zhead; w; w = w->next)
        if (!w->mdi && w->vis && w->app && w->app->tick)
            w->app->tick(w);
}

static int dialog_run(const char *title, const char *msg,
                      const char **btns, int nbtns)
{
    int nln = 1;
    for (const char *p = msg; *p; p++)
        if (*p == '\n') nln++;
    int w = 46;
    if (w > W_ - 4) w = W_ - 4;
    int h = nln + 6;
    if (h > H_ - 2) h = H_ - 2;
    int x = (W_ - w) / 2, y = (H_ - h) / 2;
    int sel = nbtns == 1 ? 0 : 1; /* default: No */
    for (;;) {
        desktop_frame();
        flush();
        dbox(x, y, w, h, title);
        const char *p = msg;
        int ry = y + 3;
        for (int i = 0; i < nln && ry < y + h - 3; i++) {
            int len = 0;
            while (p[len] && p[len] != '\n') len++;
            txt(x + 2, ry, p, T_CLI_TX, T_CLI);
            p += len + 1;
            ry++;
        }
        int bx = x + w - 2 - (nbtns == 1 ? 8 : 17);
        for (int i = 0; i < nbtns; i++) {
            dbtn(bx, y + h - 2, btns[i], sel == i);
            bx += 9;
        }
        flush();
        ev_t ev;
        if (!term_get(&ev, 400)) {
            tick_apps();
            continue;
        }
        int done = -1;
        if (ev.type == EV_KEY) {
            switch (ev.key) {
            case K_LEFT:
            case K_RIGHT:
            case K_TAB:
                if (nbtns > 1) sel ^= 1;
                break;
            case K_ENTER:
                done = sel;
                break;
            case K_ESC:
                done = nbtns == 1 ? 0 : 1;
                break;
            default:
                break;
            }
        } else if (ev.type == EV_CHAR && nbtns == 2) {
            if (ev.ch == 'y' || ev.ch == 'Y') done = 0;
            if (ev.ch == 'n' || ev.ch == 'N') done = 1;
        } else if (ev.type == EV_MOUSE && ev.btn == M_LEFT) {
            if (ev.y >= y + h - 2 && ev.y < y + h && ev.x >= x && ev.x < x + w) {
                int bx2 = x + w - 2 - (nbtns == 1 ? 8 : 17);
                for (int i = 0; i < nbtns; i++)
                    if (ev.x >= bx2 && ev.x < bx2 + 8) done = i;
            } else if (ev.x < x || ev.x >= x + w || ev.y < y || ev.y >= y + h) {
                done = nbtns == 1 ? 0 : 1;
            }
        } else if (ev.type == EV_QUIT) {
            done = nbtns == 1 ? 0 : 1;
        }
        if (done >= 0) return done;
    }
}

void dialog_msg(const char *title, const char *msg)
{
    const char *b[] = { "OK" };
    dialog_run(title, msg, b, 1);
}

int dialog_ask(const char *title, const char *msg)
{
    const char *b[] = { "Yes", "No" };
    return dialog_run(title, msg, b, 2) == 0; /* 1 = Yes */
}

/* ---------------- change-password modal ---------------- */
static void pwd_modal(void)
{
    char oldp[13] = "", nw[13] = "", cf[13] = "";
    char *flds[3] = { oldp, nw, cf };
    const char *labels[3] = { "Old password:    ", "New password:    ",
                              "Confirm new:     " };
    int sel = 0; /* 0..2 fields, 3 = OK, 4 = Cancel */
    int w = 46, h = 12;
    int x = (W_ - w) / 2, y = (H_ - h) / 2;
    for (;;) {
        desktop_frame();
        flush();
        dbox(x, y, w, h, "Change Password");
        for (int i = 0; i < 3; i++) {
            char mask[13];
            int n = (int)strlen(flds[i]);
            if (n > 12) n = 12;
            for (int k = 0; k < n; k++) mask[k] = '*';
            mask[n] = 0;
            txt(x + 3, y + 3 + i, labels[i], T_CLI_TX, T_CLI);
            txt(x + 20, y + 3 + i, mask, sel == i ? T_SEL_TX : T_CLI_TX,
                sel == i ? T_SEL : T_CLI);
        }
        dbtn(x + w - 19, y + h - 2, "OK", sel == 3);
        dbtn(x + w - 10, y + h - 2, "Cancel", sel == 4);
        flush();
        ev_t ev;
        if (!term_get(&ev, 400)) {
            tick_apps();
            continue;
        }
        int done = -1;
        if (ev.type == EV_KEY) {
            switch (ev.key) {
            case K_UP:
            case K_LEFT:
            case K_TAB:
                sel = (sel + 1) % 5;
                break;
            case K_DOWN:
            case K_RIGHT:
                sel = (sel + 4) % 5;
                break;
            case K_BACK:
                if (sel < 3) {
                    char *v = flds[sel];
                    int n = (int)strlen(v);
                    if (n) v[n - 1] = 0;
                }
                break;
            case K_ENTER:
                if (sel < 3) sel = (sel + 1) % 5;
                else done = sel;
                break;
            case K_ESC:
                done = 4;
                break;
            default:
                break;
            }
        } else if (ev.type == EV_CHAR && sel < 3) {
            char *v = flds[sel];
            int n = (int)strlen(v);
            if (n < 12) {
                v[n] = (char)ev.ch;
                v[n + 1] = 0;
            }
        } else if (ev.type == EV_MOUSE && ev.btn == M_LEFT) {
            if (ev.x >= x + 3 && ev.x < x + 20 && ev.y >= y + 3 && ev.y < y + 6)
                sel = ev.y - (y + 3);
            else if (ev.x >= x + w - 19 && ev.x < x + w - 11 &&
                     ev.y >= y + h - 2 && ev.y < y + h)
                done = 3;
            else if (ev.x >= x + w - 10 && ev.x < x + w - 2 &&
                     ev.y >= y + h - 2 && ev.y < y + h)
                done = 4;
        } else if (ev.type == EV_QUIT) {
            done = 4;
        }
        if (done >= 0) {
            if (done == 3) {
                if (nw[0] == 0 || strcmp(nw, cf) != 0)
                    dialog_msg("Change Password",
                               "The passwords do not match.\nTry again.");
                else
                    dialog_msg("Change Password",
                               "Your password has been changed.");
            }
            return;
        }
    }
}

/* ---------------- drawing ---------------- */
static void draw_titlebar(win_t *w)
{
    int x = w->x, y = w->y, ww = w->w;
    int act = (focus_w == w) || (mdi_focus == w);
    int tb = act ? T_TITLE : T_TITLE_OFF;
    int tf = act ? T_TITLE_TX : T_TITLE_OFT;
    fill(x + 1, y + 1, ww - 2, 1, tf, tb);
    /* close box (all windows except PM) */
    int cx0 = x + ww - 4;
    glyph(cx0, y + 1, 0, T_LT, T_FACE);
    txt(cx0 + 1, y + 1, "x", T_FACE_TXT, T_FACE);
    glyph(cx0 + 2, y + 1, 0, T_DK, T_FACE);
    if (w->mdi) {
        int mx0 = x + ww - 7;
        glyph(mx0, y + 1, 0, T_LT, T_FACE);
        txt(mx0 + 1, y + 1, "_", T_FACE_TXT, T_FACE);
        glyph(mx0 + 2, y + 1, 0, T_DK, T_FACE);
    }
    /* title text (MDI children show their group glyph before the name) */
    int right = x + ww - (w->mdi ? 8 : 5);
    char buf[72];
    snprintf(buf, sizeof buf, " %s", w->title);
    int bw = (int)strlen(buf);
    int bx = x + 2 + (right - (x + 2) - bw) / 2;
    if (bx < x + 2) bx = x + 2;
    txt(bx, y + 1, buf, tf, tb);
    if (w->mdi && w->param >= 0 && w->param < 4)
        glyph(bx, y + 1, group_gly[w->param], tf, tb);
}

static void draw_win(win_t *w)
{
    int x = w->x, y = w->y, ww = w->w, h = w->h;
    fill(x, y, ww, h, T_FACE_TXT, T_FACE);
    frame3d(x, y, ww, h, T_FACE_TXT, T_FACE, 1);
    if (w->app) {
        draw_titlebar(w);
        int cx = x + 1, cy = y + 2, cw = ww - 2, ch = h - 3;
        fill(cx, cy, cw, ch, T_CLI_TX, w->app->bg);
        if (w->app->draw) w->app->draw(w, cx, cy, cw, ch);
    } else if (w->mdi) {
        /* group window */
        draw_titlebar(w);
        fill(x + 1, y + 2, ww - 2, h - 3, T_FACE_TXT, T_FACE);
        icon_t *ic = groups[w->param].icons;
        int n = groups[w->param].n;
        int act = (mdi_focus == w);
        for (int i = 0; i < n; i++) {
            int sx = slot_x(w, i), sy = slot_y(w, i);
            int sel = act && w->sel_icon == i;
            frame3d(sx + 1, sy, 10, 4, T_CLI_TX, T_CLI, !sel);
            glyph(sx + 5, sy + 1, ic[i].gl, T_CLI_TX, T_CLI);
            char lab[13];
            const char *src = ic[i].lab;
            int k = 0;
            while (src[k] && k < 12) k++;
            memcpy(lab, src, (size_t)k);
            lab[k] = 0;
            int lw = k;
            int lx = sx + (12 - lw) / 2;
            if (sel) {
                fill(lx, sy + 5, lw, 1, T_SEL_TX, T_SEL);
                txt(lx, sy + 5, lab, T_SEL_TX, T_SEL);
            } else {
                txt(lx, sy + 5, lab, T_FACE_TXT, T_FACE);
            }
        }
    }
}

static void draw_pm(void)
{
    pmw.w = W_;
    pmw.h = H_;
    fill(0, 0, W_, H_, T_FACE_TXT, T_FACE);
    frame3d(0, 0, W_, H_, T_FACE_TXT, T_FACE, 1);
    /* title */
    fill(1, 1, W_ - 2, 1, T_TITLE_TX, T_TITLE);
    txt((W_ - (int)strlen("Program Manager")) / 2, 1, "Program Manager",
        T_TITLE_TX, T_TITLE);
    /* menu bar */
    fill(1, 2, W_ - 2, 1, T_FACE_TXT, T_FACE);
    for (int i = 0; i < 4; i++) {
        int len = (int)strlen(bar_items[i]);
        if (menu_open == i)
            fill(bar_x[i], 2, len, 1, T_SEL_TX, T_SEL);
        txt(bar_x[i], 2, bar_items[i], menu_open == i ? T_SEL_TX : T_FACE_TXT,
            menu_open == i ? T_SEL : T_FACE);
    }
    /* MDI client */
    fill(PM_CX, PM_CY, pm_cw(), pm_ch(), T_FACE_TXT, T_FACE);
    /* status bar */
    fill(1, H_ - 2, W_ - 2, 1, T_FACE_TXT, T_FACE);
    const char *left = focus_w && focus_w != &pmw ? focus_w->title : "Program Manager";
    txt(2, H_ - 2, left, T_FACE_TXT, T_FACE);
    char mem[32];
    int f = mem_free_x10();
    snprintf(mem, sizeof mem, "Memory: %d.%d MB free", f / 10, f % 10);
    txt(W_ - 3 - (int)strlen(mem), H_ - 2, mem, T_FACE_TXT, T_FACE);
}

static void draw_menu_drop(void)
{
    if (menu_open < 0) return;
    const char **items = bar_menu(menu_open);
    int n = 0, w = 10;
    while (items[n]) {
        int len = (int)strlen(items[n]);
        if (len + 2 > w) w = len + 2;
        n++;
    }
    int x = bar_x[menu_open], y = 4, h = n + 2;
    if (x + w > W_ - 1) x = W_ - 1 - w;
    if (y + h > H_ - 1) y = H_ - 1 - h;
    frame3d(x, y, w, h, T_CLI_TX, T_CLI, 1);
    for (int i = 0; i < n; i++) {
        if (items[i][0] == 0) {
            fill(x + 1, y + 1 + i, w - 2, 1, T_DK, T_CLI);
            continue;
        }
        int sel = i == menu_sel;
        fill(x + 1, y + 1 + i, w - 2, 1, sel ? T_SEL_TX : T_CLI_TX,
             sel ? T_SEL : T_CLI);
        txt(x + 2, y + 1 + i, items[i], sel ? T_SEL_TX : T_CLI_TX,
            sel ? T_SEL : T_CLI);
    }
}

void desktop_frame(void)
{
    WGet();
    fill_all(T_DESK_TXT, T_DESK);
    draw_pm();
    for (win_t *w = zhead; w; w = w->next) {
        if (w == &pmw || !w->vis || w->min) continue;
        if (w->mdi) term_clip(PM_CX, PM_CY, pm_cw(), pm_ch());
        draw_win(w);
        term_clip_none();
    }
    draw_menu_drop();
}

/* ---------------- hit testing ---------------- */
win_t *win_at(int x, int y)
{
    win_t *hit = 0;
    for (win_t *w = zhead; w; w = w->next) {
        if (!w->vis || w->min) continue;
        if (w == &pmw) continue;
        if (w->mdi && (x < PM_CX || x >= PM_CX + pm_cw() ||
                       y < PM_CY || y >= PM_CY + pm_ch()))
            continue;
        if (x >= w->x && x < w->x + w->w && y >= w->y && y < w->y + w->h)
            hit = w;
    }
    if (hit) return hit;
    if (x >= 0 && x < W_ && y >= 0 && y < H_) return &pmw;
    return 0;
}

/* ---------------- window lifecycle ---------------- */
win_t *launch_app(int app_id, int param)
{
    const app_t *a = app_by_id(app_id);
    if (!a) return 0;
    win_t *w = alloc_win();
    if (!w) return 0;
    w->w = a->cw + 2;
    w->h = a->ch + 3;
    if (w->w > W_ - 2) w->w = W_ - 2;
    if (w->h > H_ - 3) w->h = H_ - 3;
    int open = 0;
    for (win_t *t = zhead; t; t = t->next)
        if (!t->mdi && t != &pmw) open++;
    w->x = 6 + (open % 6) * 4;
    w->y = 2 + (open % 6) * 2;
    move_win(w, w->x, w->y);
    snprintf(w->title, sizeof w->title, "%s", a->name);
    w->app = a;
    w->param = param;
    w->vis = w->focus = 1;
    w->mdi = 0;
    w->ctx = 0;
    if (a->start) a->start(w);
    zlink(w);
    zraise(w);
    focus_w = w;
    mem_take(250 + rand() % 350);
    proc_add(a->name);
    return w;
}

void close_win(win_t *w)
{
    if (w == &pmw) return;
    if (w->app) {
        if (w->app->stop) w->app->stop(w);
        w->ctx = 0;
        proc_drop(w->app->name);
    }
    mem_release(400);
    zunlink(w);
    w->id = 0;
    if (focus_w == w) focus_w = &pmw;
    if (mdi_focus == w) {
        mdi_focus = 0;
        for (win_t *t = zhead; t; t = t->next)
            if (t->mdi && t->vis && !t->min) { mdi_focus = t; break; }
    }
}

/* ---------------- window menu commands ---------------- */
static void arrange(int tile)
{
    int i = 0;
    for (win_t *w = zhead; w; w = w->next) {
        if (!w->mdi || w->min) continue;
        if (tile) {
            int col = i % 2, row = i / 2;
            move_win(w, PM_CX + col * 26, PM_CY + row * 10);
        } else {
            move_win(w, PM_CX + i * 4, PM_CY + i * 4);
        }
        i++;
    }
}

static void next_window(void)
{
    win_t *cur = mdi_focus;
    for (win_t *w = cur ? cur->next : zhead; w; w = w->next)
        if (w->mdi && w->vis && !w->min) {
            mdi_focus = w;
            zraise(w);
            return;
        }
    for (win_t *w = zhead; w; w = w->next)
        if (w->mdi && w->vis && !w->min) {
            mdi_focus = w;
            zraise(w);
            return;
        }
}

static void restore_min(void)
{
    int i = 0;
    for (win_t *w = zhead; w; w = w->next)
        if (w->mdi && w->min) {
            w->min = 0;
            w->vis = 1;
            move_win(w, PM_CX + i * 4, PM_CY + i * 4);
            i++;
        }
}

/* ---------------- shutdown ---------------- */
void shutdown_os(void)
{
    int sw, sh;
    term_size(&sw, &sh);
    fill_all(0, C_BLACK);
    flush();
    const char *s1 = "Windows NT is shutting down...";
    txt((sw - (int)strlen(s1)) / 2, sh / 2 - 1, s1, C_WHITE, C_BLACK);
    flush();
    term_bell();
    msleep(fast_mode() ? 200 : 1100);
    fill_all(0, C_BLACK);
    flush();
    const char *s2 = "It is now safe to turn off your computer.";
    txt((sw - (int)strlen(s2)) / 2, sh / 2, s2, C_WHITE, C_BLACK);
    flush();
    long long t0 = now_ms();
    for (;;) {
        ev_t ev;
        if (term_get(&ev, 500)) {
            if ((ev.type == EV_KEY && ev.key == K_ENTER) || ev.type == EV_QUIT ||
                (ev.type == EV_CHAR && (ev.ch == '\r' || ev.ch == '\n')))
                break;
        }
        if (now_ms() - t0 > 5000) break;
    }
    term_shutdown();
    exit(0);
}

/* ---------------- Ctrl-C ---------------- */
static int quit_pending;

int desktop_quit(void)
{
    if (quit_pending) {
        quit_pending = 0;
        return 1;
    }
    quit_pending = 1;
    int r = dialog_ask("Exit Windows NT", "Are you sure you want to exit Windows NT?");
    quit_pending = 0;
    return r;
}

/* ---------------- tick ---------------- */
int desktop_tick(void)
{
    int any = 0;
    for (win_t *w = zhead; w; w = w->next)
        if (!w->mdi && w->vis && w->app && w->app->tick)
            any |= w->app->tick(w);
    return any;
}

/* ---------------- init ---------------- */
void desktop_init(void)
{
    WGet();
    zhead = ztail = 0;
    memset(pool, 0, sizeof pool);
    next_id = 100;
    memset(&pmw, 0, sizeof pmw);
    pmw.id = 1;
    pmw.w = W_;
    pmw.h = H_;
    snprintf(pmw.title, sizeof pmw.title, "Program Manager");
    pmw.vis = pmw.focus = 1;
    zlink(&pmw);
    focus_w = &pmw;
    menu_open = -1;
    menu_sel = 0;
    for (int i = 0; i < 4; i++) {
        win_t *g = alloc_win();
        if (!g) break;
        g->mdi = 1;
        g->w = 52;
        g->h = 17;
        g->x = PM_CX + i * 4;
        g->y = PM_CY + i * 4;
        snprintf(g->title, sizeof g->title, "%s", groups[i].name);
        g->param = i;
        g->sel_icon = -1;
        g->vis = g->focus = 1;
        zlink(g);
        if (!mdi_focus) mdi_focus = g;
    }
    term_bell();
}

void desktop_shutdown(void)
{
    /* state lives in statics; nothing to free */
}

static void open_menu(int i)
{
    menu_open = i;
    const char **items = bar_menu(i);
    menu_sel = 0;
    while (items[menu_sel] && items[menu_sel][0] == 0) menu_sel++;
}

static void close_menu(void)
{
    menu_open = -1;
}

static void menu_key(const ev_t *ev)
{
    const char **items = bar_menu(menu_open);
    int n = 0;
    while (items[n]) n++;
    switch (ev->key) {
    case K_LEFT:
        open_menu((menu_open + 3) % 4);
        break;
    case K_RIGHT:
        open_menu((menu_open + 1) % 4);
        break;
    case K_UP:
    case K_DOWN: {
        int d = ev->key == K_DOWN ? 1 : -1;
        int guard = 0;
        while (guard++ < n) {
            menu_sel = (menu_sel + d + n) % n;
            if (items[menu_sel][0] != 0) break;
        }
        break;
    }
    case K_TAB:
        open_menu((menu_open + 1) % 4);
        break;
    case K_ENTER:
        if (items[menu_sel] && items[menu_sel][0] != 0) {
            int bar = menu_open;
            int idx = menu_sel;
            close_menu();
            menu_act(bar, idx);
        }
        break;
    case K_ESC:
    case K_F10:
        close_menu();
        break;
    default:
        break;
    }
}

static void group_key(win_t *g, int key)
{
    int n = groups[g->param].n;
    switch (key) {
    case K_LEFT:
        if (g->sel_icon > 0) g->sel_icon--;
        else g->sel_icon = -1;
        break;
    case K_RIGHT:
        if (g->sel_icon < 0) g->sel_icon = 0;
        else if (g->sel_icon < n - 1) g->sel_icon++;
        break;
    case K_UP:
        if (g->sel_icon >= 4 && g->sel_icon - 4 >= 0) g->sel_icon -= 4;
        break;
    case K_DOWN:
        if (g->sel_icon >= 0 && g->sel_icon + 4 < n) g->sel_icon += 4;
        else if (g->sel_icon < 0) g->sel_icon = 0;
        break;
    case K_ENTER:
        if (g->sel_icon >= 0) icon_act(g, g->sel_icon);
        break;
    case K_TAB: {
        win_t *next = g->next;
        while (next && (!next->mdi || !next->vis || next->min)) next = next->next;
        if (!next) next = mdi_focus;
        for (win_t *t = zhead; t; t = t->next)
            if (t->mdi && t->vis && !t->min) { next = t; break; }
        if (next) {
            mdi_focus = next;
            zraise(next);
        }
        break;
    }
    default:
        break;
    }
}

void desktop_event(const ev_t *ev)
{
    WGet();
    DLOG("ev type=%d key=%d ch=%d btn=%d wheel=%d x=%d y=%d\n",
         ev->type, ev->key, ev->ch, ev->btn, ev->wheel, ev->x, ev->y);
    /* ---- menus capture everything ---- */
    if (menu_open >= 0) {
        if (ev->type == EV_KEY) {
            menu_key(ev);
            return;
        }
        if (ev->type == EV_MOUSE && ev->btn == M_LEFT) {
            const char **items = bar_menu(menu_open);
            int n = 0;
            while (items[n]) n++;
            int w = 10;
            for (int i = 0; i < n; i++)
                if ((int)strlen(items[i]) + 2 > w) w = (int)strlen(items[i]) + 2;
            int x = bar_x[menu_open], y = 4, h = n + 2;
            if (x + w > W_ - 1) x = W_ - 1 - w;
            if (y + h > H_ - 1) y = H_ - 1 - h;
            if (ev->y >= 2 && ev->y <= 2 &&
                ev->x >= bar_x[menu_open] &&
                ev->x < bar_x[menu_open] + (int)strlen(bar_items[menu_open])) {
                close_menu();
                return;
            }
            if (ev->x >= x && ev->x < x + w && ev->y >= y && ev->y < y + h) {
                int idx = ev->y - y - 1;
                if (idx >= 0 && idx < n && items[idx][0] != 0) {
                    int bar = menu_open;
                    close_menu();
                    menu_act(bar, idx);
                }
                return;
            }
            /* click on another bar item */
            if (ev->y == 2) {
                for (int i = 0; i < 4; i++)
                    if (ev->x >= bar_x[i] &&
                        ev->x < bar_x[i] + (int)strlen(bar_items[i])) {
                        open_menu(i);
                        return;
                    }
            }
            close_menu();
        }
        return;
    }

    /* ---- mouse ---- */
    if (ev->type == EV_MOUSE) {
        if (ev->btn == M_MOVE && drag_w) {
            move_win(drag_w, ev->x - drag_dx, ev->y - drag_dy);
            return;
        }
        if (drag_w) {
            drag_w = 0;
            return;
        }
        if (ev->btn == M_LEFT || ev->btn == M_RIGHT) {
            win_t *w = win_at(ev->x, ev->y);
            DLOG("  win_at(%d,%d) -> %s (id=%d)\n", ev->x, ev->y,
                 w ? (w == &pmw ? "PM" : w->title) : "none", w ? w->id : 0);
            if (!w) return;
            if (w == &pmw) {
                if (ev->y == 2 && ev->btn == M_LEFT) {
                    for (int i = 0; i < 4; i++)
                        if (ev->x >= bar_x[i] &&
                            ev->x < bar_x[i] + (int)strlen(bar_items[i])) {
                            open_menu(i);
                            return;
                        }
                }
                focus_w = &pmw;
                /* clicking empty PM client: focus first visible group */
                mdi_focus = 0;
                for (win_t *t = zhead; t; t = t->next)
                    if (t->mdi && t->vis && !t->min) { mdi_focus = t; break; }
                return;
            }
            if (ev->y == w->y + 1) {
                /* title bar */
                int close0 = w->x + w->w - 4;
                int min0 = w->x + w->w - 7;
                if (ev->btn == M_LEFT) {
                    if (ev->x >= close0 && ev->x < close0 + 3) {
                        if (w->mdi) {
                            term_bell();
                            dialog_msg("Program Manager",
                                       "You cannot close this group window.");
                        } else {
                            close_win(w);
                        }
                        return;
                    }
                    if (w->mdi && ev->x >= min0 && ev->x < min0 + 3) {
                        w->min = 1;
                        w->vis = 0;
                        if (mdi_focus == w) {
                            mdi_focus = 0;
                            for (win_t *t = zhead; t; t = t->next)
                                if (t->mdi && t->vis && !t->min) { mdi_focus = t; break; }
                        }
                        term_bell();
                        return;
                    }
                    /* start drag */
                    drag_w = w;
                    drag_dx = ev->x - w->x;
                    drag_dy = ev->y - w->y;
                    zraise(w);
                    if (w->mdi) mdi_focus = w;
                    else focus_w = w;
                }
                return;
            }
            /* client area */
            if (w->mdi) {
                mdi_focus = w;
                zraise(w);
                int i = hit_icon(w, ev->x, ev->y);
                if (i >= 0 && ev->btn == M_LEFT) {
                    long long now = now_ms();
                    DLOG("  dblclick? icon=%d sel=%d last_g=%d dt=%lld\n",
                         i, w->sel_icon, last_click_g,
                         now - last_click_t);
                    if (w->sel_icon == i && last_click_g == w->id &&
                        now - last_click_t < 500)
                        icon_act(w, i);
                    else {
                        w->sel_icon = i;
                        last_click_g = w->id;
                        last_click_t = now;
                    }
                }
                return;
            }
            /* top-level app */
            if (focus_w != w) {
                focus_w = w;
                zraise(w);
            }
            if (w->app && w->app->mouse)
                w->app->mouse(w, ev->x - (w->x + 1), ev->y - (w->y + 2),
                              ev->btn, 0);
            return;
        }
        return;
    }

    /* ---- keys / chars ---- */
    if (ev->type == EV_KEY || ev->type == EV_CHAR) {
        win_t *w = focus_w;
        if (w && w != &pmw && w->app && !w->min) {
            if (w->app->key && w->app->key(w, ev->key, ev->ch))
                return;
            /* app did not handle it: move the window */
            if (ev->key == K_UP) move_win(w, w->x, w->y - 1);
            else if (ev->key == K_DOWN) move_win(w, w->x, w->y + 1);
            else if (ev->key == K_LEFT) move_win(w, w->x - 1, w->y);
            else if (ev->key == K_RIGHT) move_win(w, w->x + 1, w->y);
            else if (ev->key == K_ESC) close_win(w);
            return;
        }
        /* Program Manager level */
        switch (ev->key) {
        case K_F10:
            open_menu(0);
            break;
        case K_ESC:
            if (mdi_focus) term_bell();
            break;
        default:
            if (mdi_focus && (ev->key == K_LEFT || ev->key == K_RIGHT ||
                              ev->key == K_UP || ev->key == K_DOWN ||
                              ev->key == K_ENTER || ev->key == K_TAB))
                group_key(mdi_focus, ev->key);
            break;
        }
        return;
    }
}
