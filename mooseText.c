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

/*** data ***/

struct appendString
{
    char *s;
    int len;
};


typedef struct editorRow
{
    // stores a line of text as a pointer to dynamically alloc'ed
    // character data and its length
    int rowSize;
    char *characters;

    int renderSize;
    char *render;

    unsigned char *highlight;

} editorRow;


struct editorSyntax
{
    char *filetype;
    char *singleline_comment_start;
    char **filematch;
    char **keywords;
    int flags;
};


struct editorConfig
{
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
    struct termios original_struct;
};

struct editorConfig CONFIG;

/*** defines ***/

#define MOOSE_VERSION "0.0.1"
#define MOOSE_TAB_STOP 8
#define MOOSE_QUIT_TIMES 3

#define CTRL_KEY(k) ((k) & 0x1f)
#define APPENDSTRING_INIT {NULL, 0}

#define HL_HIGHLIGHT_NUMBERS (1 << 0)
#define HL_HIGHLIGHT_STRINGS (1 << 1)

/*** prototypes ***/

void append(struct appendString *, const char *, int);
int cursorBottomLeft();

void die(const char *);
void disableRawMode();
void drawRows(struct appendString *);
void drawStatusBar(struct appendString *);
void drawMessageBar(struct appendString *);

void editorInsertRow(int, char *, size_t);
void enableRawMode();
void editorOpen();
char * editorRowsToString(int *);
void editorScroll();
void editorUpdateRow(editorRow *);
int editorRowCxToRx(editorRow *, int);
int editorRowRxToCx(editorRow *, int);
void editorRowInsertChar(editorRow *, int, int);
void editorRowDelChar(editorRow *, int);
void editorFreerRow(editorRow *);
void editorSave();
void editorDelRow(int);
void editorRowAppendString(editorRow *, char *, size_t);

void find();
void findCallback(char *, int);

int getWindowSize(int *, int *);

void initEditor();
void insertChar(int);
void insertNewLine();

int is_separator(int);

void processKeypress();

char * prompt(char *, void (*callback)(char *, int));

int readKey();
int readEscapeSequence();
void refreshScreen();

void setStatusMessage(const char *, ...);
int syntaxToColor(int);

void selectSyntaxHighlight();

void updateSyntax(editorRow *);

enum KEYS
{
    BACKSPACE = 127,
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    END_KEY,
    HOME_KEY,
    PAGE_UP,
    PAGE_DOWN,
};


enum HIGHLIGHT
{
    HL_NORMAL = 0,
    HL_COMMENT,
    HL_KEYWORDS1,
    HL_KEYWORDS2,
    HL_STRING,
    HL_NUMBER,
    HL_MATCH,
};

/*** file types ***/

char *c_hl_extensions[] = {".c", ".h", ".cpp", NULL};
char *c_hl_keywords[] = {
    "switch", "if", "while", "for", "break", "continue", "return", "else",
    "struct", "union", "typedef", "static", "enum", "class", "case",

    "int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|",
    "void|", "NULL|", NULL
};


struct editorSyntax HLDB[] = {
    {
        "c", "//",
        c_hl_extensions,
        c_hl_keywords,
        HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS,
    }
};

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

/*** init ***/

int main(int argc, const char *argv[])
{
    enableRawMode();
    initEditor();

    if (argc >= 2) {
        editorOpen(argv[1]);
    }

    setStatusMessage("HELP:  Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find");

    while (1)
    {
        refreshScreen();
        processKeypress();
    }
    return 0;
}


void initEditor()
{
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


/*** terminal ***/

int cursorBottomLeft()
{
    int out = 0;

    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) { out = -1; }

    return out;
}


void die(const char *s)
{
    refreshScreen();

    perror(s);
    exit(1);
}


void disableRawMode()
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &CONFIG.original_struct) == -1) {
        die("tcsetattr");
    }
}


void enableRawMode()
{
    struct termios raw;

    if (tcgetattr(STDIN_FILENO, &CONFIG.original_struct) == -1) {
        die("tsgetattr");
    }

    atexit(disableRawMode);

    raw = CONFIG.original_struct;
    raw.c_cflag |= (CS8);
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        die("tcsetattr");
    }
}


int getCursorPosition(int *rows, int *cols)
{
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) { return -1; }

    while (i < sizeof(buf) - 1)
    {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) { break; }
        if (buf[i] == 'R') { break; }

        i++;
    }

    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[') { return -1; }
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) { return -1; }

    return 0;
}


int getWindowSize(int *rows, int *cols)
{
    struct winsize ws;
    int out = 0;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        out = cursorBottomLeft();
        if (out == 0) { return getCursorPosition(rows, cols); }
    }

    else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
    }

    return out;
}


int readKey()
{
    int nread;
    char c;

    while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
    {
        if (nread == -1 && errno != EAGAIN) { die("read"); } 
    }

    if (c == '\x1b') {
        return readEscapeSequence();
    }

    else {
        return c;
    }
}

int readEscapeSequence()
{
    char seq[3];
    char failure = '\x1b';

    if (read(STDIN_FILENO, &seq[0], 1) != 1) { return failure; }
    if (read(STDIN_FILENO, &seq[1], 1) != 1) { return failure; }

    if (seq[0] == '[') {

        if (seq[1] >= '0' && seq[1] <= '9') {
            if (read(STDIN_FILENO, &seq[2], 1) != 1) { return failure; }

            if (seq[2] == '~') {
                switch (seq[1])
                {
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
            switch (seq[1])
            {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }

        else {

            switch (seq[1])
            {
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


/*** file I/O ***/

void editorOpen(char *filename)
{
    free(CONFIG.filename);
    CONFIG.filename = strdup(filename);

    selectSyntaxHighlight();

    FILE *fp = fopen(filename, "r");
    if (!fp) { die("fopen"); }

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    while ((linelen = getline(&line, &linecap, fp)) != -1)
    {

        while (
            linelen > 0 && 
            (
                line[linelen - 1] == '\n'
                || line[linelen - 1] == '\r'
            )
        )
        {
            linelen--;
        }

        editorInsertRow(CONFIG.numRows, line, linelen);
    }

    free(line);
    fclose(fp);
    CONFIG.dirty = 0;
}


char * editorRowsToString(int *buflen)
{
    int totalLen = 0;
    int j;

    for (j = 0; j < CONFIG.numRows; j++)
    {
        totalLen += CONFIG.row[j].rowSize + 1;
    }
    *buflen = totalLen;

    char *buf = malloc(totalLen);
    char *p = buf;

    for (j = 0; j < CONFIG.numRows; j++)
    {
        memcpy(p, CONFIG.row[j].characters, CONFIG.row[j].rowSize);
        p += CONFIG.row[j].rowSize;
        *p = '\n';
        p++;
    }

    return buf;
}


void editorSave()
{
    if (CONFIG.filename == NULL) {
        CONFIG.filename = prompt("Enter filename to save as: %s", NULL);
        if (CONFIG.filename == NULL) {
            setStatusMessage("Save aborted!");
            return;
        }
        selectSyntaxHighlight();
    }

    int len;
    char *buf = editorRowsToString(&len);

    int fd = open(CONFIG.filename, O_RDWR | O_CREAT, 0644);
    if (fd != -1) {
        if (ftruncate(fd, len) != -1) {
            if (write(fd, buf, len) == len) {
                close(fd);
                free(buf);
                CONFIG.dirty = 0;
                setStatusMessage("%d bytes written to disk", len);
                return;
            }
        }
        close(fd);
    }
    free(buf);
    setStatusMessage(
        "Oh oh - didn't manage to save the file: %s", 
        strerror(errno)
    );
}

/*** row operations ***/

void editorFreerRow(editorRow *row)
{
    free(row->render);
    free(row->characters);
    free(row->highlight);
}


void editorDelRow(int at)
{
    if (at < 0 || at >= CONFIG.numRows) { return; }
    editorFreerRow(&CONFIG.row[at]);

    memmove(&CONFIG.row[at], &CONFIG.row[at + 1],
        sizeof(editorRow) * (CONFIG.numRows - at - 1)
    );
    CONFIG.numRows--;
    CONFIG.dirty++;

}


void editorRowAppendString(editorRow *row, char *s, size_t len)
{
    row->characters = realloc(row->characters, row->rowSize + len + 1);
    memcpy(&row->characters[row->rowSize], s, len);

    row->rowSize += len;
    row->characters[row->rowSize] = '\0';
    editorUpdateRow(row);
    CONFIG.dirty++;
}


int editorRowCxToRx(editorRow *row, int cursorX)
{
    int rx = 0;
    int j;

    for (j = 0; j < cursorX; j++)
    {
        if (row->characters[j] == '\t'){
            rx += (MOOSE_TAB_STOP - 1) - (rx % MOOSE_TAB_STOP);        
        }
        rx++;
    }
    return rx;
}


int editorRowRxToCx(editorRow *row, int rx)
{
    int cur_rx = 0;
    int cx;

    for (cx = 0; cx < row->rowSize; cx++)
    {
        if (row->characters[cx] == '\t') {
            cur_rx += (MOOSE_TAB_STOP - 1) - (cur_rx % MOOSE_TAB_STOP);
        }
        cur_rx++;

        if (cur_rx > rx) { return cx; }
    }
    return cx;
}


void editorRowInsertChar(editorRow *row, int at, int c)
{
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


void editorInsertRow(int at, char *s, size_t len)
{
    if (at < 0 || at > CONFIG.numRows) { return; }

    CONFIG.row = realloc(CONFIG.row, sizeof(editorRow) * (CONFIG.numRows + 1));
    memmove(
        &CONFIG.row[at + 1], &CONFIG.row[at],
        sizeof(editorRow) * (CONFIG.numRows - at)
    );

    CONFIG.row[at].rowSize = len;
    CONFIG.row[at].characters = malloc(len + 1);

    memcpy(CONFIG.row[at].characters, s, len);

    CONFIG.row[at].characters[len] = '\0';

    CONFIG.row[at].renderSize = 0;
    CONFIG.row[at].render = NULL;
    CONFIG.row[at].highlight = NULL;
    editorUpdateRow(&CONFIG.row[at]);

    CONFIG.numRows++;
    CONFIG.dirty++;
}


void editorRowDelChar(editorRow *row, int at)
{
    if (at < 0 || at >= row->rowSize) { return; }

    memmove(
        &row->characters[at], &row->characters[at + 1],
        row->rowSize - at
    );
    row->rowSize--;
    editorUpdateRow(row);

    CONFIG.dirty++;
}


void editorUpdateRow(editorRow *row)
{
    int tabs = 0;
    int j;

    for (j = 0; j < row->rowSize; j++)
    {
        if (row->characters[j] == '\t') { tabs++; }
    }

    free(row->render);
    row->render = malloc(row->rowSize + tabs * (MOOSE_TAB_STOP - 1) + 1);

    int i = 0;

    for (j = 0; j < row->rowSize; j++)
    {
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

/*** editor operations **/

void insertChar(int c)
{
    if (CONFIG.cursorY == CONFIG.numRows) {
        editorInsertRow(CONFIG.numRows,"", 0);
    }
    editorRowInsertChar(&CONFIG.row[CONFIG.cursorY], CONFIG.cursorX, c);
    CONFIG.cursorX++;
}


void delChar()
{
    if (CONFIG.cursorY == CONFIG.numRows) { return; }
    if (CONFIG.cursorX == 0 && CONFIG.cursorY == 0) { return; }

    editorRow *row = &CONFIG.row[CONFIG.cursorY];
    if (CONFIG.cursorX > 0) {
        editorRowDelChar(row, CONFIG.cursorX - 1);
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


void insertNewLine()
{
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

/*** input ***/

void moveCursorKeypress(int key)
{
    editorRow *row = (CONFIG.cursorY >= CONFIG.numRows) ? NULL : &CONFIG.row[CONFIG.cursorY];

    switch(key)
    {
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


void processKeypress()
{
    static int quit_times = MOOSE_QUIT_TIMES;

    int c = readKey();

    switch(c)
    {
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
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
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
            // this moves the cursor along with the deletion
            // moveCursorKeypress(ARROW_LEFT);
            break;

        case PAGE_UP:
        case PAGE_DOWN:
            {
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
                while (times--)
                {
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


void setStatusMessage(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    vsnprintf(CONFIG.statusMsg, sizeof(CONFIG.statusMsg), fmt, ap);
    va_end(ap);

    CONFIG.statusMsg_time = time(NULL);
}


char * prompt(char *prompt, void (*callback)(char *, int))
{
    size_t bufsize = 128;
    char *buf = malloc(bufsize);

    size_t buflen = 0;
    buf[0] = '\0';

    while (1)
    {
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

/*** output ***/

void drawMessageBar(struct appendString *as)
{
    append(as, "\x1b[K", 3);

    int msgLen = strlen(CONFIG.statusMsg);
    if (msgLen > CONFIG.screenCols) { msgLen = CONFIG.screenCols; }

    if (msgLen && time(NULL) - CONFIG.statusMsg_time < 5) {
        append(as, CONFIG.statusMsg, msgLen);
    }
}


void drawRows(struct appendString *as)
{
    for (int y = 0; y < CONFIG.screenRows; y++)
    {
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
            for (int j = 0; j < len; j++)
            {
                if (c[j] == HL_NORMAL) {
                if (iscntrl(c[j])) {
                    char sym = (c[j] <= 26) ? '@' + c[j] : '?';
                    append(as, "\x1b[7m", 4);
                    append(as, &sym, 1);
                    append(as, "\x1b[m", 3);

                    if (current_color != -1) {
                        char buf[16];
                        int clen = snprintf(
                            buf, sizeof(buf), "\x1b[%dm", current_color
                        );
                        append(as, buf, clen);
                    }
                }

                else if (hl[j] == HL_NORMAL) {
                    if (current_color != -1) {
                        append(as, "\x1b[39m", 5);
                        current_color = -1;
                    }
                    append(as, &c[j], 1);
                }

                else {
                    int color = syntaxToColor(hl[j]);
                    if (color != current_color) {
                        current_color = color;
                        char buf[16];
                        int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
                        append(as, buf, clen);
                    }
                    append(as, &c[j], 1);
                }
            }
            append(as, "\x1b[39m", 5);
        }
        // clear lines as we draw them
        append(as, "\x1b[K", 3);
        append(as, "\r\n", 2);
    }
}


void drawStatusBar(struct appendString *as)
{
    // inverts colours
    append(as, "\x1b[7m", 4);

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

    while (len < CONFIG.screenCols)
    {
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
    append(as, "\x1b[m", 3);
    append(as, "\r\n", 2);
}


void editorScroll()
{
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


void moveCursorToCxCyPosition(struct appendString *as)
{
    char buf[32];
    snprintf(
        buf, sizeof(buf), "\x1b[%d;%dH",
        (CONFIG.cursorY - CONFIG.rowOffset) + 1,
        (CONFIG.renderX - CONFIG.colOffset) + 1
    );

    append(as, buf, strlen(buf));
}


void refreshScreen()
{
    editorScroll();

    struct appendString as = APPENDSTRING_INIT;

    append(&as, "\x1b[?25l", 6);
    // clear whole screen
    // append(&as, "\x1b[2J", 4);
    append(&as, "\x1b[H", 3);

    drawRows(&as);
    drawStatusBar(&as);
    drawMessageBar(&as);

    moveCursorToCxCyPosition(&as);

    append(&as, "\x1b[?25h", 6);

    write(STDOUT_FILENO, as.s, as.len);
}

/*** appendString ***/

void append(struct appendString *as, const char *s, int len)
{
    char *newString = realloc(as->s, as->len + len);

    if (newString == NULL) { return; }

    memcpy(&newString[as->len], s, len);
    as->s = newString;
    as->len += len;

}

void stringFree(struct appendString *as)
{
    free(as->s);
}


/*** search ***/

void find()
{
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


void findCallback(char *query, int key)
{
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
    for (i = 0; i < CONFIG.numRows; i++)
    {
        current += direction;
        if (current == -1) { current = CONFIG.numRows - 1; }
        else if (current == CONFIG.numRows) { current = 0; }

        editorRow *row = &CONFIG.row[current];
        char *match = strstr(row->render, query);

        if (match) {
            last_match = current;
            CONFIG.cursorY = current;
            CONFIG.cursorX = editorRowCxToRx(row, match - row->render);
            CONFIG.rowOffset = CONFIG.numRows;

            save_hl_line = current;
            saved_hl = malloc(row->renderSize);
            memcpy(saved_hl, row->highlight, row->renderSize);
            memset(&row->highlight[match - row->render], HL_MATCH, strlen(query));
            break;
        }
    }
}

/*** syntax highlighting ***/

void updateSyntax(editorRow *row)
{
    row->highlight = realloc(row->highlight, row->renderSize);
    memset(row->highlight, HL_NORMAL, row->renderSize);

    if (CONFIG.syntax == NULL) { return; }

    char **keywords = CONFIG.syntax->keywords;

    char *scs = CONFIG.syntax->singleline_comment_start;
    int scs_len = scs ? strlen(scs) : 0;
    int prev_sep = 1;
    int in_string = 0;
    int i = 0;
    while (i < row->renderSize)
    {
        char c = row->render[i];
        unsigned char prev_hl = (i > 0) ? row->highlight[i - 1]: HL_NORMAL;

        if (scs_len && !in_string && !in_comment) {
            if (!strncmp(&row->render[i], scs, scs_len)) {
                memset(&row->highlight[i], HL_COMMENT, row->renderSize - i);
                break;
            }
        }

        if (CONFIG.syntax->flags & HL_HIGHLIGHT_STRINGS) {
            if (in_string) {
                row->highlight[i] = HL_STRING;

                if (c == '\\' && i + 1 < row->renderSize) {
                    row->highlight[i + 1] = HL_STRING;
                    i += 2;
                    continue;
                }

                if (c == in_string) { in_string = 0; }
                i++;
                prev_sep = 1;
                continue;
            }

            else {
                if (c == '"' || c == '\'') {
                    in_string = c;
                    row->highlight[i] = HL_STRING;
                    i++;
                    continue;
                }
            }
        }

        if (CONFIG.syntax->flags & HL_HIGHLIGHT_NUMBERS) {

            if (
                (isdigit(c) && (prev_sep || prev_hl == HL_NUMBER))
                || (c == '.' && prev_hl == HL_NUMBER)
            ) {
                row->highlight[i] = HL_NUMBER;
                i++;
                prev_sep = 0;
                continue;
            }
        }

        if (prev_sep) {
            int j;
            for (j = 0; keywords[j]; j++)
            {
                int klen = strlen(keywords[j]);
                int kw2 = keywords[j][klen - 1] == '|';

                if (kw2) { klen--; }

                if (
                    !strncmp(&row->render[i], keywords[j], klen)
                    && is_separator(row->render[i + klen])
                ) {
                    memset(
                        &row->highlight[i],
                        kw2 ? HL_KEYWORDS2 : HL_KEYWORDS1, klen
                    );
                    i += klen;
                    break;
                }
            }

            if (keywords[j] != NULL) {
                prev_sep = 0;
                continue;
            }
        }

        prev_sep = is_separator(c);
        i++;
    }
    }
}


int syntaxToColor(int hl)
{
    // based off colors from: https://en.wikipedia.org/wiki/ANSI_escape_code#Colors
    switch(hl)
    {
        case HL_COMMENT:
        case HL_KEYWORDS1: return 33;
        case HL_KEYWORDS2: return 32;
        case HL_STRING: return 35;
        case HL_NUMBER: return 31;
        case HL_MATCH: return 34;
        default: return 37;
    }
}


void selectSyntaxHighlight()
{
    CONFIG.syntax = NULL;
    if (CONFIG.filename == NULL) { return; }

    char *ext = strrchr(CONFIG.filename, '.');

    for (unsigned int j = 0; j < HLDB_ENTRIES; j++)
    {
        struct editorSyntax *s = &HLDB[j];

        unsigned int i = 0;
        while (s->filematch[i])
        {
            int is_ext = (s->filematch[i][0] == '.');
            if (
                (is_ext && ext && !strcmp(ext, s->filematch[i]))
                || (!is_ext && strstr(CONFIG.filename, s->filematch[i]))
            ) {
                CONFIG.syntax = s;

                int filerow;
                for (filerow = 0; filerow < CONFIG.numRows; filerow++)
                {
                    updateSyntax(&CONFIG.row[filerow]);
                }
                return;
            }
            i++;
        }
    }
}


int is_separator(int c)
{
    return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}
