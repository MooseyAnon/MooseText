/* Handle output to terminal screen. */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "core.h"
#include "escapecodes.h"
#include "input.h"
#include "highlight.h"
#include "output.h"
#include "ops/rowops.h"


static void drawMessageBar(struct appendString *as) {
    append(as, ERASE_IN_LINE, 3);

    int msgLen = strlen(CONFIG.statusMsg);
    if (msgLen > CONFIG.screenCols) { msgLen = CONFIG.screenCols; }

    if (msgLen && time(NULL) - CONFIG.statusMsg_time < 5) {
        append(as, CONFIG.statusMsg, msgLen);
    }
}


static void drawRows(struct appendString *as) {
    for (int y = 0; y < CONFIG.screenRows; y++) {
        int fileRow = y + CONFIG.rowOffset;
        if (fileRow>= CONFIG.numRows) {
            if (CONFIG.numRows == 0 && y == CONFIG.screenRows / 3 ) {
                char welcome[80];

                int welcomeLen = snprintf(
                    welcome, sizeof(welcome),
                    "MOOSE EDITOR -- VERSION %s", MOOSE_VERSION);

                if (welcomeLen > CONFIG.screenCols) {
                    welcomeLen = CONFIG.screenCols;
                }

                int padding = (CONFIG.screenCols - welcomeLen) / 2;

                if (padding) {
                    append(as, "~", 1);
                    padding--;
                }

                while (padding--) { append(as, " ", 1); }
                append(as, welcome, welcomeLen);
            }

            else {
                append(as, "~", 1);
            }
        }

        else {
            int len = CONFIG.row[fileRow].renderSize - CONFIG.colOffset;
            if (len < 0) { len = 0; }

            if (len > CONFIG.screenCols) { len = CONFIG.screenCols; }

            char *c = &CONFIG.row[fileRow].render[CONFIG.colOffset];
            unsigned char *hl = &CONFIG.row[fileRow].highlight[CONFIG.colOffset];
            int current_color = -1;
            int j;
            for (j = 0; j < len; j++) {
                if (iscntrl(c[j])) {
                    char sym = (c[j] <= 26) ? '@' + c[j] : '?';
                    append(as, SELECT_GRAPHIC_RENDITION_INVERT, 4);
                    append(as, &sym, 1);
                    append(as, SELECT_GRAPHIC_RENDITION_DEFFAULT, 3);

                    if (current_color != -1) {
                        char buf[16];
                        int clen = snprintf(
                            buf, sizeof(buf), CUSTOM_COLOR, current_color
                        );
                        append(as, buf, clen);
                    }
                }

                else if (hl[j] == HL_NORMAL) {
                    if (current_color != -1) {
                        append(as, DEFAULT_COLOR, 5);
                        current_color = -1;
                    }
                    append(as, &c[j], 1);
                }

                else {
                    int color = syntaxToColor(hl[j]);
                    if (color != current_color) {
                        current_color = color;
                        char buf[16];
                        int clen = snprintf(buf, sizeof(buf), CUSTOM_COLOR, color);
                        append(as, buf, clen);
                    }
                    append(as, &c[j], 1);
                }
            }
            append(as, DEFAULT_COLOR, 5);
        }
        // clear lines as we draw them
        append(as, ERASE_IN_LINE, 3);
        append(as, "\r\n", 2);
    }
}


static void drawStatusBar(struct appendString *as) {
    // inverts colours
    append(as, SELECT_GRAPHIC_RENDITION_INVERT, 4);

    char status[80];
    char rstatus[80];  // holds current line number

    int len = snprintf(
        status, sizeof(status), "%.20s - %d lines %s",
        CONFIG.filename ? CONFIG.filename : "[No Name]", CONFIG.numRows,
        CONFIG.dirty ? "(modified)" : ""
    );
    int rlen = snprintf(
        rstatus, sizeof(rstatus), "%s | %d/%d",
        CONFIG.syntax ? CONFIG.syntax->filetype : "no ft",
        CONFIG.cursorY + 1, CONFIG.numRows
    );

    if (len > CONFIG.screenCols) { len = CONFIG.screenCols; }
    append(as, status, len);

    while (len < CONFIG.screenCols) {
        if (CONFIG.screenCols - len == rlen) {
            append(as, rstatus, rlen);
            break;
        }

        else {
            append(as, " ", 1);
            len++;
        }
    }

    // reverts back to normal formatting
    append(as, SELECT_GRAPHIC_RENDITION_DEFFAULT, 3);
    append(as, "\r\n", 2);
}


static void editorScroll() {
    CONFIG.renderX = 0;
    if (CONFIG.cursorY < CONFIG.numRows) {
        CONFIG.renderX = editorRowCxToRx(
            &CONFIG.row[CONFIG.cursorY], CONFIG.cursorX);
    }

    // is cursor above the visible window? If so, scroll up to cursor
    if (CONFIG.cursorY < CONFIG.rowOffset) {
        CONFIG.rowOffset = CONFIG.cursorY;
    }

    // is cursor below the visible window? If so, scroll down to cursor.
    if (CONFIG.cursorY >= CONFIG.rowOffset + CONFIG.screenRows) {
        CONFIG.rowOffset = CONFIG.cursorY - CONFIG.screenRows + 1;
    }

    // cursor is to the left of visible window
    if (CONFIG.renderX < CONFIG.colOffset) {
        CONFIG.colOffset = CONFIG.renderX;
    }

    // cursor is to the right of visible window
    if (CONFIG.renderX >= CONFIG.colOffset + CONFIG.screenCols) {
        CONFIG.colOffset = CONFIG.renderX - CONFIG.screenCols + 1;
    }
}


static void moveCursorToCxCyPosition(struct appendString *as) {
    char buf[32];
    snprintf(
        buf, sizeof(buf), CUSTOM_CURSOR_POSITION,
        (CONFIG.cursorY - CONFIG.rowOffset) + 1,
        (CONFIG.renderX - CONFIG.colOffset) + 1
    );

    append(as, buf, strlen(buf));
}


void refreshScreen() {
    editorScroll();

    struct appendString as = APPENDSTRING_INIT;

    append(&as, HIDE_CURSOR, 6);
    append(&as, CURSOR_TOP_LEFT, 3);

    drawRows(&as);
    drawStatusBar(&as);
    drawMessageBar(&as);

    moveCursorToCxCyPosition(&as);

    append(&as, SHOW_CURSOR, 6);

    write(STDOUT_FILENO, as.s, as.len);
    stringFree(&as);
}


char * prompt(char *prompt, void (*callback)(char *, int)) {

    size_t bufsize = 128;
    char *buf = malloc(bufsize);

    size_t buflen = 0;
    buf[0] = '\0';

    while (1) {
        setStatusMessage(prompt, buf);
        refreshScreen();

        int c = readKey();

        if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
            if (buflen != 0) { buf[--buflen] = '\0'; }
        }

        // add escape key
        else if (c == '\x1b') {
            setStatusMessage("");
            if (callback) { callback(buf, c); }
            free(buf);
            return NULL;
        }

        else if (c == '\r') {
            if (buflen != 0) {
                setStatusMessage("");
                if (callback) { callback(buf, c); }
                return buf;
            }
        }

        else if (!iscntrl(c) && c < 128) {
            if (buflen == bufsize - 1) {
                bufsize *= 2;
                buf = realloc(buf, bufsize);
            }
            buf[buflen++] = c;
            buf[buflen] = '\0';
        }

        if (callback) { callback(buf, c); }
    }
}
