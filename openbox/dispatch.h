#ifndef __dispatch_h
#define __dispatch_h

#include "misc.h"

#include <X11/Xlib.h>

struct _ObClient;

void dispatch_startup();
void dispatch_shutdown();

typedef enum {
    Event_X_EnterNotify   = 1 << 0, /* pointer entered a window */
    Event_X_LeaveNotify   = 1 << 1, /* pointer left a window */
    Event_X_KeyPress      = 1 << 2, /* key pressed */
    Event_X_KeyRelease    = 1 << 3, /* key released */
    Event_X_ButtonPress   = 1 << 4, /* mouse button pressed */
    Event_X_ButtonRelease = 1 << 5, /* mouse button released */
    Event_X_MotionNotify  = 1 << 6, /* mouse motion */
    Event_X_Bell          = 1 << 7, /* an XKB bell event */

    Event_Client_New      = 1 << 8, /* new window, before mapping */
    Event_Client_Mapped   = 1 << 9, /* new window, after mapping
                                       or uniconified */
    Event_Client_Destroy  = 1 << 10, /* unmanaged */
    Event_Client_Unmapped = 1 << 11, /* unmanaged, after unmapping
                                        or iconified */
    Event_Client_Focus    = 1 << 12, /* focused */
    Event_Client_Unfocus  = 1 << 13, /* unfocused */
    Event_Client_Urgent   = 1 << 14, /* entered/left urgent state */
    Event_Client_Desktop  = 1 << 15, /* moved to a new desktop */
    Event_Client_Moving   = 1 << 16, /* being interactively moved */
    Event_Client_Resizing = 1 << 17, /* being interactively resized */

    Event_Ob_Desktop      = 1 << 18, /* changed desktops */
    Event_Ob_NumDesktops  = 1 << 19, /* changed the number of desktops */
    Event_Ob_ShowDesktop  = 1 << 20, /* entered/left show-the-desktop mode */

    Event_Signal          = 1 << 21, /* a signal from the OS */

    EVENT_RANGE           = 1 << 22
} EventType;

typedef struct {
    XEvent *e;
    struct _ObClient *client;
} EventData_X;

typedef struct {
    struct _ObClient *client;
    int num[3];
    /* Event_Client_Desktop: num[0] = new number, num[1] = old number
       Event_Client_Urgent: num[0] = urgent state
       Event_Client_Moving: num[0] = dest x coord, num[1] = dest y coord --
                            change these in the handler to adjust where the
                            window will be placed
       Event_Client_Resizing: num[0] = dest width, num[1] = dest height --
                              change these in the handler to adjust where the
                              window will be placed
                              num[2] = the anchored corner
     */
} EventData_Client;

typedef struct {
    int num[2];
    /* Event_Ob_Desktop: num[0] = new number, num[1] = old number
       Event_Ob_NumDesktops: num[0] = new number, num[1] = old number
       Event_Ob_ShowDesktop: num[0] = new show-desktop mode
     */
} EventData_Ob;

typedef struct {
    int signal;
} EventData_Signal;

typedef struct {
    EventData_X x;      /* for Event_X_* event types */
    EventData_Client c; /* for Event_ObClient_* event types */
    EventData_Ob o;     /* for Event_Ob_* event types */
    EventData_Signal s; /* for Event_Signal */
} EventData;

typedef struct {
    EventType type;
    EventData data;
} ObEvent;

typedef void (*EventHandler)(const ObEvent *e, void *data);

typedef unsigned int EventMask;

void dispatch_register(EventMask mask, EventHandler h, void *data);

void dispatch_x(XEvent *e, struct _ObClient *c);
void dispatch_client(EventType e, struct _ObClient *c, int num0, int num1);
void dispatch_ob(EventType e, int num0, int num1);
void dispatch_signal(int signal);
/* *x and *y should be set with the destination of the window, they may be
   changed by the event handlers */
void dispatch_move(struct _ObClient *c, int *x, int *y);
/* *w and *h should be set with the destination of the window, they may be
   changed by the event handlers */
void dispatch_resize(struct _ObClient *c, int *w, int *h, ObCorner corner);

#endif
