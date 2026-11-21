#include <windows.h>
#include <inttypes.h>
#include "game.h"

static int color;
static const char *colors[PLAYER_COUNT] = {"\x1b[91m", "\x1b[92m", "\x1b[94m", "\x1b[95m", "\x1b[96m", "\x1b[97m"};
static const char *dark_colors[PLAYER_COUNT] = {"\x1b[31m", "\x1b[32m", "\x1b[34m", "\x1b[35m", "\x1b[36m", "\x1b[37m"};
/* Redirected output and --plain must not contain terminal escape codes. */
static const char *esc(const char *sequence) { return color ? sequence : ""; }

/* Keep special highlights readable; both their normal/bright variants reflect HP.
   An attack fan colors empty cells, without hiding a player's own HP color. */
const char *display_player_style(int player, int hp, int carrier, int hit)
{
    if (carrier) return hp >= MAX_HP ? "\x1b[103;30;1m" : "\x1b[43;30m";
    if (hit) return hp >= MAX_HP ? "\x1b[101;97;1m" : "\x1b[41;37m";
    if (player < 0 || player >= PLAYER_COUNT || hp <= 0) return "\x1b[90m";
    return hp >= MAX_HP ? colors[player] : dark_colors[player];
}

/* Decode UTF-8, remove terminal controls and pad by screen cells, not byte length.
   Hangul/CJK occupy two cells. Other unusual glyphs use '?' to keep alignment. */
int display_name(char *out, size_t capacity, const char *name, int columns)
{
    char source[PLAYER_NAME_BYTES];
    WCHAR wide[PLAYER_NAME_BYTES];
    size_t n = 0, used = 0;
    int i, count, cells = 0;
    if (!capacity || columns < 0) return 0;
    while (n + 1 < sizeof(source) && name[n]) { source[n] = name[n]; ++n; }
    source[n] = '\0';
    count = MultiByteToWideChar(CP_UTF8, 0, source, -1, wide, PLAYER_NAME_BYTES);
    for (i = 0; i < count - 1; ++i) {
        WCHAR ch = wide[i];
        char bytes[4];
        int width = 1, length;
        if (ch < 32 || (ch >= 127 && ch < 160)) continue;
        if ((ch >= 0x1100 && ch <= 0x115f) || (ch >= 0x2e80 && ch <= 0xa4cf) ||
            (ch >= 0xac00 && ch <= 0xd7a3) || (ch >= 0xf900 && ch <= 0xfaff) ||
            (ch >= 0xff01 && ch <= 0xff60)) width = 2;
        else if (ch > 126) {
            if (ch >= 0xd800 && ch <= 0xdbff && i + 1 < count - 1 && wide[i + 1] >= 0xdc00 && wide[i + 1] <= 0xdfff) ++i;
            ch = '?';
        }
        if (cells + width > columns) break;
        length = WideCharToMultiByte(CP_UTF8, 0, &ch, 1, bytes, sizeof(bytes), NULL, NULL);
        if (length <= 0 || used + (size_t)length >= capacity) break;
        memcpy(out + used, bytes, (size_t)length); used += (size_t)length; cells += width;
    }
    while (cells < columns && used + 1 < capacity) { out[used++] = ' '; ++cells; }
    out[used] = '\0';
    return cells;
}

/* ceil(remaining * width / total), avoiding 64-bit overflow at long respawn times. */
int display_gauge_fill(uint64_t remaining, uint64_t total, int width)
{
    int fill = 0, i;
    if (!total || !remaining || width <= 0) return 0;
    if (remaining >= total) return width;
    for (i = 0; i < width; ++i) {
        uint64_t threshold = (total / (uint64_t)width) * (uint64_t)i +
            ((total % (uint64_t)width) * (uint64_t)i) / (uint64_t)width;
        if (remaining > threshold) ++fill;
    }
    return fill;
}

/* Reserve 48 cells for ID, name and ranking; large maps use the bottom panel. */
int display_sidebar_fits(int map_size, int console_columns)
{
    return map_size * 2 + 48 <= console_columns;
}

/* Recheck the visible console width on each frame so resizing changes the layout. */
static int console_width(void)
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    return GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info) ?
        info.srWindow.Right - info.srWindow.Left + 1 : 80;
}

/* Render a fixed-width underscore gauge with filled background and a plain fallback. */
static void respawn_bar(const PlayerState *p)
{
    uint64_t total = !p->deaths ? 0 : p->deaths <= 64 ? UINT64_C(1) << (p->deaths - 1) : UINT64_MAX;
    int i, fill = p->hp > 0 || p->rank ? 0 : display_gauge_fill(p->rest, total, 13);
    putchar('[');
    for (i = 0; i < 13; ++i) {
        if (color) printf("%s_%s", i < fill ? "\x1b[46;30m" : "\x1b[90m", "\x1b[0m");
        else putchar(i < fill ? '=' : '_');
    }
    putchar(']');
}

/* The sidebar is drawn alongside the map, including on maps shorter than nine rows. */
static void scoreboard_row(const Game *g, int row)
{
    int place = row - 3;
    if (row == 0) printf("    +-------- SCOREBOARD --------+");
    else if (row == 1) printf("    %-28s", g->draw ? "GAME OVER - DRAW" : g->game_over ? "GAME OVER" : "ESCAPE ORDER");
    else if (row == 2) printf("    RANK  PLAYER  RESULT");
    else if (place >= 0 && place < PLAYER_COUNT) {
        char name[PLAYER_NAME_BYTES];
        if (g->draw) { display_name(name, sizeof(name), player_name(place), 16); printf("    -- P%d %s DRAW (TIED)", place, name); return; }
        if (place < g->finished_count) {
            int id = g->finish_order[place];
            display_name(name, sizeof(name), player_name(id), 16);
            printf("    %2d %sP%d%s %s ", place + 1, esc(colors[id]), id, esc("\x1b[0m"), name);
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

/* One scheduled frame shows movement, attack fans and hits together. Effects last
   until the next scheduled turn; there is no extra immediate effect-clearing frame. */
void display_game(const Game *g, const char *message, int effects)
{
    int y, x, i, side = display_sidebar_fits(g->size, console_width());
    int rest_width = 4, deaths_width = 3;
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
                const char *paint = display_player_style(occupant, g->players[occupant].hp,
                    g->key_owner == occupant, effects && g->damaged[occupant]);
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
        if (side) scoreboard_row(g, y);
        putchar('\n');
    }
    for (y = g->size; side && y < PLAYER_COUNT + 3; ++y) {
        printf("%*s", g->size * 2, ""); scoreboard_row(g, y); putchar('\n');
    }
    printf("%s%s%s\n", esc(g->last_escape >= 0 ? "\x1b[92m" : "\x1b[91m"),
        g->last_escape >= 0 ? "탈출 순위 기록! 열쇠는 최초 위치에 다시 생성됩니다." : "문이 잠겼습니다. 열쇠 소유자만 출입 가능합니다.", esc("\x1b[0m"));
    puts("P0~P5 자동 경쟁 | 이동 0~3 / 공격 4~7 / 집기 8~11 / 버리기 12~15 | Ctrl+C 종료");
    puts("HP 2: 밝은 색 / HP 1: 어두운 색 | 열쇠: 노란 배경 | 소지 중 공격 시 취소 및 HP -1");
    /* Number columns grow together if late-game respawn/death counters grow. */
    for (i = 0; i < PLAYER_COUNT; ++i) {
        char number[32];
        int width = snprintf(number, sizeof(number), "%" PRIu64, g->players[i].rest);
        if (width > rest_width) rest_width = width;
        width = snprintf(number, sizeof(number), "%u", g->players[i].deaths);
        if (width > deaths_width) deaths_width = width;
    }
    puts("PLAYER / NAME         HP       RESPAWN (remaining turns)     DEATH / POSITION");
    for (i = 0; i < PLAYER_COUNT; ++i) {
        const PlayerState *p = &g->players[i];
        char name[PLAYER_NAME_BYTES];
        int hp;
        display_name(name, sizeof(name), player_name(i), 16);
        printf("%sP%d %s%s HP ", esc(display_player_style(i, p->hp, g->key_owner == i, 0)), i, name, esc("\x1b[0m"));
        for (hp = 0; hp < MAX_HP; ++hp)
            printf("%s%s%s", esc(effects && g->damaged[i] ? "\x1b[41;97m" : hp < p->hp ? "\x1b[42;30m" : "\x1b[90m"), hp < p->hp ? "[#]" : "[ ]", esc("\x1b[0m"));
        printf(" R "); respawn_bar(p);
        printf(" %*" PRIu64 "T D:%*u (%2d,%2d) C:%2d", rest_width, p->rest, deaths_width, p->deaths, p->pos.x, p->pos.y, (int)g->last[i]);
        if (p->rank) printf(" [%d위 %s]", p->rank, p->rank == PLAYER_COUNT ? "LAST" : "ESCAPED");
        else if (p->hp == 0 && p->rest == 0) printf(" [부활 대기]");
        if (g->key_owner == i) printf(" [KEY]");
        if (g->cancelled[i]) printf(" [취소]");
        if (g->damaged[i]) printf(" [피격 -1]");
        putchar('\n');
    }
    if (!side) for (y = 0; y < PLAYER_COUNT + 3; ++y) { scoreboard_row(g, y); putchar('\n'); }
    printf("%s\n", message);
    if (g->key_owner >= 0) {
        char name[PLAYER_NAME_BYTES];
        display_name(name, sizeof(name), player_name(g->key_owner), 16);
        printf("%sKEY: P%d %s 소유 중 | 복귀 타이머 정지%s\n", esc("\x1b[93m"), g->key_owner, name, esc("\x1b[0m"));
    } else printf("%sKEY: 바닥 (%d,%d) | 최초 위치 복귀까지 %2d / %d턴%s\n", esc("\x1b[93m"), g->key.x, g->key.y, game_key_remaining(g), KEY_RESPAWN_TURNS, esc("\x1b[0m"));
    fflush(stdout);
}
