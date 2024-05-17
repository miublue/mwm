#ifndef MWM_H
#define MWM_H

#include <stdbool.h>
#include <inttypes.h>
#include <X11/Xlib.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))

typedef struct client_t {
    uint32_t x, y, w, h;
    bool fullscreen;
    Window window;
} client_t;

LIST_DEFINE(client_t, list_client_t);

enum {
    MODE_TILE,
    MODE_MONOCLE,
    MODE_FLOAT,
    NUM_MODES,
};

typedef struct desktop_t {
    list_client_t list;
    int cur;
    int mode;
} desktop_t;

void ws_change(size_t c);
void win_to_ws(size_t c);
void win_focus(size_t c);
void win_center();
void win_tile();
void win_prev();
void win_next();
void win_kill();
void win_get_size();
void win_fullscreen();
void win_move(int wn, int x, int y);
void win_resize(int wn, int w, int h);
void win_swap(int a, int b);
void win_rotate_next();
void win_rotate_prev();
void win_add(Window w);
void win_del(Window w);
void exec(char **cmd);

void grab_key(Window root, uint32_t mod, KeySym key);
void key_press(XEvent *ev);

#endif
