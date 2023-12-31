/* Handle search functionality. */

#include <stdlib.h>
#include <string.h>

#include "core.h"
#include "highlight.h"
#include "io/output.h"
#include "ops/rowops.h"

static void findCallback(char *query, int key) {
    static int last_match = -1;
    static int direction = 1;

    static int save_hl_line;
    static char *saved_hl = NULL;

    if (saved_hl) {
        memcpy(
            CONFIG.row[save_hl_line].highlight, saved_hl,
            CONFIG.row[save_hl_line].renderSize
        );
        free(saved_hl);
        saved_hl = NULL;
    }

    if (key == '\r' || key == '\x1b') {
        last_match = -1;
        direction = 1;
        return;
    }

    else if (key == ARROW_RIGHT || key == ARROW_DOWN) {
        direction = 1;
    }

    else if (key == ARROW_LEFT || key == ARROW_UP) {
        direction = -1;
    }

    else {
        last_match = -1;
        direction = 1;
    }

    if (last_match == -1) { direction = 1; }

    int current = last_match;
    int i;
    for (i = 0; i < CONFIG.numRows; i++) {
        current += direction;
        if (current == -1) { current = CONFIG.numRows - 1; }
        else if (current == CONFIG.numRows) { current = 0; }

        editorRow *row = &CONFIG.row[current];
        char *match = strstr(row->render, query);

        if (match) {
            last_match = current;
            CONFIG.cursorY = current;
            CONFIG.cursorX = editorRowRxToCx(row, match - row->render);
            CONFIG.rowOffset = CONFIG.numRows;

            save_hl_line = current;
            saved_hl = malloc(row->renderSize);
            memcpy(saved_hl, row->highlight, row->renderSize);
            memset(&row->highlight[match - row->render], HL_MATCH, strlen(query));
            break;
        }
    }
}


void find() {
    int saved_cx = CONFIG.cursorX;
    int saved_cy = CONFIG.cursorY;
    int saved_colOff = CONFIG.colOffset;
    int saved_rowOff = CONFIG.rowOffset;

    char *query = prompt("Search: %s (Use ESC/Arrows/Enter)", findCallback);

    if (query) {
        free(query);
    }

    else {
        CONFIG.cursorX = saved_cx;
        CONFIG.cursorY = saved_cy;
        CONFIG.colOffset = saved_colOff;
        CONFIG.rowOffset = saved_rowOff;
    }
}
