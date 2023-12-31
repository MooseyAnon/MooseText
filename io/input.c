/* Handle low level keyboard input.*/

#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "core.h"

static int readEscapeSequence() {
    char seq[3];
    char failure = '\x1b';

    if (read(STDIN_FILENO, &seq[0], 1) != 1) { return failure; }
    if (read(STDIN_FILENO, &seq[1], 1) != 1) { return failure; }

    if (seq[0] == '[') {

        if (seq[1] >= '0' && seq[1] <= '9') {
            if (read(STDIN_FILENO, &seq[2], 1) != 1) { return failure; }

            if (seq[2] == '~') {
                switch (seq[1]) {
                    case '1': return HOME_KEY;
                    case '3': return DEL_KEY;
                    case '4': return END_KEY;
                    case '5': return PAGE_UP;
                    case '6': return PAGE_DOWN;
                    case '7': return HOME_KEY;
                    case '8': return END_KEY;
                }
            }
        }

        else if (seq[0] == 'O') {
            switch (seq[1]) {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }

        else {

            switch (seq[1]) {
                case 'A': return ARROW_UP;
                case 'B': return ARROW_DOWN;
                case 'C': return ARROW_RIGHT;
                case 'D': return ARROW_LEFT;
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }

    }

    return failure;
}


void moveCursorKeypress(int key) {
    editorRow *row = (CONFIG.cursorY >= CONFIG.numRows) ? NULL : &CONFIG.row[CONFIG.cursorY];

    switch(key) {
        case ARROW_LEFT:
            if (CONFIG.cursorX != 0) {
                CONFIG.cursorX--;
            }
            else if (CONFIG.cursorY > 0) {
                CONFIG.cursorY--;
                CONFIG.cursorX = CONFIG.row[CONFIG.cursorY].rowSize;
            }
            break;

        case ARROW_RIGHT:
            if (row && CONFIG.cursorX < row->rowSize) {
                CONFIG.cursorX++;
            }
            else if (row && CONFIG.cursorX == row->rowSize) {
                CONFIG.cursorY++;
                CONFIG.cursorX = 0;
            }
            break;

        case ARROW_DOWN:
            // cursorY starts from 0 (top left), so "down" == incrementing up
            if (CONFIG.cursorY < CONFIG.numRows) { CONFIG.cursorY++; }
            break;

        case ARROW_UP:
            // going up the page is taking cursorY back down to 0 as the cursor
            // starts at 0,0 top left hand side of the editor
            if (CONFIG.cursorY != 0) { CONFIG.cursorY--; }
            break;
    }

    row = (CONFIG.cursorY >= CONFIG.numRows) ? NULL : &CONFIG.row[CONFIG.cursorY];
    int rowLen = row ? row->rowSize : 0;

    if (CONFIG.cursorX > rowLen) { CONFIG.cursorX = rowLen; }
}


int readKey() {
    int nread;
    char c;

    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) { die("read"); } 
    }

    if (c == '\x1b') {
        return readEscapeSequence();
    }

    else {
        return c;
    }
}
