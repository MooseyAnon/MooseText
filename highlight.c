/* Text Highlighting. */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "core.h"
#include "highlight.h"

char *c_hl_extensions[] = {".c", ".h", ".cpp", NULL};
char *c_hl_keywords[] = {
    "switch", "if", "while", "for", "break", "continue", "return", "else",
    "struct", "union", "typedef", "static", "enum", "class", "case",

    "int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|",
    "void|", "NULL|", NULL
};

struct editorSyntax HLDB[] = {
    {
        "c", "//", "/*", "*/",
        c_hl_extensions,
        c_hl_keywords,
        HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS,
    }
};


static int is_separator(int c) {
    return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}


void updateSyntax(editorRow *row) {
    row->highlight = realloc(row->highlight, row->renderSize);
    memset(row->highlight, HL_NORMAL, row->renderSize);

    if (CONFIG.syntax == NULL) { return; }

    char **keywords = CONFIG.syntax->keywords;

    char *scs = CONFIG.syntax->singleline_comment_start;
    char *mcs = CONFIG.syntax->multiline_comment_start;
    char *mce = CONFIG.syntax->multiline_comment_end;

    int scs_len = scs ? strlen(scs) : 0;
    int mcs_len = mcs ? strlen(mcs) : 0;
    int mce_len = mce ? strlen(mce) : 0;

    int prev_sep = 1;
    int in_string = 0;
    int in_comment = (
        row->index > 0 && CONFIG.row[row->index - 1].highlight_open_comment
    );

    int i = 0;
    while (i < row->renderSize) {
        char c = row->render[i];
        unsigned char prev_hl = (i > 0) ? row->highlight[i - 1]: HL_NORMAL;

        if (scs_len && !in_string && !in_comment) {
            if (!strncmp(&row->render[i], scs, scs_len)) {
                memset(&row->highlight[i], HL_COMMENT, row->renderSize - i);
                break;
            }
        }

        if (mcs_len && mce_len && !in_string) {
            if (in_comment) {
                row->highlight[i] = HL_MLCOMMENT;

                if (!strncmp(&row->render[i], mce, mce_len)) {
                    memset(&row->highlight[i], HL_MLCOMMENT, mce_len);

                    i += mce_len;
                    in_comment = 0;
                    prev_sep = 1;
                    continue;
                }

                else {
                    i++;
                    continue;
                }
            }

            else if (!strncmp(&row->render[i], mcs, mcs_len)) {
                memset(&row->highlight[i], HL_MLCOMMENT, mcs_len);
                i += mcs_len;
                in_comment = 1;
                continue;
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
            for (j = 0; keywords[j]; j++) {
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

    int changed = (row->highlight_open_comment != in_comment);
    row->highlight_open_comment = in_comment;
    if (changed && row->index + 1 < CONFIG.numRows) {
        updateSyntax(&CONFIG.row[row->index + 1]);
    }
}


int syntaxToColor(int hl) {
    // based off colors from: https://en.wikipedia.org/wiki/ANSI_escape_code#Colors
    switch(hl) {
        case HL_COMMENT:
        case HL_MLCOMMENT: return 34;            
        case HL_KEYWORDS1: return 33;
        case HL_KEYWORDS2: return 32;
        case HL_STRING: return 35;
        case HL_NUMBER: return 31;
        case HL_MATCH: return 34;
        default: return 37;
    }
}


void selectSyntaxHighlight() {
    CONFIG.syntax = NULL;
    if (CONFIG.filename == NULL) { return; }

    char *ext = strrchr(CONFIG.filename, '.');

    for (unsigned int j = 0; j < HLDB_ENTRIES; j++) {
        struct editorSyntax *s = &HLDB[j];

        unsigned int i = 0;
        while (s->filematch[i]) {
            int is_ext = (s->filematch[i][0] == '.');
            if (
                (is_ext && ext && !strcmp(ext, s->filematch[i]))
                || (!is_ext && strstr(CONFIG.filename, s->filematch[i]))
            ) {
                CONFIG.syntax = s;

                int filerow;
                for (filerow = 0; filerow < CONFIG.numRows; filerow++) {
                    updateSyntax(&CONFIG.row[filerow]);
                }
                return;
            }
            i++;
        }
    }
}
