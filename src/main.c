#include <X11/XF86keysym.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/Xutil.h>

#include <inttypes.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include "mlist.h"
#include "mwm.h"
#include "config.h"

Display *display;
Window root;
XButtonEvent mouse;
XWindowAttributes hover_attr;
uint32_t screen_w, screen_h;

desktop_t desktops[NUM_WS];
size_t cur_desktop = 0;

static void (*events[LASTEvent])(XEvent*) = {
    [ButtonPress]      = button_press,
    [ButtonRelease]    = button_release,
    [ConfigureRequest] = configure_request,
    [KeyPress]         = key_press,
    [MapRequest]       = map_request,
    [UnmapNotify]      = unmap_notify,
    [DestroyNotify]    = destroy_notify,
    [MotionNotify]     = motion_notify,
};

#define CUR_WS desktops[cur_desktop]
#define WS_WIN(c) CUR_WS.list.data[(c)]
#define CUR_WIN WS_WIN(CUR_WS.cur)

static void win_tile_master_stack();
static void win_tile_monocle();
static void win_draw_floating();

void
tile_mode(const arg_t arg)
{
    if (CUR_WS.mode == arg.i) return;
    CUR_WS.last_mode = CUR_WS.mode;
    CUR_WS.mode = arg.i;
    win_tile();
}

void
ws_change(const arg_t arg)
{
    if (arg.i >= NUM_WS || arg.i == cur_desktop) return;

    for (int i = 0; i < CUR_WS.list.size; ++i) {
        XUnmapWindow(display, WS_WIN(i).window);
    }

    cur_desktop = arg.i;

    for (int i = 0; i < CUR_WS.list.size; ++i) {
        XMapWindow(display, WS_WIN(i).window);
    }

    if (CUR_WS.list.size)
        win_focus(CUR_WS.cur);
    win_tile();
}

void
win_to_ws(const arg_t arg)
{
    if (arg.i >= NUM_WS || arg.i == cur_desktop) return;

    client_t client = CUR_WIN;

    LIST_ADD(desktops[arg.i].list, 0, client);
    desktops[arg.i].cur = 0;

    XUnmapWindow(display, CUR_WIN.window);
    win_del(CUR_WIN.window);

    if (CUR_WS.list.size) {
        if (CUR_WS.cur >= CUR_WS.list.size)
            CUR_WS.cur = CUR_WS.list.size-1;
        win_focus(CUR_WS.cur);
    }
    win_tile();
}

void
win_get_size()
{
    XWindowAttributes attr;
    XGetWindowAttributes(display, CUR_WIN.window, &attr);
    CUR_WIN.x = attr.x;
    CUR_WIN.y = attr.y;
    CUR_WIN.w = attr.width;
    CUR_WIN.h = attr.height;
}

void
win_tile()
{
    if (CUR_WS.list.size == 0) return;

    if (CUR_WIN.fullscreen) {
        XRaiseWindow(display, CUR_WIN.window);
        XMoveResizeWindow(display, CUR_WIN.window, 0, 0, screen_w, screen_h);
        return;
    }

    int mode = (CUR_WS.mode == MODE_FLOAT)? CUR_WS.last_mode : CUR_WS.mode;

    switch (mode) {
    case MODE_MONOCLE:
        win_tile_monocle();
        break;
    default:
        win_tile_master_stack();
        break;
    }

    win_draw_floating();
}

static void
win_tile_master_stack()
{
    Window wn;
    int master_sz = CUR_WS.master_sz;
    int stack_sz = screen_w - master_sz;
    int master = -1;
    int nstack = 0;

    for (int i = 0; i < CUR_WS.list.size; ++i) {
        if (!WS_WIN(i).floating) {
            if (master < 0)
                master = i;
            else
                nstack++;
        }
    }

    // bruh this feels hacky as fuck

    if (master < 0)
        return;

    if ((CUR_WS.list.size == 1 && !CUR_WIN.floating) || nstack == 0) {
        wn = WS_WIN(master).window;
        XMoveResizeWindow(display, wn,
                GAPSIZE,
                TOPGAP + GAPSIZE,
                screen_w - GAPSIZE*2,
                screen_h - TOPGAP - GAPSIZE*2);

        return;
    }

    wn = WS_WIN(master).window;
    XMoveResizeWindow(display, wn,
           GAPSIZE,
           TOPGAP + GAPSIZE,
           master_sz - GAPSIZE,
           screen_h - TOPGAP - (GAPSIZE*2));

    int h = (screen_h-TOPGAP-GAPSIZE) / (nstack);
    int w = 0;

    for (int i = master+1; i < CUR_WS.list.size; ++i) {
        if (WS_WIN(i).floating) continue;
        wn = WS_WIN(i).window;

        XMoveResizeWindow(display, wn,
                master_sz + GAPSIZE,
                w*h + TOPGAP + GAPSIZE,
                stack_sz - (GAPSIZE*2),
                h - GAPSIZE);
        ++w;
    }
}

static void
win_tile_monocle()
{
    Window wn;
    for (int i = 0; i < CUR_WS.list.size; ++i) {
        if (WS_WIN(i).floating) continue;
        wn = WS_WIN(i).window;
        XMoveResizeWindow(display, wn,
                GAPSIZE,
                TOPGAP + GAPSIZE,
                screen_w - GAPSIZE*2,
                screen_h - TOPGAP - GAPSIZE*2);
    }
}

static void
win_draw_floating()
{
    client_t c;
    for (int i = 0; i < CUR_WS.list.size; ++i) {
        if (!WS_WIN(i).floating) continue;
        c = WS_WIN(i);
        XRaiseWindow(display, c.window);
        XMoveResizeWindow(display, c.window, c.x, c.y, c.w, c.h);
    }
    if (CUR_WIN.floating)
        XRaiseWindow(display, CUR_WIN.window);
}

void
win_prev(const arg_t arg)
{
    (void) arg;
    if (!CUR_WS.list.size || CUR_WIN.fullscreen) return;
    if (CUR_WS.cur == 0) CUR_WS.cur = CUR_WS.list.size-1;
    else CUR_WS.cur--;
    XRaiseWindow(display, CUR_WIN.window);
    win_focus(CUR_WS.cur);
}

void
win_next(const arg_t arg)
{
    (void) arg;
    if (!CUR_WS.list.size || CUR_WIN.fullscreen) return;
    if (CUR_WS.cur >= CUR_WS.list.size-1) CUR_WS.cur = 0;
    else CUR_WS.cur++;
    XRaiseWindow(display, CUR_WIN.window);
    win_focus(CUR_WS.cur);
}

void
win_swap(int a, int b)
{
    if (CUR_WS.list.size < 2 || a == b) return;
    client_t c = WS_WIN(a);
    WS_WIN(a) = WS_WIN(b);
    WS_WIN(b) = c;
    win_focus(b);
    win_tile();
}

void
win_rotate_next(const arg_t arg)
{
    (void) arg;
    if (CUR_WS.list.size < 2 || CUR_WIN.fullscreen) return;
    if (CUR_WS.cur == CUR_WS.list.size-1) {
        win_swap(CUR_WS.cur, 0);
        return;
    }
    win_swap(CUR_WS.cur, CUR_WS.cur+1);
}

void
win_rotate_prev(const arg_t arg)
{
    (void) arg;
    if (CUR_WS.list.size < 2 || CUR_WIN.fullscreen) return;
    if (CUR_WS.cur == 0) {
        win_swap(CUR_WS.cur, CUR_WS.list.size-1);
        return;
    }
    win_swap(CUR_WS.cur, CUR_WS.cur-1);
}

void
win_focus(size_t l)
{
    if (l > CUR_WS.list.size) return;
    if (WS_WIN(l).window == None) {
        win_del(l);
        return;
    }
    CUR_WS.cur = l;
    XSetInputFocus(display, CUR_WIN.window, RevertToParent, CurrentTime);
    XRaiseWindow(display, CUR_WIN.window);
    win_tile();
}

void
win_center(const arg_t arg)
{
    (void) arg;
    if (!CUR_WS.list.size) return;
    CUR_WIN.x = (screen_w / 2) - (CUR_WIN.w / 2);
    CUR_WIN.y = (screen_h / 2) - (CUR_WIN.h / 2) + (TOPGAP / 2);

    if (CUR_WIN.fullscreen || !CUR_WIN.floating) return;

    XMoveResizeWindow(display, CUR_WIN.window,
            CUR_WIN.x, CUR_WIN.y, CUR_WIN.w, CUR_WIN.h);
}

void
win_master(const arg_t arg)
{
    (void) arg;
    if (CUR_WS.list.size < 2 || CUR_WS.cur == 0)
        return;

    win_swap(0, CUR_WS.cur);
    win_focus(0);
}

void
resize_master(const arg_t arg)
{
    if (!CUR_WS.list.size) return;
    CUR_WS.master_sz += arg.i;
    win_tile();
}

void
win_float(const arg_t arg)
{
    (void) arg;
    if (!CUR_WS.list.size) return;
    CUR_WIN.floating = !CUR_WIN.floating;
    win_tile();
}

void
win_fullscreen(const arg_t arg)
{
    (void) arg;
    if (!CUR_WS.list.size) return;

    CUR_WIN.fullscreen = !CUR_WIN.fullscreen;
    if (CUR_WIN.fullscreen) {
        XRaiseWindow(display, CUR_WIN.window);
        XMoveResizeWindow(display, CUR_WIN.window,
                0, 0, screen_w, screen_h);
    }
    else {
        XMoveResizeWindow(display, CUR_WIN.window,
                CUR_WIN.x, CUR_WIN.y, CUR_WIN.w, CUR_WIN.h);
        win_tile();
    }
}

void
win_kill(const arg_t arg)
{
    (void) arg;
    if (CUR_WS.list.size) {
        XKillClient(display, CUR_WIN.window);
    }
}

void
win_add(Window w)
{
    if (CUR_WS.list.size)
        CUR_WIN.fullscreen = false;

    client_t c;
    c.window = w;
    c.fullscreen = false;
    c.floating = (CUR_WS.mode == MODE_FLOAT);

    LIST_ADD(CUR_WS.list, 0, c);
    CUR_WS.cur = 0;
}

void
win_del(Window w)
{
    int c = -1;
    for (size_t i = 0; i < CUR_WS.list.size; ++i) {
        if (WS_WIN(i).window == w) {
            c = i;
            break;
        }
    }

    if (c >= 0)
        LIST_POP(CUR_WS.list, c);

    if (!CUR_WS.list.size)
        CUR_WS.cur = 0;
    else if (CUR_WS.cur >= CUR_WS.list.size)
        CUR_WS.cur = CUR_WS.list.size-1;
}

void
exec(const arg_t arg)
{
    if (fork()) return;
    if (display) close(ConnectionNumber(display));

    setsid();
    execvp((char*)arg.com[0], (char**)arg.com);
}

void
grab_input(Window root)
{
    XUngrabKey(display, AnyKey, AnyModifier, root);
    KeyCode code;

    for (int i = 0; i < LEN(keys); ++i) {
        if (code = XKeysymToKeycode(display, keys[i].keysym)) {
            XGrabKey(display, code, keys[i].mod, root,
                    True, GrabModeAsync, GrabModeAsync);
        }
    }

    XGrabButton(display, 1, MOD, root, True,
        ButtonPressMask|ButtonReleaseMask|PointerMotionMask,
        GrabModeAsync, GrabModeAsync, None, None);
    XGrabButton(display, 3, MOD, root, True,
        ButtonPressMask|ButtonReleaseMask|PointerMotionMask,
        GrabModeAsync, GrabModeAsync, None, None);
}

void
map_request(XEvent *ev)
{
    Window w = ev->xmaprequest.window;
    XSelectInput(display, w, StructureNotifyMask|EnterWindowMask);
    win_add(w);
    XMapWindow(display, w);
    win_get_size();
    win_center((arg_t){0});
    win_focus(0);
    win_tile();
}

static void
win_unmap_destroy_idk(Window wn)
{
    win_del(wn);
    XSetInputFocus(display, root, RevertToParent, CurrentTime);
    if (CUR_WS.list.size) {
        win_focus(CUR_WS.cur);
        win_tile();
    }
}

void
unmap_notify(XEvent *ev)
{
    win_unmap_destroy_idk(ev->xunmap.window);
}

void
destroy_notify(XEvent *ev)
{
    win_unmap_destroy_idk(ev->xdestroywindow.window);
}

void
configure_request(XEvent *ev)
{
    XConfigureRequestEvent *cr = &ev->xconfigurerequest;
    XWindowChanges ch = {
        .x = cr->x,
        .y = cr->y,
        .width = cr->width,
        .height = cr->height,
        .sibling = cr->above,
        .stack_mode = cr->detail,
    };

    XConfigureWindow(display, cr->window, cr->value_mask, &ch);
}

void
button_press(XEvent *ev)
{
    if (ev->xbutton.subwindow == None)
        return;

    XRaiseWindow(display, ev->xbutton.subwindow);
    XGetWindowAttributes(display, ev->xbutton.subwindow, &hover_attr);
    mouse = ev->xbutton;

    for (size_t i = 0; i < CUR_WS.list.size; ++i) {
        if (WS_WIN(i).window == ev->xbutton.subwindow) {
            win_focus(i);
            break;
        }
    }
}

void
button_release(XEvent *ev)
{
    mouse.subwindow = None;
}

void
motion_notify(XEvent *ev)
{
    if (mouse.subwindow == None || !CUR_WIN.floating)
        return;

    int xdiff = ev->xbutton.x_root - mouse.x_root;
    int ydiff = ev->xbutton.y_root - mouse.y_root;
    
    XMoveResizeWindow(display, mouse.subwindow,
        hover_attr.x + (mouse.button==1 ? xdiff : 0),
        hover_attr.y + (mouse.button==1 ? ydiff : 0),
        MAX(100, hover_attr.width + (mouse.button==3 ? xdiff : 0)),
        MAX(50, hover_attr.height + (mouse.button==3 ? ydiff : 0)));
    win_get_size();
}

void
key_press(XEvent *ev)
{
    KeySym keysym = XkbKeycodeToKeysym(display, ev->xkey.keycode, 0, 0);

    for (int i = 0; i < LEN(keys); ++i) {
        if (keys[i].keysym == keysym &&
                mod_clean(keys[i].mod) == mod_clean(ev->xkey.state)) {
            keys[i].function(keys[i].arg);
        }
    }
}

int xerror() { return 0; }

int
main(int argc, char **argv)
{
    XEvent ev;

    if(!(display = XOpenDisplay(0))) {
        return 1;
    }

    signal(SIGCHLD, SIG_IGN);
    XSetErrorHandler(xerror);

    int s = DefaultScreen(display);
    root = RootWindow(display, s);
    screen_w = XDisplayWidth(display, s);
    screen_h = XDisplayHeight(display, s);
    XSelectInput(display, root, SubstructureRedirectMask);

    for (int i = 0; i < NUM_WS; ++i) {
        desktops[i].list = (list_client_t) LIST_ALLOC(client_t);
        desktops[i].mode = desktops[i].last_mode = DEFAULT_MODE;
        desktops[i].cur = 0;
        desktops[i].master_sz = screen_w * MASTER_SIZE;
    }

    XDefineCursor(display, root, XCreateFontCursor(display, 68));
    grab_input(root);

    for (;;) {
        XNextEvent(display, &ev);
        if (events[ev.type])
            events[ev.type](&ev);
    }

    for (int i = 0; i < NUM_WS; ++i) {
        LIST_FREE(desktops[i].list);
    }
    XCloseDisplay(display);
    return 0;
}
