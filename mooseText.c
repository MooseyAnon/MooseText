  /*** includes ***/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <time.h>
#include <termios.h>
#include <unistd.h>

#include "core.h"
#include "termconf.h"
#include "highlight.h"
#include "search.h"
#include "io/file.h"
#include "io/input.h"
#include "io/output.h"
#include "ops/editorops.h"
#include "ops/rowops.h"

static void initEditor() {
    CONFIG.cursorX = 0;
    CONFIG.cursorY = 0;
    CONFIG.renderX = 0;

    CONFIG.colOffset = 0;
    CONFIG.rowOffset = 0;

    CONFIG.row = NULL;
    CONFIG.filename = NULL;

    CONFIG.statusMsg[0] = '\0';
    CONFIG.statusMsg_time = 0;

    CONFIG.dirty = 0;

    CONFIG.syntax = NULL;

    if (getWindowSize(&CONFIG.screenRows, &CONFIG.screenCols) == -1) {
        die("getWindowSize");
    }
    CONFIG.screenRows -= 2;
}



static void processKeypress() {
    static int quit_times = MOOSE_QUIT_TIMES;

    int c = readKey();

    switch(c) {
        case '\r':
            insertNewLine();
            break;

        case CTRL_KEY('q'):
            if (CONFIG.dirty && quit_times > 0) {
                setStatusMessage(
                    "WARNING!!! File has unsaved changes. "
                    "Press Ctrl-Q %d more times to quit.", quit_times
                );
                quit_times--;
                return;
            }
            exit(0);
            break;

        case CTRL_KEY('s'):
            editorSave();
            break;

        case HOME_KEY:
            if (CONFIG.cursorY < CONFIG.numRows) {
                CONFIG.cursorX = CONFIG.row[CONFIG.cursorY].rowSize;
            }
            break;

        case END_KEY:
            CONFIG.cursorX = CONFIG.screenCols - 1;
            break;

        case CTRL_KEY('f'):
            find();
            break;

        case BACKSPACE:
        case CTRL_KEY('h'):
        case DEL_KEY:
            if (c == DEL_KEY) { moveCursorKeypress(ARROW_RIGHT); }
            delChar();
            break;

        case PAGE_UP:
        case PAGE_DOWN: {
                if (c == PAGE_UP) {
                    CONFIG.cursorY = CONFIG.rowOffset;
                }

                else if (c == PAGE_DOWN) {
                    CONFIG.cursorY = CONFIG.rowOffset + CONFIG.screenRows - 1;

                    if (CONFIG.cursorY > CONFIG.numRows) {
                        CONFIG.cursorY = CONFIG.numRows;
                    }
                }

                int times = CONFIG.screenRows;
                while (times--) {
                    moveCursorKeypress(c == PAGE_UP ? ARROW_UP: ARROW_DOWN);
                }
            }
            break;

        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            moveCursorKeypress(c);
            break;

        case CTRL_KEY('l'):
        case '\x1b':
            break;

        default:
            insertChar(c);
            break;
    }

    quit_times = MOOSE_QUIT_TIMES;
}


int main(int argc, const char *argv[]) {
    enableRawMode();
    initEditor();

    if (argc >= 2) {
        editorOpen(argv[1]);
    }

    setStatusMessage("HELP:  Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find");

    while (1) {
        refreshScreen();
        processKeypress();
    }
    return 0;
}
