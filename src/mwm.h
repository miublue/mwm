#ifndef MWM_H
#define MWM_H

#include <stdbool.h>
#include <inttypes.h>
#include <X11/Xlib.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define LEN(x) (sizeof(x) / sizeof((x)[0]))

// taken from SOWM, which took from DWM. Long live FOSS!
#define mod_clean(mask) (mask & \
        (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))

typedef struct arg_t {
    const char** com;
    const int i;
} arg_t;

struct key_t {
    uint32_t mod;
    KeySym keysym;
    void (*function)(const arg_t arg);
    const arg_t arg;
};

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

void exec(const arg_t arg);
void tile_mode(const arg_t arg);
void ws_change(const arg_t arg);
void win_to_ws(const arg_t arg);
void win_master(const arg_t arg);
void win_center(const arg_t arg);
void win_prev(const arg_t arg);
void win_next(const arg_t arg);
void win_kill(const arg_t arg);
void win_fullscreen(const arg_t arg);
void win_rotate_next(const arg_t arg);
void win_rotate_prev(const arg_t arg);
void win_swap(int a, int b);
void win_move(int wn, int x, int y);
void win_resize(int wn, int w, int h);

void win_focus(size_t c);
void win_tile();
void win_get_size();
void win_add(Window w);
void win_del(Window w);

void button_press(XEvent *ev);
void button_release(XEvent *ev);
void configure_request(XEvent *ev);
void key_press(XEvent *ev);
void map_request(XEvent *ev);
void destroy_notify(XEvent *ev);
void unmap_notify(XEvent *ev);
void motion_notify(XEvent *ev);
void grab_input(Window root);

#endif
