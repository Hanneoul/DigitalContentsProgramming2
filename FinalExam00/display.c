#include <windows.h>
#include <inttypes.h>
#include "game.h"

static int color;
static const char *colors[PLAYER_COUNT] = {"\x1b[91m", "\x1b[92m", "\x1b[94m", "\x1b[95m", "\x1b[96m", "\x1b[97m"};
static const char *roles[PLAYER_COUNT] = {"Evader", "Attacker", "Blocker", "Survivor", "Snatcher", "Balanced"};
/* Redirected output and --plain must not contain terminal escape codes. */
static const char *esc(const char *sequence) { return color ? sequence : ""; }

/* The sidebar is drawn alongside the map, including on maps shorter than nine rows. */
static void scoreboard_row(const Game *g, int row)
{
    int place = row - 3;
    if (row == 0) printf("    +-------- SCOREBOARD --------+");
    else if (row == 1) printf("    %-28s", g->draw ? "GAME OVER - DRAW" : g->game_over ? "GAME OVER" : "ESCAPE ORDER");
    else if (row == 2) printf("    RANK  PLAYER  RESULT");
    else if (place >= 0 && place < PLAYER_COUNT) {
        if (g->draw) { printf("    --    %sP%d%s      DRAW (TIED)", esc(colors[place]), place, esc("\x1b[0m")); return; }
        if (place < g->finished_count) {
            int id = g->finish_order[place];
            printf("    %2d    %sP%d%s      ", place + 1, esc(colors[id]), id, esc("\x1b[0m"));
            if (place == PLAYER_COUNT - 1) printf("LAST");
            else printf("ESCAPED T%d", g->finish_turn[place]);
        } else printf("    %2d    --      RACING", place + 1);
    }
}

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
                if (g->players[i].rank) continue;
                if (g->players[i].pos.x == x && g->players[i].pos.y == y) {
                    if (g->players[i].hp > 0) occupant = i;
                    else if (effects && g->damaged[i]) fallen = 1;
                }
            }
            if (g->terrain[y][x] == '#') printf("%s##%s", esc("\x1b[100;90m"), esc("\x1b[0m"));
            else if (occupant >= 0) {
                /* Bright yellow / black keeps the carrier ID legible in every frame. */
                const char *paint = g->key_owner == occupant ? "\x1b[103;30;1m" :
                    effects && g->damaged[occupant] ? "\x1b[41;97;1m" :
                    effects && g->attack[y][x] ? "\x1b[43;30m" : colors[occupant];
                printf("%s%c%d%s", esc(paint), 'P',
                    occupant, esc("\x1b[0m"));
            }
            else if (fallen) printf("%s!!%s", esc("\x1b[41;97;1m"), esc("\x1b[0m"));
            else {
                const char *token = "  ";
                const char *paint = "";
                if (g->terrain[y][x] == 'E') { token = "()"; paint = g->last_escape >= 0 ? "\x1b[92m" : "\x1b[91m"; }
                else if (g->key_owner < 0 && g->key.x == x && g->key.y == y) { token = "@-"; paint = "\x1b[93m"; }
                if (effects && g->attack[y][x]) { paint = "\x1b[43;30m"; if (token[0] == ' ') token = "**"; }
                printf("%s%s%s", esc(paint), token, esc("\x1b[0m"));
            }
        }
        scoreboard_row(g, y);
        putchar('\n');
    }
    for (y = g->size; y < PLAYER_COUNT + 3; ++y) {
        printf("%*s", g->size * 2, ""); scoreboard_row(g, y); putchar('\n');
    }
    printf("%s%s%s\n", esc(g->last_escape >= 0 ? "\x1b[92m" : "\x1b[91m"),
        g->last_escape >= 0 ? "탈출 순위 기록! 열쇠는 최초 위치에 다시 생성됩니다." : "문이 잠겼습니다. 열쇠 소유자만 출입 가능합니다.", esc("\x1b[0m"));
    puts("P0~P5 자동 경쟁 | 이동 0~3 / 공격 4~7 / 집기 8~11 / 버리기 12~15 | Ctrl+C 종료");
    puts("열쇠 소지자: 밝은 노란 배경 / 검은 글자 | 소지 중 공격 시 취소 및 HP -1");
    for (i = 0; i < PLAYER_COUNT; ++i) {
        const PlayerState *p = &g->players[i];
        if (p->rank) {
            printf("%sP%d%s : %d위 %s\n", esc(colors[i]), i, esc("\x1b[0m"), p->rank,
                p->rank == PLAYER_COUNT ? "[LAST]" : "[ESCAPED]");
            continue;
        }
        printf("%s%c%d%s [%s] (%d,%d) HP:%d/2 사망:%u 휴식:%" PRIu64 " 명령:%d%s%s%s\n",
            esc(g->key_owner == i ? "\x1b[103;30;1m" : effects && g->damaged[i] ? "\x1b[41;97m" : colors[i]), 'P',
            i, esc("\x1b[0m"), roles[i], p->pos.x, p->pos.y, p->hp, p->deaths, p->rest,
            (int)g->last[i], g->key_owner == i ? " [KEY]" : "", g->cancelled[i] ? " [취소]" : "", g->damaged[i] ? " [피격 -1]" : "");
    }
    printf("%s\n", message);
    fflush(stdout);
}
