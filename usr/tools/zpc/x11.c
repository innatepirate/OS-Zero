#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <zpc/zpc.h>
#include <zpc/op.h>
#include <zero/cdecl.h>
#include <zero/param.h>
#include "x11.h"

extern struct zpcstkitem *zpcregstk;

#define NHASHITEM 1024
static struct x11win     *winhash[NHASHITEM] ALIGNED(PAGESIZE);
static zpccfunc_t        *buttonopertab[ZPC_NROW][ZPC_NCOLUMN]
= {
    { NULL, NULL, NULL, not64, shr64, inc64 },
    { NULL, NULL, NULL, mod64, shrl64, dec64 },
    { NULL, NULL, NULL, div64, shl64, ror64 },
    { NULL, NULL, NULL, mul64, xor64, rol64 },
    { NULL, NULL, NULL, dec64, or64, NULL },
    { NULL, NULL, NULL, add64, and64, NULL }
};
static zpccfunc_t        *buttonopertabflt[ZPC_NROW][ZPC_NCOLUMN]
= {
    { NULL, NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL, fdiv32, NULL, NULL },
    { NULL, NULL, NULL, mul32, NULL, NULL },
    { NULL, NULL, NULL, fsub32, NULL, NULL },
    { NULL, NULL, NULL, fadd32, NULL, NULL}
};
static zpccfunc_t        *buttonopertabdbl[ZPC_NROW][ZPC_NCOLUMN]
= {
    { NULL, NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL, fdiv64, NULL, NULL },
    { NULL, NULL, NULL, mul64, NULL, NULL },
    { NULL, NULL, NULL, fsub64, NULL, NULL },
    { NULL, NULL, NULL, fadd64, NULL, NULL}
};
static Window             buttonwintab[ZPC_NROW][ZPC_NCOLUMN];
static const char        *buttonstrtab[ZPC_NROW][ZPC_NCOLUMN]
= {
    { "d", "e", "f", "~", ">>>", "++" },
    { "a", "b", "c", "%", ">>", "--" },
    { "7", "8", "9", "/", "<<", "..>" },
    { "4", "5", "6", "*", "^", "<.." },
    { "1", "2", "3", "-", "|", NULL },
    { "0", ".", "SPC", "+", "&", "ENTER" }
};
#define ENTER 0xff
static const uint8_t      nargtab[ZPC_NROW][ZPC_NCOLUMN]
= {
    { 0, 0, 0, 1, 2, 1 },
    { 0, 0, 0, 2, 2, 1 },
    { 0, 0, 0, 2, 2, 2 },
    { 0, 0, 0, 2, 2, 2 },
    { 0, 0, 0, 2, 2, 0 },
    { 0, 0, 0, 2, 2, ENTER }
};
static const uint8_t      isflttab[ZPC_NROW][ZPC_NCOLUMN]
= {
    { 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 1, 0, 0 },
    { 0, 0, 0, 1, 0, 0 },
    { 0, 0, 0, 1, 0, 0 },
    { 0, 0, 0, 1, 0, 0 }
};
#define BUTTONNORMAL  0
#define BUTTONHOVER   1
#define BUTTONCLICKED 2
#define NBUTTONSTATE  3
static Pixmap         buttonpmaps[NBUTTONSTATE];
XFontStruct          *font;
int                   fontw;
int                   fonth;
static struct x11app *app;
static Window         mainwin;
GC                    textgc;

void
x11initgcs(void)
{
    XGCValues gcval;

    gcval.foreground = WhitePixel(app->display, DefaultScreen(app->display));
    gcval.font = font->fid;
    gcval.graphics_exposures = False;

    textgc = XCreateGC(app->display, app->win,
                       GCForeground | GCFont | GCGraphicsExposures,
                       &gcval);
    if (!textgc) {
        fprintf(stderr, "failed to create GC\n");

        exit(1);
    }

    return;
}

XFontStruct *
x11loadfont(const char *fontname)
{
    XFontStruct *newfont;

    newfont = XLoadQueryFont(app->display, fontname);
    if (newfont) {
        fontw = newfont->max_bounds.width;
        fonth = newfont->ascent + newfont->descent;
    }

    return newfont;
}

void
x11addwin(struct x11win *win)
{
    long           key = win->id & (NHASHITEM - 1);

    win->next = winhash[key];
    winhash[key] = win;
}

struct x11win *
x11findwin(Window id)
{
    struct x11win *hashwin = NULL;
    long           key = id & (NHASHITEM - 1);

    hashwin = winhash[key];
    while ((hashwin) && hashwin->id != id) {
        hashwin = hashwin->next;
    }

    return hashwin;
}

void
x11initpmaps(void)
{
    buttonpmaps[BUTTONNORMAL] = imlib2loadimage("mosaic.png",
                                                ZPC_BUTTON_WIDTH,
                                                ZPC_BUTTON_HEIGHT);
    buttonpmaps[BUTTONHOVER] = imlib2loadimage("gaussian.png",
                                               ZPC_BUTTON_WIDTH,
                                               ZPC_BUTTON_HEIGHT);
#if 0
    buttonpmaps[BUTTONCLICKED] = imlib2loadimage("button_clicked.png",
                                                 ZPC_BUTTON_WIDTH,
                                                 ZPC_BUTTON_HEIGHT);
#endif
    fprintf(stderr, "PIXMAPS: %x, %x, %x\n",
            (int)buttonpmaps[BUTTONNORMAL],
            (int)buttonpmaps[BUTTONHOVER],
            (int)buttonpmaps[BUTTONCLICKED]);

    return;
}

struct x11app *
x11initapp(char *displayname)
{
    struct x11app *app;
    Display       *disp;

    app = calloc(1, sizeof(struct x11app));
    XInitThreads();
    disp = XOpenDisplay(displayname);
    if (!disp) {
        fprintf(stderr, "failed to open display\n");

        exit(1);
    }
    app->display = disp;
    app->screen = DefaultScreen(disp);
    app->depth = DefaultDepth(disp, app->screen);
    app->visual = DefaultVisual(disp, app->screen);
    app->colormap = DefaultColormap(disp, app->screen);

    return app;
}

Window
x11initwin(struct x11app *app, Window parent, int x, int y, int w, int h)
{
    XSetWindowAttributes atr;
    Window               win;

    if (!parent) {
        parent = RootWindow(app->display, app->screen);
    }
    memset(&atr, 0, sizeof(atr));
    atr.background_pixel = BlackPixel(app->display, app->screen);
    win = XCreateWindow(app->display,
                        parent,
                        x,
                        y,
                        w,
                        h,
                        0,
                        app->depth,
                        InputOutput,
                        app->visual,
                        CWBackPixel,
                        &atr);

    return win;
}

void
buttonexpose(void *arg, XEvent *event)
{
    struct x11win *win = arg;
    size_t         len;

    if (!event->xexpose.count) {
        if (buttonpmaps[BUTTONNORMAL]) {
            XSetWindowBackgroundPixmap(app->display, win->id,
                                       buttonpmaps[BUTTONNORMAL]);
        }
        XClearWindow(app->display, win->id);
        XSync(app->display, False);
    }
    XClearWindow(app->display, win->id);
    if (win->str) {
        len = strlen(win->str);
        if (len) {
            XDrawString(app->display, win->id, textgc,
                        (ZPC_BUTTON_WIDTH - len * fontw) >> 1,
                        (ZPC_BUTTON_HEIGHT + fonth) >> 1,
                        win->str, len);
        }
    }
    XSync(app->display, False);

    return;
}

#if 0
void
buttonenter(void *arg, XEvent *event)
{
    struct x11win *win = arg;

    if (!event->xexpose.count) {
        if (buttonpmaps[BUTTONHOVER]) {
            XSetWindowBackgroundPixmap(app->display, win->id,
                                       buttonpmaps[BUTTONHOVER]);
        }
        XClearWindow(app->display, win->id);
        XSync(app->display, False);
    }

    return;
}

void
buttonleave(void *arg, XEvent *event)
{
    struct x11win *win = arg;

    if (!event->xexpose.count) {
        XSetWindowBackgroundPixmap(app->display, win->id,
                                   buttonpmaps[BUTTONNORMAL]);
        XClearWindow(app->display, win->id);
        XSync(app->display, False);
    }

    return;
}
#endif

void
buttonpress(void *arg, XEvent *event)
{
    struct x11win *win = arg;
    int            evbut = toevbutton(event->xbutton.button);
    copfunc_t     *func;
    uint64_t      *usrc = NULL;
    uint64_t      *udest = NULL;
    uint64_t       ures64;
    int64_t       *src = NULL;
    int64_t       *dest = NULL;
    int64_t        res64;
    float         *fsrc = NULL;
    float         *fdest = NULL;
    float         *fres;
    double        *dsrc = NULL;
    double        *ddest = NULL;
    double        *dres;
    int            type = 0;
    int            size = 0;

    if (!win->narg) {
        /* queue and display digit here */
    } else if (win->narg == 1) {
        if (zpcregstk) {
            if (zpcregstk->type == ZPCUINT) {
                type = ZPCUINT;
                usrc = &zpcregstk->data.u64;
            } else if (zpcregstk->type == ZPCINT) {
                type = ZPCINT;
                src = &zpcregstk->data.i64;
            } else if (zpcregstk->type == ZPCFLT) {
                type = ZPCFLT;
                if (zpcregstk->size == 4) {
                    size = 4;
                    fsrc = &zpcregstk->data.f32;
                } else {
                    size = 8;
                    dsrc = &zpcregstk->data.f64;
                }
            } else {
                fprintf(stderr, "EEEE\n");
                
                return;
            }
        } else {
            fprintf(stderr, "EEEE\n");
            
            return;
        }
    } else if ((zpcregstk) && zpcregstk->next) {
        if (type == ZPCUINT && zpcregstk->next->type == ZPCUINT) {
            udest = &zpcregstk->next->data.u64;
        } else if (type == ZPCINT && zpcregstk->next->type == ZPCINT) {
            dest = &zpcregstk->next->data.i64;
        } else if (type == ZPCFLT && zpcregstk->next->type == ZPCFLT) {
            if (size == 4 && zpcregstk->next->size == 4) {
                fdest = &zpcregstk->next->data.f32;
            } else if (size == 8) {
                ddest = &zpcregstk->next->data.f64;
            }
        } else {
            fprintf(stderr, "EEEE\n");
            
            return;
        }
    } else {
        fprintf(stderr, "EEEE\n");
        
        return;
    }
#if 0
    XSetWindowBackgroundPixmap(app->display, win->id,
                               buttonpmaps[BUTTONCLICKED]);
    XClearWindow(app->display, win->id);
    XSync(app->display, False);
#endif
    if (evbut < NBUTTON) {
        if (type == ZPCINT || type == ZPCUINT) {
            func = win->clickfunc[evbut];
            if (func) {
                if (type == ZPCINT) {
                    func(src, dest, &res64);
                } else {
                    func(usrc, udest, &ures64);
                }
            }
        } else if (type == ZPCFLT) {
            if (size == 4) {
                func = win->clickfuncflt[evbut];
                if (func) {
                    func(fsrc, fdest, &fres);
                }
            } else {
                func = win->clickfuncdbl[evbut];
                if (func) {
                    func(dsrc, ddest, &dres);
                }
            }
        }
    }
}

#if 0
void
buttonrelease(void *arg, XEvent *event)
{
    struct x11win *win = arg;
    int            evbut = toevbutton(event->xbutton.button);
    copfunc_t     *func;
    int64_t       *src = &zpcregstktab[1]->data.i64;
    int64_t       *dest = &zpcregstktab[0]->data.i64;

    XSetWindowBackgroundPixmap(app->display, win->id,
                               buttonpmaps[BUTTONNORMAL]);
    XClearWindow(app->display, win->id);
    XSync(app->display, False);
}
#endif

void
x11init(void)
{
    Window         win;
    int            row;
    int            col;
    int            i;
    struct x11win *wininfo;

    app = x11initapp(NULL);
    mainwin = x11initwin(app, 0, 0, 0, ZPC_WINDOW_WIDTH, ZPC_WINDOW_HEIGHT);
    app->win = mainwin;
    imlib2init(app);
//    x11initpmaps();
    font = x11loadfont("fixed");
    x11initgcs();
    if (!font) {
        fprintf(stderr, "failed to load font fixed\n");

        exit(1);
    } else {
        fprintf(stderr, "FONT: fixed (%dx%d)\n", fontw, fonth);
    }
    for (row = 0 ; row < ZPC_NROW ; row++) {
        for (col = 0 ; col < ZPC_NCOLUMN ; col++) {
            win = x11initwin(app,
                             mainwin,
                             col * ZPC_BUTTON_WIDTH,
                             row * ZPC_BUTTON_HEIGHT,
                             ZPC_BUTTON_WIDTH,
                             ZPC_BUTTON_HEIGHT);
            if (win) {
                wininfo = calloc(1, sizeof(struct x11win));
                wininfo->id = win;
                wininfo->str = buttonstrtab[row][col];
                wininfo->narg = nargtab[row][col];
                wininfo->row = row;
                wininfo->col = col;
//                wininfo->evfunc[EnterNotify] = buttonenter;
//                wininfo->evfunc[LeaveNotify] = buttonleave;
                wininfo->evfunc[ButtonPress] = buttonpress;
                wininfo->evfunc[Expose] = buttonexpose;
                buttonwintab[row][col] = win;
                wininfo->clickfunc[0] = buttonopertab[row][col];
                wininfo->clickfuncflt[0] = buttonopertabflt[row][col];
                wininfo->clickfuncdbl[0] = buttonopertabdbl[row][col];
                x11addwin(wininfo);
                XSelectInput(app->display, win,
                             ExposureMask | ButtonPressMask);
                XMapRaised(app->display, win);
            } else {
                fprintf(stderr, "failed to create window\n");

                exit(1);
            }
        }
    }
    XMapSubwindows(app->display, mainwin);
    XMapRaised(app->display, mainwin);
}

void
x11nextevent(void)
{
    XEvent         event;
    struct x11win *win;
    x11evfunc_t   *func;

    XNextEvent(app->display, &event);
    win = x11findwin(event.xany.window);
    if (win) {
        func = win->evfunc[event.type];
        if (func) {
            func(win, &event);
        }
    }
}
