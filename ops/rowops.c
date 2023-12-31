/* Handle operations on a single row. */

#include <stdlib.h>
#include <string.h>

#include "core.h"
#include "highlight.h"

static void editorFreerRow(editorRow *row) {
    free(row->render);
    free(row->characters);
    free(row->highlight);
}


void editorUpdateRow(editorRow *row) {
    int tabs = 0;
    int j;

    for (j = 0; j < row->rowSize; j++) {
        if (row->characters[j] == '\t') { tabs++; }
    }

    free(row->render);
    row->render = malloc(row->rowSize + tabs * (MOOSE_TAB_STOP - 1) + 1);

    int i = 0;

    for (j = 0; j < row->rowSize; j++) {
        if (row->characters[j] == '\t') {
            row->render[i++] = ' ';

            while (i % MOOSE_TAB_STOP != 0) { row->render[i++] = ' '; }
        }

        else {
            row->render[i++] = row->characters[j];
        }
    }
    row->render[i] = '\0';
    row->renderSize = i;

    updateSyntax(row);
}


void editorDelRow(int at) {
    if (at < 0 || at >= CONFIG.numRows) { return; }
    editorFreerRow(&CONFIG.row[at]);

    memmove(&CONFIG.row[at], &CONFIG.row[at + 1],
        sizeof(editorRow) * (CONFIG.numRows - at - 1)
    );
    for (int j = at; j <= CONFIG.numRows - 1; j++) { CONFIG.row[j].index--; }
    CONFIG.numRows--;
    CONFIG.dirty++;

}


void editorRowAppendString(editorRow *row, char *s, size_t len) {
    row->characters = realloc(row->characters, row->rowSize + len + 1);
    memcpy(&row->characters[row->rowSize], s, len);

    row->rowSize += len;
    row->characters[row->rowSize] = '\0';
    editorUpdateRow(row);
    CONFIG.dirty++;
}


int editorRowCxToRx(editorRow *row, int cursorX) {
    int rx = 0;
    int j;

    for (j = 0; j < cursorX; j++) {
        if (row->characters[j] == '\t') {
            rx += (MOOSE_TAB_STOP - 1) - (rx % MOOSE_TAB_STOP);        
        }
        rx++;
    }
    return rx;
}


int editorRowRxToCx(editorRow *row, int rx) {
    int cur_rx = 0;
    int cx;

    for (cx = 0; cx < row->rowSize; cx++) {
        if (row->characters[cx] == '\t') {
            cur_rx += (MOOSE_TAB_STOP - 1) - (cur_rx % MOOSE_TAB_STOP);
        }
        cur_rx++;

        if (cur_rx > rx) { return cx; }
    }
    return cx;
}


void editorRowInsertChar(editorRow *row, int at, int c) {
    if (at < 0 || at > row->rowSize) { at = row->rowSize; }

    row->characters = realloc(row->characters, row->rowSize + 2);
    memmove(
        &row->characters[at + 1], &row->characters[at],
        row->rowSize - at + 1
    );
    row->rowSize++;
    row->characters[at] = c;
    editorUpdateRow(row);

    CONFIG.dirty++;
}


void editorRowDelChar(editorRow *row, int at) {
    if (at < 0 || at >= row->rowSize) { return; }

    memmove(
        &row->characters[at], &row->characters[at + 1],
        row->rowSize - at
    );
    row->rowSize--;
    editorUpdateRow(row);

    CONFIG.dirty++;
}


void editorInsertRow(int at, char *s, size_t len) {
    if (at < 0 || at > CONFIG.numRows) { return; }

    CONFIG.row = realloc(CONFIG.row, sizeof(editorRow) * (CONFIG.numRows + 1));
    memmove(
        &CONFIG.row[at + 1], &CONFIG.row[at],
        sizeof(editorRow) * (CONFIG.numRows - at)
    );
    for (int j = at + 1; j <= CONFIG.numRows; j++) { CONFIG.row[j].index++; }

    CONFIG.row[at].index = at;
    CONFIG.row[at].rowSize = len;
    CONFIG.row[at].characters = malloc(len + 1);

    memcpy(CONFIG.row[at].characters, s, len);

    CONFIG.row[at].characters[len] = '\0';

    CONFIG.row[at].renderSize = 0;
    CONFIG.row[at].render = NULL;
    CONFIG.row[at].highlight = NULL;
    CONFIG.row[at].highlight_open_comment = 0;
    editorUpdateRow(&CONFIG.row[at]);

    CONFIG.numRows++;
    CONFIG.dirty++;
}
