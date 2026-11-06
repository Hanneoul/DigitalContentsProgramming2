#include "game.h"

static int fail(char *error, size_t capacity, const char *message)
{
    snprintf(error, capacity, "%s", message);
    return 0;
}

int game_load(Game *game, FILE *file, char *error, size_t capacity)
{
    char line[MAP_MAX * 2 + 4];
    int row = 0, width = -1, starts[PLAYER_COUNT] = {0}, keys = 0, exits = 0;
    int i, col, seen[MAP_MAX][MAP_MAX] = {0};
    Position queue[MAP_MAX * MAP_MAX];
    int head = 0, tail = 0;
    memset(game, 0, sizeof(*game));
    game->key_owner = game->winner = -1;
    while (fgets(line, sizeof(line), file)) {
        size_t length = strlen(line);
        if (length && line[length - 1] == '\n') line[--length] = '\0';
        if (length && line[length - 1] == '\r') line[--length] = '\0';
        if (!length || length % 2 || length > MAP_MAX * 2 || row >= MAP_MAX)
            return fail(error, capacity, "Map must have 2 ASCII characters per cell, size 3..32.");
        if (width < 0) width = (int)length / 2;
        if ((int)length != width * 2) return fail(error, capacity, "Map rows have different widths.");
        for (col = 0; col < width; ++col) {
            char a = line[col * 2], b = line[col * 2 + 1];
            Position pos = {row, col};
            game->terrain[row][col] = ' ';
            if (a == '#' && b == '#') game->terrain[row][col] = '#';
            else if (a == ' ' && b == ' ') { /* floor */ }
            else if (a == '@' && b == '-') { game->key = pos; ++keys; }
            else if (a == '(' && b == ')') { game->exit = pos; game->terrain[row][col] = 'E'; ++exits; }
            else if (a == 'P' && b >= '0' && b <= '9') {
                i = b - '0';
                if (i < PLAYER_COUNT) { game->players[i] = pos; ++starts[i]; }
            }
            else return fail(error, capacity, "Unknown map token (UTF-8 BOM and tabs are not allowed).");
        }
        ++row;
    }
    if (ferror(file)) return fail(error, capacity, "Map read failed.");
    if (width < 3 || row != width) return fail(error, capacity, "Map must be square (3..32 cells).");
    if (keys != 1 || exits != 1) return fail(error, capacity, "Map needs exactly one key and one exit.");
    for (i = 0; i < PLAYER_COUNT; ++i)
        if (starts[i] != 1) return fail(error, capacity, "Map needs exactly one of each P0..P5.");
    game->size = width;
    /* Every starting position and the exit must be reachable from the key. */
    queue[tail++] = game->key;
    seen[game->key.row][game->key.col] = 1;
    while (head < tail) {
        static const int dr[4] = {-1, 1, 0, 0}, dc[4] = {0, 0, -1, 1};
        Position p = queue[head++];
        for (i = 0; i < 4; ++i) {
            Position next = {p.row + dr[i], p.col + dc[i]};
            if (next.row < 0 || next.col < 0 || next.row >= width || next.col >= width) continue;
            if (seen[next.row][next.col] || game->terrain[next.row][next.col] == '#') continue;
            seen[next.row][next.col] = 1;
            queue[tail++] = next;
        }
    }
    if (!seen[game->exit.row][game->exit.col]) return fail(error, capacity, "Exit cannot be reached from the key.");
    for (i = 0; i < PLAYER_COUNT; ++i)
        if (!seen[game->players[i].row][game->players[i].col]) return fail(error, capacity, "A player cannot reach the key.");
    if (capacity) error[0] = '\0';
    return 1;
}

int game_move(Game *game, int player, Command command)
{
    static const int dr[4] = {-1, 1, 0, 0}, dc[4] = {0, 0, -1, 1};
    Position next;
    if (player < 0 || player >= PLAYER_COUNT || (int)command < 0 || command > COMMAND_RIGHT || game->winner >= 0) return 0;
    next = game->players[player];
    next.row += dr[command]; next.col += dc[command];
    if (next.row < 0 || next.col < 0 || next.row >= game->size || next.col >= game->size || game->terrain[next.row][next.col] == '#') return 0;
    game->players[player] = next;
    if (game->key_owner < 0 && next.row == game->key.row && next.col == game->key.col) game->key_owner = player;
    if (game->key_owner == player && next.row == game->exit.row && next.col == game->exit.col) game->winner = player;
    return 1;
}
