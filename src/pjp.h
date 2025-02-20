// In exactly one C or C++ file in your project:
// #define PJP_IMPLEMENTATION
// #include "pjp.h"

#ifndef PJP_H
#define PJP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

size_t file_length(FILE *f);
unsigned char *read_file(const char *filename, size_t *plen);
char **read_file_lines(const char *filename, size_t *file_size, size_t *line_count);

#ifdef PJP_IMPLEMENTATION

size_t file_length(FILE *f) {
	long len, pos;
	pos = ftell(f);
	fseek(f, 0, SEEK_END);
	len = ftell(f);
	fseek(f, pos, SEEK_SET);
	return (size_t) len;
}

unsigned char *read_file(const char *filename, size_t *plen) {
    FILE *f = fopen(filename, "rb");
    size_t len = file_length(f);
    unsigned char *buffer = (unsigned char*)malloc(len);
    len = fread(buffer, 1, len, f);
    if (plen) *plen = len;
    fclose(f);
    return buffer;
}

char **read_file_lines(const char *filename, size_t *file_size, size_t *line_count) {
    FILE *f = fopen(filename, "rb");
    char *buffer, **list=NULL, *s;
    size_t len, count, i;

    if (!f) return NULL;
    len = file_length(f);
    buffer = (char *) malloc(len+1);
    len = fread(buffer, 1, len, f);
    buffer[len] = 0;
    fclose(f);
    if (file_size) *file_size = len;

    // two passes through: first time count lines, second time set them
    for (i = 0; i < 2; i++) {
        s = buffer;
        if (i == 1) {
            list[0] = s;
        }
        count = 1;
        while (*s) {
            if (*s == '\n' || *s == '\r') {
                // detect if both cr & lf are together
                int crlf = (s[0] + s[1]) == ('\n' + '\r');
                if (i == 1) *s = 0;
                if (crlf) s++;
                if (s[1]) { // it's not over yet
                    if (i == 1) list[count] = s+1;
                    count++;
                }
            }
            s++;
        }
        if (i == 0) {
            list = (char **) malloc(sizeof(*list) * (count+1) + len+1);
            if (!list) return NULL;
            list[count] = 0;
            // recopy the file so there's just a single allocation to free
            memcpy(&list[count+1], buffer, len + 1);
            free(buffer);
            buffer = (char*)&list[count + 1];
            if (line_count) *line_count = (int)count;
        }
    }
    return list;
}

#endif // PJP_IMPLEMENTATION

#ifdef __cplusplus
}
#endif

#endif // PJP_H
