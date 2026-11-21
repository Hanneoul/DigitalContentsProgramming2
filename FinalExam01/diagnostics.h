#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H
#include <stdio.h>
#include <stdlib.h>
#ifdef _DEBUG
#include <crtdbg.h>
#endif

void diagnostics_begin(void);
int diagnostics_end(void);
FILE *checked_fopen(const char *path, const char *mode, const char *source, int line);
int checked_fopen_s(FILE **out, const char *path, const char *mode, const char *source, int line);
int checked_fclose(FILE *stream, const char *source, int line);

#ifndef DIAGNOSTICS_IMPLEMENTATION
#define fopen(path, mode) checked_fopen((path), (mode), __FILE__, __LINE__)
#define fopen_s(out, path, mode) checked_fopen_s((out), (path), (mode), __FILE__, __LINE__)
#define fclose(stream) checked_fclose((stream), __FILE__, __LINE__)
#ifdef _DEBUG
#define malloc(size) _malloc_dbg((size), _NORMAL_BLOCK, __FILE__, __LINE__)
#define calloc(count, size) _calloc_dbg((count), (size), _NORMAL_BLOCK, __FILE__, __LINE__)
#define realloc(ptr, size) _realloc_dbg((ptr), (size), _NORMAL_BLOCK, __FILE__, __LINE__)
#define free(ptr) _free_dbg((ptr), _NORMAL_BLOCK)
#endif
#endif
#endif
