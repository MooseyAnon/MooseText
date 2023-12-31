/* Handle operations on the editor. */

#include "core.h"
#include "rowops.h"

void insertChar(int c) {
    if (CONFIG.cursorY == CONFIG.numRows) {
        editorInsertRow(CONFIG.numRows,"", 0);
    }
    editorRowInsertChar(&CONFIG.row[CONFIG.cursorY], CONFIG.cursorX, c);
    CONFIG.cursorX++;
}


void delChar() {
    if (CONFIG.cursorY == CONFIG.numRows) { return; }
    if (CONFIG.cursorX == 0 && CONFIG.cursorY == 0) { return; }
    editorRow *row = &CONFIG.row[CONFIG.cursorY];
    if (CONFIG.cursorX > 0) {
        editorRowDelChar(row, CONFIG.cursorX - 1);
        // this moves the cursor along with the deletion
        CONFIG.cursorX--;
    }
    else {
        CONFIG.cursorX = CONFIG.row[CONFIG.cursorY - 1].rowSize;
        editorRowAppendString(&CONFIG.row[CONFIG.cursorY - 1],
            row->characters, row->rowSize
        );
        editorDelRow(CONFIG.cursorY);
        CONFIG.cursorY--;
    }
}


void insertNewLine() {
    if (CONFIG.cursorX == 0) {
        editorInsertRow(CONFIG.cursorY, "", 0);
    }
    else {
        editorRow *row = &CONFIG.row[CONFIG.cursorY];
        editorInsertRow(
            CONFIG.cursorY + 1, &row->characters[CONFIG.cursorX],
            row->rowSize - CONFIG.cursorX
        );
        row = &CONFIG.row[CONFIG.cursorY];
        row->rowSize = CONFIG.cursorX;
        row->characters[row->rowSize] = '\0';
        editorUpdateRow(row);
    }
    CONFIG.cursorY++;
    CONFIG.cursorX = 0;
}
