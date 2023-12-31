/* Covert users terminal into raw mode. */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "core.h"
#include "escapecodes.h"
#include "termconf.h"


static struct termios terminal_attrs;  /* holds the original terminal settings */


static void disableRawMode() {
    if (write(STDIN_FILENO, PRIMARY_BUFFER, 15) != 15) {
        die("write");
    }

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal_attrs) == -1) {
        die("tcsetattr");
    }
}


void enableRawMode() {
    struct termios raw;

    if (tcgetattr(STDIN_FILENO, &terminal_attrs) == -1) {
        die("tsgetattr");
    }

    atexit(disableRawMode);  /* register handler for exiting */

    raw = terminal_attrs;  /* make a copy of the terminal setting so we can overwrite the copy */
    raw.c_cflag |= (CS8);
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        die("tcsetattr");
    }

    if (write(STDOUT_FILENO, ALTERNATE_BUFFER, 15) != 15) { die("write"); }
}


static int cursorBottomLeft() {
    int out = 0;

    if (write(STDOUT_FILENO, CURSOR_BOTTOM_RIGHT, 12) != 12) { out = -1; }

    return out;
}


static int getCursorPosition(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, CURRENT_CURSOR_POSITION, 4) != 4) { return -1; }

    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) { break; }
        if (buf[i] == 'R') { break; }

        i++;
    }

    buf[i] = '\0';

    if (buf[0] != ESCAPE || buf[1] != '[') { return -1; }
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) { return -1; }

    return 0;
}


int getWindowSize(int *rows, int *cols) {
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
