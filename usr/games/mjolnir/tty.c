#include <stdlib.h>
#include <mjolnir/conf.h>

#if (MJOL_TTY)

#include <mjolnir/mjol.h>
#include <mjolnir/scr.h>
#include <mjolnir/tty.h>

int
mjolgetchtty(void)
{
    int retval = EOF;

#if (MJOL_VT)
    printf("READ\n");
    retval = getchar();
    if (retval == EOF) {
        fprintf(stderr, "EOF read\n");

        exit(0);
    }
#elif (MJOL_CURSES)
    retval = getch();
#endif

    return retval;
}

void
mjolmovetotty(int x, int y)
{
#if (MJOL_VT)
    char esc[] = "\033[";
#endif

#if (MJOL_VT)
    printf("%s", esc);
    printf("%d:%dH", y, x);
#elif (MJOL_CURSES)
    /* TODO */
#endif
}

void
mjoldrawchrtty(struct mjolgame *game, struct mjolchr *chr)
{
    struct mjolscr *scr = game->scr;
    long            lvl = game->lvl;
    int             x = chr->data.x;
    int             y = chr->data.y;

    if (x != scr->x || y != scr->y) {
        scr->moveto(x, y);
    }
#if (MJOL_VT)
    printf("%lx", game->objtab[lvl][x][y]->id);
#elif (MJOL_CURSES)
    printw("%lx", game->objtab[lvl][x][y]->id);
#endif
}

int
mjolrefreshtty(void)
{
    return 1;
}

void
mjolclosetty(void)
{
#if (MJOL_CURSES)
    endwin();
#endif

    return;
}

void
mjolmkscrtty(struct mjolgame *game)
{
    struct mjolwindatatty *data = calloc(1, sizeof(struct mjolwindatatty));
#if (MJOL_CURSES)
    WINDOW                *win;
#endif

#if (MJOL_VT)
#elif (MJOL_CURSES)
    /* create main window */
    win = newwin(game->height, game->width, 0, 0);
    if (!win) {
        mjolclosetty();
        fprintf(stderr, "window creation failure\n");
        
        exit(1);
    }
    data->mainwin = win;
    touchwin(win);
    /* create game window */
    win = derwin(data->mainwin, game->height - 2, game->width, 1, 0);
    if (!win) {
        mjolclosetty();
        fprintf(stderr, "window creation failure\n");
        
        exit(1);
    }
    data->gamewin = win;
    /* create message window */
    win = derwin(data->mainwin, 1, game->width, 0, 0);
    if (!win) {
        mjolclosetty();
        fprintf(stderr, "window creation failure\n");
        
        exit(1);
    }
    data->msgwin = win;
    /* create status window */
    win = derwin(data->mainwin, 1, game->width, game->height - 1, 0);
    if (!win) {
        mjolclosetty();
        fprintf(stderr, "window creation failure\n");
        
        exit(1);
    }
    data->statwin = win;
#endif
    game->scr = calloc(1, sizeof(struct mjolscr));
    if (game->scr) {
        game->scr->data = data;
#if (MJOL_VT)
        game->scr->getch = mjolgetchtty;
#elif (MJOL_CURSES)
        game->scr->getch = getch;
#endif
        game->scr->moveto = mjolmovetotty;
        game->scr->drawchr = mjoldrawchrtty;
#if (MJOL_VT)
        game->scr->printmsg = printf;
        game->scr->refresh = mjolrefreshtty;
#elif (MJOL_CURSES)
        game->scr->printmsg = printw;
        game->scr->refresh = refresh;
#endif
        game->scr->close = mjolclosetty;
    } else {
        mjolclosetty();
        fprintf(stderr, "memory allocation failure\n");
        
        exit(1);
    }

    return;
}

void
mjolopentty(struct mjolgame *game)
{
#if (MJOL_VT)
    setvbuf(stdin, NULL, _IONBF, BUFSIZ);
#elif (MJOL_CURSES)
    initscr();
    cbreak();
    keypad(stdscr, TRUE);
    noecho();
#endif
    mjolmkscrtty(game);
#if 0
    game->scr->getch = mjolgetchtty;
    game->scr->moveto = mjolmovetotty;
    game->scr->drawchr = mjoldrawchrtty;
    game->scr->printmsg = printf;
#endif
        
    return;
}

#endif /* MJOL_TTY */

