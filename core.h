/* Core headers used across the system */

#ifndef CORE_H
#define CORE_H

#include <time.h>

#define MOOSE_VERSION "0.0.1"
#define MOOSE_TAB_STOP 8
#define MOOSE_QUIT_TIMES 3

#define CTRL_KEY(k) ((k) & 0x1f)
#define APPENDSTRING_INIT {NULL, 0}


struct appendString {
    char *s;
    int len;
};


enum KEYS {
    BACKSPACE = 127,
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    END_KEY,
    HOME_KEY,
    PAGE_UP,
    PAGE_DOWN
};


typedef struct editorRow {
    int index;
    // stores a line of text as a pointer to dynamically alloc'ed
    // character data and its length
    int rowSize;
    char *characters;

    int renderSize;
    char *render;

    unsigned char *highlight;
    int highlight_open_comment;

} editorRow;


struct editorSyntax {
    char *filetype;
    char *singleline_comment_start;
    char *multiline_comment_start;
    char *multiline_comment_end;
    char **filematch;
    char **keywords;
    int flags;
};


struct editorConfig {
    int cursorX;
    int cursorY;

    int colOffset;
    int rowOffset;

    int screenRows;
    int screenCols;

    int numRows;
    editorRow *row;

    char *filename;

    int renderX;  // indexs into the row.render array

    char statusMsg[80];
    time_t statusMsg_time;

    int dirty;

    struct editorSyntax *syntax;
};

struct editorConfig CONFIG;


void append(struct appendString *, const char *, int);
void die(const char *);
void setStatusMessage(const char *, ...);
void stringFree(struct appendString *);

#endif
