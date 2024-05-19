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

#include "config.h"

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
tile_mode(const arg_t arg)
{
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
win_prev(const arg_t arg)
{
    (void) arg;
    if (!CUR_WS.list.size) return;
    if (CUR_WS.cur == 0) CUR_WS.cur = CUR_WS.list.size-1;
    else CUR_WS.cur--;
    XRaiseWindow(display, CUR_WIN.window);
    win_focus(CUR_WS.cur);
}

void
win_next(const arg_t arg)
{
    (void) arg;
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
win_rotate_next(const arg_t arg)
{
    (void) arg;
    if (CUR_WS.list.size < 2) return;
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
win_center(const arg_t arg)
{
    (void) arg;
    if (!CUR_WS.list.size || CUR_WS.mode != MODE_FLOAT) return;
    CUR_WIN.fullscreen = false;

    CUR_WIN.x = (screen_w / 2) - (CUR_WIN.w / 2);
    CUR_WIN.y = (screen_h / 2) - (CUR_WIN.h / 2) + (TOPGAP / 2);

    XMoveResizeWindow(display, CUR_WIN.window,
            CUR_WIN.x, CUR_WIN.y, CUR_WIN.w, CUR_WIN.h);
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

// TODO: PLEASE PLEASE PLEASE PLEASE PLEASE PLEASE
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

    grab_input(root);

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
            win_center((arg_t){0});
            win_focus(0);
            win_tile();
        }
        if (ev.type == DestroyNotify) {
            win_del(ev.xdestroywindow.window);
            CUR_WS.cur = 0;
            if (CUR_WS.list.size) {
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
