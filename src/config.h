#ifndef CONFIG_H
#define CONFIG_H

#include <X11/Xlib.h>
#include "mwm.h"

#define MOD Mod4Mask
#define TOPGAP 18
#define GAPSIZE 6
#define DEFAULT_MODE MODE_TILE
#define NUM_WS 10
#define MASTER_SIZE 0.5

const char *term[] = { "mterm",     NULL };
const char *menu[] = { "dmenu_run", NULL };

#define DESKTOPCHANGE(key, ws) \
    {MOD, key, ws_change, {.i = ws}}, \
    {MOD|ShiftMask, key, win_to_ws, {.i = ws}}

static struct key_t keys[] = {
    {MOD,           XK_q,       win_kill,        {0}},
    {MOD,           XK_f,       win_fullscreen,  {0}},
    {MOD,           XK_c,       win_center,      {0}},

    {MOD,           XK_space,   win_master,      {0}},
    {MOD|ShiftMask, XK_space,   win_float,       {0}},

    {MOD,           XK_Tab,     win_next,        {0}},
    {MOD|ShiftMask, XK_Tab,     win_prev,        {0}},

    {MOD,           XK_j,       win_next,        {0}},
    {MOD,           XK_k,       win_prev,        {0}},
    {MOD|ShiftMask, XK_j,       win_rotate_next, {0}},
    {MOD|ShiftMask, XK_k,       win_rotate_prev, {0}},

    {MOD,           XK_h,       resize_master,   {.i = -20}},
    {MOD,           XK_l,       resize_master,   {.i =  20}},

    {MOD|ShiftMask, XK_t,       tile_mode,       {.i = MODE_TILE}},
    {MOD|ShiftMask, XK_m,       tile_mode,       {.i = MODE_MONOCLE}},
    {MOD|ShiftMask, XK_f,       tile_mode,       {.i = MODE_FLOAT}},

    {MOD,           XK_d,       exec,            {.com = menu}},
    {MOD,           XK_Return,  exec,            {.com = term}},

    DESKTOPCHANGE(XK_1, 0),
    DESKTOPCHANGE(XK_2, 1),
    DESKTOPCHANGE(XK_3, 2),
    DESKTOPCHANGE(XK_4, 3),
    DESKTOPCHANGE(XK_5, 4),
    DESKTOPCHANGE(XK_6, 5),
    DESKTOPCHANGE(XK_7, 6),
    DESKTOPCHANGE(XK_8, 7),
    DESKTOPCHANGE(XK_9, 8),
    DESKTOPCHANGE(XK_0, 9),
};

#endif
