#include <windows.h>
#include <ctype.h>
#include "game.h"

static DWORD original_mode;
static int restore_mode;
/* Restore the console after the one-time start key. */
static void input_restore(void)
{
    if (restore_mode) SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), original_mode);
    restore_mode = 0;
}
/* Only the start screen uses input; there is no human-controlled actor. */
void input_init(void)
{
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    if (GetConsoleMode(input, &original_mode)) {
        restore_mode = SetConsoleMode(input, original_mode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT));
        if (restore_mode) atexit(input_restore);
    }
}
/* Ignore other keys until s/S; restore console mode before autonomous play. */
int input_wait_start(void)
{
    int ch;
    puts("Press 's' Key");
    fflush(stdout);
    while ((ch = getchar()) != EOF) {
        if (tolower((unsigned char)ch) == 's') { input_restore(); return 1; }
    }
    input_restore();
    return 0;
}
