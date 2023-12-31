/* Core shared editor functionality. */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core.h"


void die(const char *s) {
    perror(s);
    exit(1);
}


void setStatusMessage(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    vsnprintf(CONFIG.statusMsg, sizeof(CONFIG.statusMsg), fmt, ap);
    va_end(ap);

    CONFIG.statusMsg_time = time(NULL);
}


void append(struct appendString *as, const char *s, int len) {
    char *newString = realloc(as->s, as->len + len);

    if (newString == NULL) { return; }

    memcpy(&newString[as->len], s, len);
    as->s = newString;
    as->len += len;

}

void stringFree(struct appendString *as) {
    free(as->s);
}
