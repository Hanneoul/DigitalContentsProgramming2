#include "player_api.h"

/* Simple example only: pick up an adjacent key, attack an adjacent opponent,
   otherwise try a rotated direction. Improve this ONE function for your team. */
Command player5_choose_command(const char *state_path)
{
    Snapshot s;
    FILE *file = fopen(state_path, "r");
    const int dx[4] = {0, 0, -1, 1}, dy[4] = {-1, 1, 0, 0};
    int d, i, j, x, y, ok;
    if (!file) return COMMAND_UP;
    ok = snapshot_read(file, &s);
    fclose(file); /* Also close the file on malformed input. */
    if (!ok) return COMMAND_UP;
    for (d = 0; d < 4; ++d) {
        x = s.players[s.self].pos.x + dx[d];
        y = s.players[s.self].pos.y + dy[d];
        if (s.key_owner == -1 && x == s.key.x && y == s.key.y)
            return (Command)(COMMAND_PICK_UP + d);
    }
    /* Only consider combat after checking every direction for a key. */
    for (d = 0; d < 4; ++d) {
        x = s.players[s.self].pos.x + dx[d];
        y = s.players[s.self].pos.y + dy[d];
        for (j = 0; j < PLAYER_COUNT; ++j)
            if (j != s.self && s.players[j].hp > 0 && s.players[j].pos.x == x && s.players[j].pos.y == y)
                return (Command)(COMMAND_ATTACK_UP + d);
    }
    /* No pathfinding: demonstrate loops, arrays, bounds and enum return values. */
    for (i = 0; i < 4; ++i) {
        d = (s.turn + s.self + i) % 4;
        x = s.players[s.self].pos.x + dx[d];
        y = s.players[s.self].pos.y + dy[d];
        if (x < 0 || y < 0 || x >= s.size || y >= s.size || s.terrain[y][x] == '#') continue;
        if (s.terrain[y][x] == 'E' && s.key_owner != s.self) continue;
        return (Command)d;
    }
    return COMMAND_ATTACK_UP; /* Empty-space attack is legal when surrounded. */
}
