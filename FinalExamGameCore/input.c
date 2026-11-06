#include <windows.h>
#include <ctype.h>
#include "game.h"

static DWORD original_mode;
static int restore_mode;

/* Restore normal line editing after ESC, EOF or normal game completion. */
static void input_restore(void)
{
    if (restore_mode) SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), original_mode);
}

/* getchar remains the input API; console line mode is disabled for single keys. */
void input_init(void)
{
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    if (GetConsoleMode(input, &original_mode)) {
        restore_mode = SetConsoleMode(input, original_mode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT));
        if (restore_mode) atexit(input_restore);
    }
}

/* Decode exactly the displayed control scheme, including context-dependent drop. */
int input_decode(int ch, int has_key, Command *command)
{
    const char *keys = "wsadtgfhikjl";
    const char *key;
    if (ch == EOF || ch == 0) return 0;
    ch = tolower((unsigned char)ch);
    key = strchr(keys, ch);
    if (!key) return 0;
    *command = (Command)(key - keys);
    if (*command >= COMMAND_PICK_UP && has_key) *command = (Command)(*command + 4);
    return 1;
}

/* Wait until a legal key is received. ESC and redirected-input EOF terminate play. */
int input_command(int has_key, Command *command)
{
    int ch;
    printf("H0 입력: 이동 wsad | 공격 tgfh | %s ikjl | ESC 종료\n", has_key ? "버리기" : "집기");
    fflush(stdout);
    while ((ch = getchar()) != EOF) {
        if (ch == 27) return 0;
        if (input_decode(ch, has_key, command)) return 1;
    }
    return 0;
}
