#include <windows.h>
#include "game.h"

static int color;
static const char *player_colors[PLAYER_COUNT] = {"\x1b[91m", "\x1b[92m", "\x1b[94m", "\x1b[95m", "\x1b[96m", "\x1b[97m"};
static const char *esc(const char *sequence) { return color ? sequence : ""; }

void display_init(int plain)
{
    DWORD mode;
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleOutputCP(CP_UTF8);
    color = !plain && GetConsoleMode(output, &mode) && SetConsoleMode(output, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

void display_game(const Game *game, const char *message)
{
    int row, col, i;
    printf("%sDungeon Escape | Round %d / Turn %d\n", esc("\x1b[2J\x1b[H"), game->round, game->turn);
    for (row = 0; row < game->size; ++row) {
        for (col = 0; col < game->size; ++col) {
            int occupant = -1, count = 0;
            for (i = 0; i < PLAYER_COUNT; ++i) {
                if (game->players[i].row == row && game->players[i].col == col) {
                    ++count;
                    if (occupant < 0 || i == game->key_owner) occupant = i;
                }
            }
            if (occupant >= 0) printf("%s%c%d%s", esc(player_colors[occupant]), count > 1 ? '+' : 'P', occupant, esc("\x1b[0m"));
            else if (game->terrain[row][col] == '#') printf("%s##%s", esc("\x1b[100;90m"), esc("\x1b[0m"));
            else if (game->terrain[row][col] == 'E') printf("%s()%s", esc(game->winner >= 0 ? "\x1b[92m" : "\x1b[91m"), esc("\x1b[0m"));
            else if (game->key_owner < 0 && game->key.row == row && game->key.col == col) printf("%s@-%s", esc("\x1b[93m"), esc("\x1b[0m"));
            else printf("  ");
        }
        putchar('\n');
    }
    printf("%s%s%s\n", esc(game->winner >= 0 ? "\x1b[92m" : "\x1b[91m"),
        game->winner >= 0 ? "문이 열렸습니다! 탈출 성공." : "문이 잠겼습니다. 열쇠를 가지고 ()로 이동하세요.", esc("\x1b[0m"));
    puts("Commands: UP=0 (상), DOWN=1 (하), LEFT=2 (좌), RIGHT=3 (우)");
    puts("## 벽 | @- 열쇠 | () 탈출구 | +N 여러 명이 있는 칸");
    for (i = 0; i < PLAYER_COUNT; ++i)
        printf("%sP%d%s=(%d,%d)%s  ", esc(player_colors[i]), i, esc("\x1b[0m"), game->players[i].row, game->players[i].col, game->key_owner == i ? " [KEY]" : "");
    printf("\n%s\n", message);
    fflush(stdout);
}
