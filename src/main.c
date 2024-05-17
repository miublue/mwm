#include <X11/Xlib.h>
#include <X11/XF86keysym.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>

#include <inttypes.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include "mlist.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define MOD Mod4Mask

typedef struct client_t {
    uint32_t x, y, w, h;
    bool fullscreen;
    Window window;
} client_t;

LIST_DEFINE(client_t, list_client_t);

enum {
    MODE_FLOAT,
    MODE_TILE,
    MODE_MONOCLE,

    NUM_MODES,
};

#define TOPGAP 18
int mode = MODE_TILE;
list_client_t list;
size_t cur = 0;
bool focus_follows_pointer = false;

// TODO: multiple workspaces

static void win_focus(size_t c);
static void win_center();
static void win_tile();
static void win_prev();
static void win_next();
static void win_kill();
static void win_get_size();
static void win_fullscreen();
static void win_move(int wn, int x, int y);
static void win_resize(int wn, int w, int h);
static void win_swap(int a, int b);
static void win_rotate_next();
static void win_rotate_prev();
static void win_add(Window w);
static void win_del(Window w);
static void exec(char **cmd);

void grab_key(Window root, uint32_t mod, KeySym key);
void key_press(XEvent *ev);

const char *term_cmd[]  = { "mterm",    NULL };
const char *menu_cmd[]  = { "dmenu_run", NULL };

Display *display;
Window root;
XButtonEvent start;
XWindowAttributes attr;
uint32_t screen_w, screen_h;

static void
win_get_size()
{
    client_t *c = &list.data[cur];
    XWindowAttributes attr;
    XGetWindowAttributes(display, c->window, &attr);
    c->x = attr.x;
    c->y = attr.y;
    c->w = attr.width;
    c->h = attr.height;
}

static void
win_tile()
{
    if (list.size == 0 || mode == MODE_FLOAT) return;
    Window wn;

    if (mode == MODE_MONOCLE) {
        for (int i = 0; i < list.size; ++i) {
            wn = list.data[i].window;
            list.data[i].fullscreen = false;
            XMoveResizeWindow(display, wn, 0, TOPGAP, screen_w, screen_h-TOPGAP);
        }
        return;
    }

    if (list.size == 1) {
        wn = list.data[cur].window;
        XMoveResizeWindow(display, wn, 0, TOPGAP, screen_w, screen_h-TOPGAP);
        return;
    }

    wn = list.data[0].window;
    XMoveResizeWindow(display, wn, 0, TOPGAP, screen_w / 2, screen_h-TOPGAP);

    int h = (screen_h-TOPGAP) / (list.size-1);

    for (int i = 1; i < list.size; ++i) {
        wn = list.data[i].window;
        list.data[i].fullscreen = false;
        XMoveResizeWindow(display, wn, screen_w / 2, TOPGAP + (i-1)*h, screen_w / 2, h);
    }
}

static void
win_move(int wn, int x, int y)
{
    if (!list.size || wn < 0 || wn >= list.size) return;
    if (mode != MODE_FLOAT || list.data[wn].fullscreen) return;
    client_t *c = &list.data[wn];
    c->x += x;
    c->y += y;
    XMoveWindow(display, c->window, c->x, c->y);
}

static void
win_resize(int wn, int w, int h)
{
    if (!list.size || wn < 0 || wn >= list.size) return;
    if (mode != MODE_FLOAT || list.data[wn].fullscreen) return;
    client_t *c = &list.data[wn];
    c->w += w;
    c->h += h;
    XResizeWindow(display, c->window, c->w, c->h);
}

static void
win_prev()
{
    if (!list.size) return;
    if (cur == 0) cur = list.size-1;
    else cur--;
    XRaiseWindow(display, list.data[cur].window);
    win_focus(cur);
}

static void
win_next()
{
    if (!list.size) return;
    if (cur >= list.size-1) cur = 0;
    else cur++;
    XRaiseWindow(display, list.data[cur].window);
    win_focus(cur);
}

static void
win_swap(int a, int b)
{
    if (list.size < 2 || a == b) return;
    client_t c = list.data[a];
    list.data[a] = list.data[b];
    list.data[b] = c;
    win_focus(b);
    win_tile();
}

static void
win_rotate_next()
{
    if (list.size < 2) return;
    if (cur == list.size-1) {
        win_swap(cur, 0);
        return;
    }
    win_swap(cur, cur+1);
}

static void
win_rotate_prev()
{
    if (list.size < 2) return;
    if (cur == 0) {
        win_swap(cur, list.size-1);
        return;
    }
    win_swap(cur, cur-1);
}

static void
win_focus(size_t l)
{
    cur = l;
    XSetInputFocus(display, list.data[cur].window, RevertToParent, CurrentTime);
    if (mode == MODE_FLOAT && !list.data[cur].fullscreen)
        win_get_size();
}

static void
win_center()
{
    if (!list.size || mode != MODE_FLOAT) return;
    client_t *c = &list.data[cur];
    c->fullscreen = false;

    c->x = (screen_w / 2) - (c->w / 2);
    c->y = (screen_h / 2) - (c->h / 2) + (TOPGAP / 2);

    XMoveResizeWindow(display, c->window, c->x, c->y, c->w, c->h);
}

static void
win_fullscreen()
{
    if (!list.size) return;
    client_t *c = &list.data[cur];

    c->fullscreen = !c->fullscreen;
    if (c->fullscreen) {
        XRaiseWindow(display, c->window);
        XMoveResizeWindow(display, c->window, 0, 0, screen_w, screen_h);
    }
    else {
        XMoveResizeWindow(display, c->window, c->x, c->y, c->w, c->h);
        win_tile();
    }
}

static void
win_kill()
{
    if (list.size) {
        XKillClient(display, list.data[cur].window);
    }
}

static void
win_add(Window w)
{
    client_t c;
    c.window = w;
    c.fullscreen = false;

    LIST_ADD(list, 0, c);
    cur = 0;
}

static void
win_del(Window w)
{
    int c = -1;
    for (size_t i = 0; i < list.size; ++i) {
        if (list.data[i].window == w) {
            c = i;
            break;
        }
    }

    if (!list.size || c < 0) return;
    LIST_POP(list, c);

    if (cur >= list.size) cur = list.size;
}

static void
exec(char **cmd)
{
    if (fork()) return;
    if (display) close(ConnectionNumber(display));

    setsid();
    execvp((char*)cmd[0], (char**)cmd);
}

void
grab_key(Window root, uint32_t mod, KeySym key)
{
    XGrabKey(display, XKeysymToKeycode(display, key), mod,
        root, True, GrabModeAsync, GrabModeAsync);
}

void
key_press(XEvent *ev)
{
    if (ev->xkey.keycode == XKeysymToKeycode(display, XK_Return)) {
        exec(term_cmd);
    }
    else if (ev->xkey.keycode == XKeysymToKeycode(display, XK_d)) {
        exec(menu_cmd);
    }
    else if (ev->xkey.keycode == XKeysymToKeycode(display, XK_q)) {
        win_kill();
    }
    else if (ev->xkey.keycode == XKeysymToKeycode(display, XK_c)) {
        win_center();
    }
    else if (ev->xkey.keycode == XKeysymToKeycode(display, XK_f)) {
        win_fullscreen();
    }
    else if (ev->xkey.keycode == XKeysymToKeycode(display, XK_w)) {
        if (++mode == NUM_MODES) mode = 0;
        win_tile();
    }
    else if (ev->xkey.keycode == XKeysymToKeycode(display, XK_o)) {
        win_rotate_prev();
    }
    else if (ev->xkey.keycode == XKeysymToKeycode(display, XK_p)) {
        win_rotate_next();
    }
    else if (ev->xkey.keycode == XKeysymToKeycode(display, XK_Tab)) {
        if (list.size && list.data[cur].fullscreen) return;
        if (ev->xkey.state & ShiftMask) {
            win_prev();
        }
        else {
            win_next();
        }
    }
    else if (ev->xkey.keycode == XKeysymToKeycode(display, XK_h)) {
        if (ev->xkey.state & ShiftMask) {
            win_resize(cur, -20, 0);
        }
        else {
            win_move(cur, -20, 0);
        }
    }
    else if (ev->xkey.keycode == XKeysymToKeycode(display, XK_l)) {
        if (ev->xkey.state & ShiftMask) {
            win_resize(cur, 20, 0);
        }
        else {
            win_move(cur, 20, 0);
        }
    }
    else if (ev->xkey.keycode == XKeysymToKeycode(display, XK_j)) {
        if (ev->xkey.state & ShiftMask) {
            win_resize(cur, 0, 20);
        }
        else {
            win_move(cur, 0, 20);
        }
    }
    else if (ev->xkey.keycode == XKeysymToKeycode(display, XK_k)) {
        if (ev->xkey.state & ShiftMask) {
            win_resize(cur, 0, -20);
        }
        else {
            win_move(cur, 0, -20);
        }
    }
}

int
main(int argc, char **argv)
{
    XEvent ev;

    if(!(display = XOpenDisplay(0))) {
        return 1;
    }

    signal(SIGCHLD, SIG_IGN);

    int s = DefaultScreen(display);
    root = RootWindow(display, s);
    screen_w = XDisplayWidth(display, s);
    screen_h = XDisplayHeight(display, s);

    XSelectInput(display, root, SubstructureRedirectMask);
    list = (list_client_t) LIST_ALLOC(client_t);

    grab_key(root, MOD, XK_Return);
    grab_key(root, MOD, XK_d);
    grab_key(root, MOD, XK_q);
    grab_key(root, MOD, XK_c);
    grab_key(root, MOD, XK_f);
    grab_key(root, MOD, XK_w);
    grab_key(root, MOD, XK_o);
    grab_key(root, MOD, XK_p);

    grab_key(root, MOD, XK_Tab);
    grab_key(root, MOD|ShiftMask, XK_Tab);

    grab_key(root, MOD, XK_h);
    grab_key(root, MOD, XK_l);
    grab_key(root, MOD, XK_j);
    grab_key(root, MOD, XK_k);

    grab_key(root, MOD|ShiftMask, XK_h);
    grab_key(root, MOD|ShiftMask, XK_l);
    grab_key(root, MOD|ShiftMask, XK_j);
    grab_key(root, MOD|ShiftMask, XK_k);

    XGrabButton(display, 1, MOD, root, True,
        ButtonPressMask|ButtonReleaseMask|PointerMotionMask,
        GrabModeAsync, GrabModeAsync, None, None);
    XGrabButton(display, 3, MOD, root, True,
        ButtonPressMask|ButtonReleaseMask|PointerMotionMask,
        GrabModeAsync, GrabModeAsync, None, None);

    for (;;) {
        XNextEvent(display, &ev);

        if (ev.type == EnterNotify && focus_follows_pointer) {
            for (size_t i = 0; i < list.size; ++i) {
                if (list.data[i].window == ev.xcrossing.window) {
                    win_focus(i);
                    break;
                }
            }
        }
        if (ev.type == MapRequest) {
            Window w = ev.xmaprequest.window;
            XSelectInput(display, w, StructureNotifyMask|EnterWindowMask);
            win_add(w);
            XMapWindow(display, w);
            win_get_size();
            win_center();
            win_focus(0);
            win_tile();
        }
        if (ev.type == DestroyNotify) {
            win_del(ev.xdestroywindow.window);
            if (list.size) {
                win_focus(0);
                win_tile();
            }
        }
        if (ev.type == ConfigureRequest) {
            XConfigureRequestEvent *cr = &ev.xconfigurerequest;
            XWindowChanges ch = {
                .x = cr->x,
                .y = cr->y,
                .width = cr->width,
                .height = cr->height,
                .sibling = cr->above,
                .stack_mode = cr->detail,
            };
            XConfigureWindow(display, cr->window, cr->value_mask, &ch);
            // win_get_size();
        }

        if (ev.type == KeyPress) {
            key_press(&ev);
        }

        if (ev.type == ButtonPress && ev.xbutton.subwindow != None) {
            XRaiseWindow(display, ev.xbutton.subwindow);
            XGetWindowAttributes(display, ev.xbutton.subwindow, &attr);
            start = ev.xbutton;
            for (size_t i = 0; i < list.size; ++i) {
                if (list.data[i].window == ev.xbutton.subwindow) {
                    win_focus(i);
                    break;
                }
            }
        }
        else if (ev.type == MotionNotify && start.subwindow != None) {
            if (mode == MODE_FLOAT) {
                int xdiff = ev.xbutton.x_root - start.x_root;
                int ydiff = ev.xbutton.y_root - start.y_root;
                
                XMoveResizeWindow(display, start.subwindow,
                    attr.x + (start.button==1 ? xdiff : 0),
                    attr.y + (start.button==1 ? ydiff : 0),
                    MAX(100, attr.width + (start.button==3 ? xdiff : 0)),
                    MAX(50, attr.height + (start.button==3 ? ydiff : 0)));
            }
        }
    }

    LIST_FREE(list);
    return 0;
}
