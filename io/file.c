/* Handle file I/O. */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "core.h"
#include "highlight.h"
#include "output.h"
#include "ops/rowops.h"

void editorOpen(char *filename) {
    free(CONFIG.filename);
    CONFIG.filename = strdup(filename);

    selectSyntaxHighlight();

    FILE *fp = fopen(filename, "r");
    if (!fp) { die("fopen"); }

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    while ((linelen = getline(&line, &linecap, fp)) != -1) {

        while (
            linelen > 0 && 
            (
                line[linelen - 1] == '\n'
                || line[linelen - 1] == '\r'
            )
        ) {
            linelen--;
        }

        editorInsertRow(CONFIG.numRows, line, linelen);
    }

    free(line);
    fclose(fp);
    CONFIG.dirty = 0;
}


static char * editorRowsToString(int *buflen) {
    int totalLen = 0;
    int j;

    for (j = 0; j < CONFIG.numRows; j++) {
        totalLen += CONFIG.row[j].rowSize + 1;
    }
    *buflen = totalLen;

    char *buf = malloc(totalLen);
    char *p = buf;

    for (j = 0; j < CONFIG.numRows; j++) {
        memcpy(p, CONFIG.row[j].characters, CONFIG.row[j].rowSize);
        p += CONFIG.row[j].rowSize;
        *p = '\n';
        p++;
    }

    return buf;
}


void editorSave() {
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
