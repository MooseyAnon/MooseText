/* Escape codes for controlling terminal. */

#ifndef ESCAPE_CODES_H
#define ESCAPE_CODES_H

#define ALTERNATE_BUFFER "\033[?1049h\033[2J\033[H"
#define PRIMARY_BUFFER "\033[2J\033[H\033[?1049l"
/* 
* Ctrl-F for more info on escape codes and args:
*    - https://vt100.net/docs/vt100-ug/chapter3.html
*/
#define CUSTOM_CURSOR_POSITION "\x1b[%d;%dH"
#define ESCAPE '\x1b'
#define CLEAR_SCREEN "\x1b[2J"
#define CURSOR_TOP_LEFT "\x1b[H"
#define CURSOR_BOTTOM_RIGHT "\x1b[999C\x1b[999B"
#define CURRENT_CURSOR_POSITION "\x1b[6n"
#define HIDE_CURSOR "\x1b[?25l"
#define SHOW_CURSOR "\x1b[?25h"
#define ERASE_IN_LINE "\x1b[K"

/*
* Generally speaking '[m' is used to set the colour of the terminal.
* It can be used to set the foreground i.e. character colours or background
* colours.
*/
#define CUSTOM_COLOR "\x1b[%dm"
#define DEFAULT_COLOR "\x1b[39m"
#define SELECT_GRAPHIC_RENDITION_INVERT "\x1b[7m"   /* invert the colour of the text or background */
#define SELECT_GRAPHIC_RENDITION_DEFFAULT "\x1b[m"  /* return text formatting to normal */
#define RED "\x1b[31m"
#endif
