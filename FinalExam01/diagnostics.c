#define DIAGNOSTICS_IMPLEMENTATION
#include "diagnostics.h"
#include <errno.h>

typedef struct OpenFile {
    FILE *stream;
    const char *source;
    int line;
} OpenFile;
static OpenFile files[256];
static int errors;

/* Route CRT diagnostics to the per-worker stderr log and check heap integrity. */
void diagnostics_begin(void)
{
#ifdef _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
}

/* Remember the opening source location without allocating tracker memory. */
FILE *checked_fopen(const char *path, const char *mode, const char *source, int line)
{
    int i;
    FILE *stream = fopen(path, mode);
    if (!stream) return NULL;
    for (i = 0; i < 256; ++i) {
        if (!files[i].stream) {
            files[i].stream = stream;
            files[i].source = source;
            files[i].line = line;
            return stream;
        }
    }
    fprintf(stderr, "%s(%d): file tracker full; open rejected\n", source, line);
    ++errors;
    fclose(stream);
    errno = EMFILE;
    return NULL;
}

/* Provide the same tracked ownership for the secure CRT fopen_s API. */
int checked_fopen_s(FILE **out, const char *path, const char *mode, const char *source, int line)
{
    if (!out || !path || !mode) return EINVAL;
    *out = checked_fopen(path, mode, source, line);
    return *out ? 0 : (errno ? errno : EIO);
}

/* Remove known streams; report unknown/double closes rather than using stale FILEs. */
int checked_fclose(FILE *stream, const char *source, int line)
{
    int i;
    for (i = 0; i < 256; ++i) {
        if (stream && files[i].stream == stream) {
            files[i].stream = NULL;
            return fclose(stream);
        }
    }
    fprintf(stderr, "%s(%d): invalid/untracked fclose (possibly double close)\n", source, line);
    ++errors;
    return EOF;
}

/* Report and close leaked streams, then dump remaining CRT heap allocations. */
int diagnostics_end(void)
{
    int i, leaks = errors;
    for (i = 0; i < 256; ++i) {
        if (files[i].stream) {
            fprintf(stderr, "%s(%d): FILE LEAK: missing fclose\n", files[i].source, files[i].line);
            fclose(files[i].stream);
            files[i].stream = NULL;
            ++leaks;
        }
    }
#ifdef _DEBUG
    if (!_CrtCheckMemory()) ++leaks;
    if (_CrtDumpMemoryLeaks()) ++leaks;
#endif
    return leaks;
}
