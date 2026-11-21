#ifndef FINAL_EXAM_SAMPLE_AI_H
#define FINAL_EXAM_SAMPLE_AI_H
#include "player_api.h"

/* Shared sample implementation only. Each playerN.c selects its own personality;
   students may replace that function without seeing other teams' player sources. */
typedef enum SamplePersonality {
    SAMPLE_EVADE, SAMPLE_ATTACK, SAMPLE_BLOCK, SAMPLE_SURVIVE,
    SAMPLE_SNATCH, SAMPLE_BALANCED
} SamplePersonality;

/* Every role has the SAME final objective: acquire the key, then reach the exit.
   Nonnegative danger-weighted Dijkstra costs produce wall-aware cardinal paths. */
static inline Command sample_ai_choose(const char *path, SamplePersonality role)
{
    Snapshot s;
    FILE *file = fopen(path, "r");
    const int dx[4] = {0, 0, -1, 1}, dy[4] = {-1, 1, 0, 0};
    int cost[MAP_MAX][MAP_MAX], first[MAP_MAX][MAP_MAX];
    unsigned char occupied[MAP_MAX][MAP_MAX] = {0};
    unsigned char danger[MAP_MAX][MAP_MAX] = {0}, visited[MAP_MAX][MAP_MAX] = {0};
    Position me;
    int i, x, y, d, ok, carrying, mode, risk_weight, best_attack = -1;
    int attack_score = -100000, safe_attack = -1;
    if (!file) return COMMAND_ATTACK_UP;
    ok = snapshot_read(file, &s);
    fclose(file);
    if (!ok) return COMMAND_ATTACK_UP;
    me = s.players[s.self].pos;
    carrying = s.key_owner == s.self;
    /* Mode 0: pickup adjacency, 1: exit, 2: carrier, 3: any rival, 4: block exit. */
    mode = carrying ? 1 : s.key_owner < 0 ? 0 : 2;
    risk_weight = role == SAMPLE_EVADE ? 8 : role == SAMPLE_SURVIVE ? 12 :
        role == SAMPLE_SNATCH ? 1 : role == SAMPLE_ATTACK ? 0 : 3;
    for (i = 0; i < PLAYER_COUNT; ++i) {
        Position enemy = s.players[i].pos;
        if (i == s.self || s.players[i].hp <= 0 || s.players[i].rank) continue;
        occupied[enemy.y][enemy.x] = 1;
        for (y = 0; y < s.size; ++y) for (x = 0; x < s.size; ++x) {
            int ax = abs(x - enemy.x), ay = abs(y - enemy.y);
            if (ax <= 1 && ay <= 1) danger[y][x] += 3;
            else if (ax + ay <= 3) ++danger[y][x];
        }
    }
    /* All personalities immediately collect an adjacent free key, even attackers. */
    if (!carrying && s.key_owner < 0)
        for (d = 0; d < 4; ++d)
            if (me.x + dx[d] == s.key.x && me.y + dy[d] == s.key.y)
                return (Command)(COMMAND_PICK_UP + d);
    /* Score the actual three-cell attack fans; also find a harmless stand-still
       command because this core intentionally has no separate WAIT command. */
    for (d = 0; d < 4; ++d) {
        int score = 0, targets = 0;
        for (i = 0; i < PLAYER_COUNT; ++i) {
            int ax, ay, hit;
            if (i == s.self || s.players[i].hp <= 0 || s.players[i].rank) continue;
            ax = s.players[i].pos.x - me.x; ay = s.players[i].pos.y - me.y;
            hit = d < 2 ? ay == dy[d] && abs(ax) <= 1 : ax == dx[d] && abs(ay) <= 1;
            if (hit) { score += 10 + (s.key_owner == i ? 40 : 0) + (s.players[i].hp == 1 ? 10 : 0); ++targets; }
        }
        if (!targets && safe_attack < 0) safe_attack = d;
        if (targets && score > attack_score) { attack_score = score; best_attack = d; }
    }
    if (!carrying) {
        /* Aggression is bounded: two combat turns, then three objective turns. */
        if (role == SAMPLE_ATTACK && (s.turn + s.self) % 5 < 2) mode = 3;
        if (role == SAMPLE_BALANCED && s.turn % 7 == 0) mode = 3;
        /* Blockers temporarily camp a doorway, then return to acquiring the key. */
        if (role == SAMPLE_BLOCK && s.key_owner >= 0 && s.turn % 6 < 2) mode = 4;
        if (best_attack >= 0 && (role == SAMPLE_ATTACK || role == SAMPLE_BLOCK ||
            (role == SAMPLE_BALANCED && s.players[s.self].hp == MAX_HP) ||
            (role == SAMPLE_SNATCH && attack_score >= 50)))
            return (Command)(COMMAND_ATTACK_UP + best_attack);
        /* Survival/evade personalities occasionally step out of immediate danger,
           but never spend every turn fleeing instead of pursuing the objective. */
        if ((role == SAMPLE_SURVIVE || role == SAMPLE_EVADE) &&
            s.players[s.self].hp == 1 && danger[me.y][me.x] >= 3 && s.turn % 3 == 0) {
            int best = danger[me.y][me.x], move = -1;
            for (d = 0; d < 4; ++d) {
                x = me.x + dx[d]; y = me.y + dy[d];
                if (x < 0 || y < 0 || x >= s.size || y >= s.size || occupied[y][x] || s.terrain[y][x] != ' ') continue;
                if (danger[y][x] < best) { best = danger[y][x]; move = d; }
            }
            if (move >= 0) return (Command)move;
        }
    }
    for (y = 0; y < s.size; ++y) for (x = 0; x < s.size; ++x) {
        cost[y][x] = 100000000; first[y][x] = -1;
    }
    cost[me.y][me.x] = 0;
    for (;;) {
        int bx = -1, by = -1, minimum = 100000000, action = -1;
        for (y = 0; y < s.size; ++y) for (x = 0; x < s.size; ++x)
            if (!visited[y][x] && cost[y][x] < minimum) { bx = x; by = y; minimum = cost[y][x]; }
        if (bx < 0) break;
        visited[by][bx] = 1;
        if (mode == 1) {
            if (bx == s.exit.x && by == s.exit.y) action = COMMAND_UP;
        } else if (mode == 0) {
            for (d = 0; d < 4; ++d)
                if (bx + dx[d] == s.key.x && by + dy[d] == s.key.y) action = COMMAND_PICK_UP + d;
        } else if (mode == 4) {
            if (abs(bx - s.exit.x) + abs(by - s.exit.y) == 1)
                action = COMMAND_ATTACK_UP + (best_attack >= 0 ? best_attack : safe_attack >= 0 ? safe_attack : 0);
        } else {
            for (i = 0; i < PLAYER_COUNT; ++i) {
                int ax, ay;
                if (i == s.self || s.players[i].hp <= 0 || s.players[i].rank || (mode == 2 && i != s.key_owner)) continue;
                ax = s.players[i].pos.x - bx; ay = s.players[i].pos.y - by;
                if (abs(ax) <= 1 && abs(ay) <= 1 && (ax || ay)) {
                    action = ay < 0 ? COMMAND_ATTACK_UP : ay > 0 ? COMMAND_ATTACK_DOWN :
                        ax < 0 ? COMMAND_ATTACK_LEFT : COMMAND_ATTACK_RIGHT;
                    break;
                }
            }
        }
        if (action >= 0) return first[by][bx] >= 0 ? (Command)first[by][bx] : (Command)action;
        for (i = 0; i < 4; ++i) {
            int next;
            d = (i + s.self + s.turn) % 4;
            x = bx + dx[d]; y = by + dy[d];
            if (x < 0 || y < 0 || x >= s.size || y >= s.size || occupied[y][x] || visited[y][x]) continue;
            if (s.terrain[y][x] == '#' || (s.terrain[y][x] == 'E' && !carrying)) continue;
            next = minimum + 10 + risk_weight * danger[y][x];
            if (next < cost[y][x]) { cost[y][x] = next; first[y][x] = first[by][bx] < 0 ? d : first[by][bx]; }
        }
    }
    /* A blocked carrier drops to a legal floor rather than attacking for -1 HP. */
    if (carrying) {
        for (d = 0; d < 4; ++d) {
            x = me.x + dx[d]; y = me.y + dy[d];
            if (x >= 0 && y >= 0 && x < s.size && y < s.size && !occupied[y][x] && s.terrain[y][x] == ' ')
                return (Command)(COMMAND_DROP_UP + d);
        }
        return COMMAND_UP;
    }
    return (Command)(COMMAND_ATTACK_UP + (safe_attack >= 0 ? safe_attack : best_attack >= 0 ? best_attack : 0));
}
#endif
