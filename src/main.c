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

#include "mwm.h"

// TODO: move stuffs to a config.h
#define MOD Mod4Mask
#define TOPGAP 18
#define GAPSIZE 6
#define DEFAULT_MODE MODE_TILE
#define NUM_WS 10

const bool focus_follows_pointer = false;

const char *term_cmd[]  = { "msterm",    NULL };
const char *menu_cmd[]  = { "dmenu_run", NULL };

Display *display;
Window root;
XButtonEvent mouse;
XWindowAttributes hover_attr;
uint32_t screen_w, screen_h;

desktop_t desktops[NUM_WS];
size_t cur_desktop = 0;

#define CUR_WS desktops[cur_desktop]
#define WS_WIN(c) CUR_WS.list.data[(c)]
#define CUR_WIN WS_WIN(CUR_WS.cur)

void
ws_change(size_t c)
{
    if (c >= NUM_WS || c == cur_desktop) return;

    for (int i = 0; i < CUR_WS.list.size; ++i) {
        XUnmapWindow(display, WS_WIN(i).window);
    }

    cur_desktop = c;

    for (int i = 0; i < CUR_WS.list.size; ++i) {
        XMapWindow(display, WS_WIN(i).window);
    }

    if (CUR_WS.list.size)
        win_focus(CUR_WS.cur);
    win_tile();
}

void
win_to_ws(size_t c)
{
    if (c >= NUM_WS || c == cur_desktop) return;

    client_t client = CUR_WIN;

    LIST_ADD(desktops[c].list, 0, client);
    desktops[c].cur = 0;

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
    if (CUR_WS.list.size == 0 || CUR_WS.mode == MODE_FLOAT) return;
    Window wn;

    if (CUR_WS.mode == MODE_MONOCLE) {
        for (int i = 0; i < CUR_WS.list.size; ++i) {
            wn = WS_WIN(i).window;
            WS_WIN(i).fullscreen = false;
            XMoveResizeWindow(display, wn,
                    GAPSIZE,
                    TOPGAP + GAPSIZE,
                    screen_w - GAPSIZE*2,
                    screen_h - TOPGAP - GAPSIZE*2);
        }
        return;
    }

    if (CUR_WS.list.size == 1) {
        wn = CUR_WIN.window;
        XMoveResizeWindow(display, wn,
                GAPSIZE,
                TOPGAP + GAPSIZE,
                screen_w - GAPSIZE*2,
                screen_h - TOPGAP - GAPSIZE*2);
        return;
    }

    int master_sz = screen_w / 2;
    wn = WS_WIN(0).window;
    XMoveResizeWindow(display, wn,
            GAPSIZE,
            TOPGAP + GAPSIZE,
            master_sz - GAPSIZE,
            screen_h - TOPGAP - (GAPSIZE*2));

    int h = (screen_h-TOPGAP-GAPSIZE) / (CUR_WS.list.size-1);

    for (int i = 1; i < CUR_WS.list.size; ++i) {
        wn = WS_WIN(i).window;
        WS_WIN(i).fullscreen = false;
        XMoveResizeWindow(display, wn,
                master_sz + GAPSIZE,
                (i-1)*h + TOPGAP + GAPSIZE,
                master_sz - (GAPSIZE*2),
                h - GAPSIZE);
    }
}

void
win_move(int wn, int x, int y)
{
    if (!CUR_WS.list.size || wn < 0 || wn >= CUR_WS.list.size) return;
    if (CUR_WS.mode != MODE_FLOAT || WS_WIN(wn).fullscreen) return;
    WS_WIN(wn).x += x;
    WS_WIN(wn).y += y;
    XMoveWindow(display, WS_WIN(wn).window, WS_WIN(wn).x, WS_WIN(wn).y);
}

void
win_resize(int wn, int w, int h)
{
    if (!CUR_WS.list.size || wn < 0 || wn >= CUR_WS.list.size) return;
    if (CUR_WS.mode != MODE_FLOAT || WS_WIN(wn).fullscreen) return;
    WS_WIN(wn).w += w;
    WS_WIN(wn).h += h;
    XResizeWindow(display, WS_WIN(wn).window, WS_WIN(wn).w, WS_WIN(wn).h);
}

void
win_prev()
{
    if (!CUR_WS.list.size) return;
    if (CUR_WS.cur == 0) CUR_WS.cur = CUR_WS.list.size-1;
    else CUR_WS.cur--;
    XRaiseWindow(display, CUR_WIN.window);
    win_focus(CUR_WS.cur);
}

void
win_next()
{
    if (!CUR_WS.list.size) return;
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
win_rotate_next()
{
    if (CUR_WS.list.size < 2) return;
    if (CUR_WS.cur == CUR_WS.list.size-1) {
        win_swap(CUR_WS.cur, 0);
        return;
    }
    win_swap(CUR_WS.cur, CUR_WS.cur+1);
}

void
win_rotate_prev()
{
    if (CUR_WS.list.size < 2) return;
    if (CUR_WS.cur == 0) {
        win_swap(CUR_WS.cur, CUR_WS.list.size-1);
        return;
    }
    win_swap(CUR_WS.cur, CUR_WS.cur-1);
}

void
win_focus(size_t l)
{
    CUR_WS.cur = l;
    XSetInputFocus(display, CUR_WIN.window, RevertToParent, CurrentTime);
    if (CUR_WS.mode == MODE_FLOAT && !CUR_WIN.fullscreen)
        win_get_size();
}

void
win_center()
{
    if (!CUR_WS.list.size || CUR_WS.mode != MODE_FLOAT) return;
    CUR_WIN.fullscreen = false;

    CUR_WIN.x = (screen_w / 2) - (CUR_WIN.w / 2);
    CUR_WIN.y = (screen_h / 2) - (CUR_WIN.h / 2) + (TOPGAP / 2);

    XMoveResizeWindow(display, CUR_WIN.window,
            CUR_WIN.x, CUR_WIN.y, CUR_WIN.w, CUR_WIN.h);
}

void
win_fullscreen()
{
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
win_kill()
{
    if (CUR_WS.list.size) {
        XKillClient(display, CUR_WIN.window);
    }
}

void
win_add(Window w)
{
    client_t c;
    c.window = w;
    c.fullscreen = false;

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

    if (!CUR_WS.list.size || c < 0) return;
    LIST_POP(CUR_WS.list, c);

    if (CUR_WS.cur >= CUR_WS.list.size)
        CUR_WS.cur = CUR_WS.list.size-1;
}

void
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

// TODO: PLEASE PLEASE PLEASE PLEASE PLEASE PLEASE
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
        if (++CUR_WS.mode == NUM_MODES)
            CUR_WS.mode = 0;
        win_tile();
    }
    else if (ev->xkey.keycode == XKeysymToKeycode(display, XK_o)) {
        win_rotate_prev();
    }
    else if (ev->xkey.keycode == XKeysymToKeycode(display, XK_p)) {
        win_rotate_next();
    }
    else if (ev->xkey.keycode == XKeysymToKeycode(display, XK_Tab)) {
        if (CUR_WS.list.size && CUR_WIN.fullscreen) return;
        if (ev->xkey.state & ShiftMask) {
            win_prev();
        }
        else {
            win_next();
        }
    }
    else if (ev->xkey.keycode == XKeysymToKeycode(display, XK_h)) {
        if (ev->xkey.state & ShiftMask) {
            win_resize(CUR_WS.cur, -20, 0);
        }
        else {
            win_move(CUR_WS.cur, -20, 0);
        }
    }
    else if (ev->xkey.keycode == XKeysymToKeycode(display, XK_l)) {
        if (ev->xkey.state & ShiftMask) {
            win_resize(CUR_WS.cur, 20, 0);
        }
        else {
            win_move(CUR_WS.cur, 20, 0);
        }
    }
    else if (ev->xkey.keycode == XKeysymToKeycode(display, XK_j)) {
        if (ev->xkey.state & ShiftMask) {
            win_resize(CUR_WS.cur, 0, 20);
        }
        else {
            win_move(CUR_WS.cur, 0, 20);
        }
    }
    else if (ev->xkey.keycode == XKeysymToKeycode(display, XK_k)) {
        if (ev->xkey.state & ShiftMask) {
            win_resize(CUR_WS.cur, 0, -20);
        }
        else {
            win_move(CUR_WS.cur, 0, -20);
        }
    }
    else if (ev->xkey.keycode == XKeysymToKeycode(display, XK_1)) {
        if (ev->xkey.state & ShiftMask) {
            win_to_ws(0);
        }
        else {
            ws_change(0);
        }
    }
    else if (ev->xkey.keycode == XKeysymToKeycode(display, XK_2)) {
        if (ev->xkey.state & ShiftMask) {
            win_to_ws(1);
        }
        else {
            ws_change(1);
        }
    }
    else if (ev->xkey.keycode == XKeysymToKeycode(display, XK_3)) {
        if (ev->xkey.state & ShiftMask) {
            win_to_ws(2);
        }
        else {
            ws_change(2);
        }
    }
    else if (ev->xkey.keycode == XKeysymToKeycode(display, XK_4)) {
        if (ev->xkey.state & ShiftMask) {
            win_to_ws(3);
        }
        else {
            ws_change(3);
        }
    }
    else if (ev->xkey.keycode == XKeysymToKeycode(display, XK_5)) {
        if (ev->xkey.state & ShiftMask) {
            win_to_ws(4);
        }
        else {
            ws_change(4);
        }
    }
    else if (ev->xkey.keycode == XKeysymToKeycode(display, XK_6)) {
        if (ev->xkey.state & ShiftMask) {
            win_to_ws(5);
        }
        else {
            ws_change(5);
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

    for (int i = 0; i < NUM_WS; ++i) {
        desktops[i].list = (list_client_t) LIST_ALLOC(client_t);
        desktops[i].mode = DEFAULT_MODE;
        desktops[i].cur = 0;
    }

    // TODO: for the love of god fix this garbage
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

    grab_key(root, MOD, XK_1);
    grab_key(root, MOD, XK_2);
    grab_key(root, MOD, XK_3);
    grab_key(root, MOD, XK_4);
    grab_key(root, MOD, XK_5);
    grab_key(root, MOD, XK_6);

    grab_key(root, MOD|ShiftMask, XK_1);
    grab_key(root, MOD|ShiftMask, XK_2);
    grab_key(root, MOD|ShiftMask, XK_3);
    grab_key(root, MOD|ShiftMask, XK_4);
    grab_key(root, MOD|ShiftMask, XK_5);
    grab_key(root, MOD|ShiftMask, XK_6);

    XGrabButton(display, 1, MOD, root, True,
        ButtonPressMask|ButtonReleaseMask|PointerMotionMask,
        GrabModeAsync, GrabModeAsync, None, None);
    XGrabButton(display, 3, MOD, root, True,
        ButtonPressMask|ButtonReleaseMask|PointerMotionMask,
        GrabModeAsync, GrabModeAsync, None, None);

    for (;;) {
        XNextEvent(display, &ev);

        if (ev.type == EnterNotify && focus_follows_pointer) {
            for (size_t i = 0; i < CUR_WS.list.size; ++i) {
                if (WS_WIN(i).window == ev.xcrossing.window) {
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
            if (CUR_WS.list.size) {
                win_focus(0);
                win_tile();
            }
        }
        if (ev.type == ConfigureRequest) {
            XConfigureRequestEvent *cr = &ev.xconfigurerequest;
            if (cr->window != None) {
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
            // win_get_size();
        }

        if (ev.type == KeyPress) {
            key_press(&ev);
        }

        if (ev.type == ButtonPress && ev.xbutton.subwindow != None) {
            XRaiseWindow(display, ev.xbutton.subwindow);
            XGetWindowAttributes(display, ev.xbutton.subwindow, &hover_attr);
            mouse = ev.xbutton;
            for (size_t i = 0; i < CUR_WS.list.size; ++i) {
                if (WS_WIN(i).window == ev.xbutton.subwindow) {
                    win_focus(i);
                    break;
                }
            }
        }
        else if (ev.type == MotionNotify && mouse.subwindow != None) {
            if (CUR_WS.mode == MODE_FLOAT) {
                int xdiff = ev.xbutton.x_root - mouse.x_root;
                int ydiff = ev.xbutton.y_root - mouse.y_root;
                
                XMoveResizeWindow(display, mouse.subwindow,
                    hover_attr.x + (mouse.button==1 ? xdiff : 0),
                    hover_attr.y + (mouse.button==1 ? ydiff : 0),
                    MAX(100, hover_attr.width + (mouse.button==3 ? xdiff : 0)),
                    MAX(50, hover_attr.height + (mouse.button==3 ? ydiff : 0)));
            }
        }
    }

    for (int i = 0; i < NUM_WS; ++i) {
        LIST_FREE(desktops[i].list);
    }
    return 0;
}
