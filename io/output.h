/* Headers for editor output. */

#ifndef OUTPUT_H
#define OUTPUT_H

char * prompt(char *, void (*callback)(char *, int));
void refreshScreen();

#endif
