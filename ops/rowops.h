/* Row operation headers. */
#ifndef ROWOPS_H
#define ROWOPS_H

#include "core.h"

void editorInsertRow(int, char *, size_t);
void editorUpdateRow(editorRow *);
int editorRowCxToRx(editorRow *, int);
int editorRowRxToCx(editorRow *, int);
void editorRowInsertChar(editorRow *, int, int);
void editorRowDelChar(editorRow *, int);
void editorDelRow(int);
void editorRowAppendString(editorRow *, char *, size_t);

#endif
