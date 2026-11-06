#include <windows.h>
#include <inttypes.h>
#include "game.h"

static int color;
static const char *colors[PLAYER_COUNT] = {"\x1b[91m", "\x1b[92m", "\x1b[94m", "\x1b[95m", "\x1b[96m", "\x1b[97m", "\x1b[93m"};
/* Redirected output and --plain must not contain terminal escape codes. */
static const char *esc(const char *sequence) { return color ? sequence : ""; }

/* Enable ANSI processing only when the output handle is an interactive console. */
void display_init(int plain)
{
    DWORD mode;
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleOutputCP(CP_UTF8);
    color = !plain && GetConsoleMode(output, &mode) && SetConsoleMode(output, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

/* Draw each logical cell using exactly two printable characters, plus zero-width ANSI.
   The effect frame paints attack fans and hit players; the final frame clears effects. */
void display_game(const Game *g, const char *message, int effects)
{
    int y, x, i;
    printf("%sDungeon Escape | Turn %d | x=column, y=row\n", esc("\x1b[2J\x1b[H"), g->turn);
    for (y = 0; y < g->size; ++y) {
        for (x = 0; x < g->size; ++x) {
            int occupant = -1, fallen = 0;
            for (i = 0; i < PLAYER_COUNT; ++i) {
                if (g->players[i].pos.x == x && g->players[i].pos.y == y) {
                    if (g->players[i].hp > 0) occupant = i;
                    else if (effects && g->damaged[i]) fallen = 1;
                }
            }
            if (g->terrain[y][x] == '#') printf("%s##%s", esc("\x1b[100;90m"), esc("\x1b[0m"));
            else if (occupant >= 0) {
                const char *paint = effects && g->damaged[occupant] ? "\x1b[41;97;1m" :
                    effects && g->attack[y][x] ? "\x1b[43;30m" : colors[occupant];
                printf("%s%c%d%s", esc(paint), occupant == HUMAN_PLAYER ? 'H' : 'P',
                    occupant == HUMAN_PLAYER ? 0 : occupant, esc("\x1b[0m"));
            }
            else if (fallen) printf("%s!!%s", esc("\x1b[41;97;1m"), esc("\x1b[0m"));
            else {
                const char *token = "  ";
                const char *paint = "";
                if (g->terrain[y][x] == 'E') { token = "()"; paint = g->winner >= 0 ? "\x1b[92m" : "\x1b[91m"; }
                else if (g->key_owner < 0 && g->key.x == x && g->key.y == y) { token = "@-"; paint = "\x1b[93m"; }
                if (effects && g->attack[y][x]) { paint = "\x1b[43;30m"; if (token[0] == ' ') token = "**"; }
                printf("%s%s%s", esc(paint), token, esc("\x1b[0m"));
            }
        }
        putchar('\n');
    }
    printf("%s%s%s\n", esc(g->winner >= 0 ? "\x1b[92m" : "\x1b[91m"),
        g->winner >= 0 ? "문이 열렸습니다! 탈출 성공." : "문이 잠겼습니다. 열쇠 소유자만 출입 가능합니다.", esc("\x1b[0m"));
    puts("방향 순서: 상/하/좌/우 | 이동 0~3 wsad | 공격 4~7 tgfh | 집기 8~11 / 버리기 12~15 ikjl | ESC 종료");
    for (i = 0; i < PLAYER_COUNT; ++i) {
        const PlayerState *p = &g->players[i];
        printf("%s%c%d%s (%d,%d) HP:%d/2 사망:%u 휴식:%" PRIu64 " 명령:%d%s%s%s\n",
            esc(effects && g->damaged[i] ? "\x1b[41;97m" : colors[i]), i == HUMAN_PLAYER ? 'H' : 'P',
            i == HUMAN_PLAYER ? 0 : i, esc("\x1b[0m"), p->pos.x, p->pos.y, p->hp, p->deaths, p->rest,
            (int)g->last[i], g->key_owner == i ? " [KEY]" : "", g->cancelled[i] ? " [취소]" : "", g->damaged[i] ? " [피격 -1]" : "");
    }
    printf("%s\n", message);
    fflush(stdout);
}
