#include "player_api.h"

/* HUNTER: chase the nearest reachable opponent and attack its three-cell fan.
   Each call reads only this turn's file. BFS avoids walls, locked exits and live
   occupants. No persistent/global memory or access to another player's code. */
Command player1_choose_command(const char *state_path)
{
    Snapshot s;
    FILE *file = fopen(state_path, "r");
    const int dx[4] = {0, 0, -1, 1}, dy[4] = {-1, 1, 0, 0};
    Position queue[MAP_MAX * MAP_MAX];
    int first[MAP_MAX][MAP_MAX], occupied[MAP_MAX][MAP_MAX] = {0};
    int head = 0, tail = 0, x, y, i, d, ok, carrying;
    Position me;
    if (!file) return COMMAND_ATTACK_UP;
    ok = snapshot_read(file, &s);
    fclose(file);
    if (!ok) return COMMAND_ATTACK_UP;
    me = s.players[s.self].pos;
    carrying = s.key_owner == s.self;
    for (y = 0; y < s.size; ++y)
        for (x = 0; x < s.size; ++x) first[y][x] = -2;
    for (i = 0; i < PLAYER_COUNT; ++i)
        if (i != s.self && s.players[i].hp > 0)
            occupied[s.players[i].pos.y][s.players[i].pos.x] = 1;
    /* Hunters do not seek keys. If given one, discard it before fighting. */
    if (carrying) {
        for (d = 0; d < 4; ++d) {
            x = me.x + dx[d]; y = me.y + dy[d];
            if (x >= 0 && y >= 0 && x < s.size && y < s.size && !occupied[y][x] && s.terrain[y][x] == ' ')
                return (Command)(COMMAND_DROP_UP + d);
        }
        return COMMAND_UP;
    }
    queue[tail++] = me;
    first[me.y][me.x] = -1;
    /* BFS visits candidate action positions in increasing walking distance. */
    while (head < tail) {
        Position at = queue[head++];
        int action = -1;
        /* A target is in range if it is one cardinal/diagonal cell away.
           For diagonals, a vertical attack fan includes that cell. */
        for (i = 0; i < PLAYER_COUNT; ++i) {
            int ax, ay;
            if (i == s.self || s.players[i].hp <= 0) continue;
            
            ax = s.players[i].pos.x - at.x;
            ay = s.players[i].pos.y - at.y;
            if (abs(ax) <= 1 && abs(ay) <= 1 && (ax || ay)) {
                action = ay < 0 ? COMMAND_ATTACK_UP : ay > 0 ? COMMAND_ATTACK_DOWN :
                    ax < 0 ? COMMAND_ATTACK_LEFT : COMMAND_ATTACK_RIGHT;
                break;
            }
        }
        if (action >= 0) {
            if (first[at.y][at.x] >= 0) return (Command)first[at.y][at.x];
            return (Command)action;
        }
        for (i = 0; i < 4; ++i) {
            /* Rotate equal-length choices without random/global state. */
            d = (i + s.turn + s.self) % 4;
            x = at.x + dx[d]; y = at.y + dy[d];
            if (x < 0 || y < 0 || x >= s.size || y >= s.size || first[y][x] != -2 || occupied[y][x]) continue;
            if (s.terrain[y][x] == '#' || (s.terrain[y][x] == 'E' && !carrying)) continue;
            first[y][x] = first[at.y][at.x] < 0 ? d : first[at.y][at.x];
            queue[tail].x = x; queue[tail++].y = y;
        }
    }
    /* No WAIT command exists in this core. Never issue a forbidden attack with
       a key: put it on an empty adjacent floor if completely trapped. */
    if (carrying) {
        for (d = 0; d < 4; ++d) {
            x = me.x + dx[d]; y = me.y + dy[d];
            if (x >= 0 && y >= 0 && x < s.size && y < s.size && !occupied[y][x] && s.terrain[y][x] == ' ')
                return (Command)(COMMAND_DROP_UP + d);
        }
        return COMMAND_UP; /* Completely blocked: no legal non-damaging command. */
    }
    return COMMAND_ATTACK_UP; /* Legal empty-space attack if no target is reachable. */
}
