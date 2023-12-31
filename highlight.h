/* Headers for syntax highlighting. */

#ifndef HIGHLIGHT_H
#define HIGHLIGHT_H

#define HL_HIGHLIGHT_NUMBERS (1 << 0)
#define HL_HIGHLIGHT_STRINGS (1 << 1)


enum HIGHLIGHT
{
    HL_NORMAL = 0,
    HL_COMMENT,
    HL_MLCOMMENT,
    HL_KEYWORDS1,
    HL_KEYWORDS2,
    HL_STRING,
    HL_NUMBER,
    HL_MATCH,
};


char *c_hl_extensions[5];
char *c_hl_keywords[40];
struct editorSyntax HLDB[5];

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))


int syntaxToColor(int);
void selectSyntaxHighlight();
void updateSyntax(editorRow *);

#endif
