#include "game.h"

/* Return a readable validation error without exposing a partially loaded map. */
static int fail(char *error, size_t capacity, const char *message)
{
    snprintf(error, capacity, "%s", message);
    return 0;
}

/* Coordinates always refer to logical cells, never console character offsets. */
static int inside(const Game *g, Position p)
{
    return p.x >= 0 && p.y >= 0 && p.x < g->size && p.y < g->size;
}

/* Compare logical positions for claims, occupancy and item ownership. */
static int same(Position a, Position b) { return a.x == b.x && a.y == b.y; }

/* Decode the strict two-character map and validate size, spawns and reachability. */
int game_load(Game *g, FILE *file, char *error, size_t capacity)
{
    char line[MAP_MAX * 2 + 4];
    int row = 0, width = -1, starts[PLAYER_COUNT] = {0}, keys = 0, exits = 0;
    int i, col, seen[MAP_MAX][MAP_MAX] = {0};
    Position queue[MAP_MAX * MAP_MAX];
    int head = 0, tail = 0;
    memset(g, 0, sizeof(*g));
    g->key_owner = g->winner = -1;
    while (fgets(line, sizeof(line), file)) {
        size_t length = strlen(line);
        if (length && line[length - 1] == '\n') line[--length] = '\0';
        if (length && line[length - 1] == '\r') line[--length] = '\0';
        if (!length || length % 2 || length > MAP_MAX * 2 || row >= MAP_MAX)
            return fail(error, capacity, "Map requires two ASCII characters per cell; size 3..32.");
        if (width < 0) width = (int)length / 2;
        if ((int)length != width * 2) return fail(error, capacity, "Map rows have different widths.");
        for (col = 0; col < width; ++col) {
            char a = line[col * 2], b = line[col * 2 + 1];
            Position pos = {col, row};
            g->terrain[row][col] = ' ';
            if (a == '#' && b == '#') g->terrain[row][col] = '#';
            else if (a == ' ' && b == ' ') { /* Floor. */ }
            else if (a == '@' && b == '-') { g->key = g->key_start = pos; ++keys; }
            else if (a == '(' && b == ')') { g->exit = pos; g->terrain[row][col] = 'E'; ++exits; }
            else if (a == 'P' && b >= '0' && b <= '9') {
                i = b - '0';
                if (i < NPC_COUNT) { g->players[i].pos = g->players[i].spawn = pos; ++starts[i]; }
            }
            else if (a == 'H' && b == '0') {
                g->players[HUMAN_PLAYER].pos = g->players[HUMAN_PLAYER].spawn = pos;
                ++starts[HUMAN_PLAYER];
            }
            else return fail(error, capacity, "Unknown map token; no BOM or tabs allowed.");
        }
        ++row;
    }
    if (ferror(file)) return fail(error, capacity, "Map read failed.");
    if (width < 3 || row != width) return fail(error, capacity, "Map must be square (3..32 cells).");
    if (keys != 1 || exits != 1) return fail(error, capacity, "Map needs exactly one key and exit.");
    for (i = 0; i < PLAYER_COUNT; ++i) {
        if (starts[i] != 1) return fail(error, capacity, "Map needs one each of P0..P5 and H0.");
        g->players[i].hp = MAX_HP;
        g->last[i] = (Command)-1;
    }
    g->size = width;
    queue[tail++] = g->key;
    seen[g->key.y][g->key.x] = 1;
    /* The locked exit cannot be used as a bridge to reach the key. */
    while (head < tail) {
        static const int dx[4] = {0, 0, -1, 1}, dy[4] = {-1, 1, 0, 0};
        Position p = queue[head++];
        for (i = 0; i < 4; ++i) {
            Position next = {p.x + dx[i], p.y + dy[i]};
            if (!inside(g, next) || seen[next.y][next.x] || g->terrain[next.y][next.x] == '#') continue;
            seen[next.y][next.x] = 1;
            if (!same(next, g->exit)) queue[tail++] = next;
        }
    }
    if (!seen[g->exit.y][g->exit.x]) return fail(error, capacity, "Exit cannot be reached from key.");
    for (i = 0; i < PLAYER_COUNT; ++i)
        if (!seen[g->players[i].pos.y][g->players[i].pos.x]) return fail(error, capacity, "A spawn cannot reach key without crossing locked exit.");
    if (capacity) error[0] = '\0';
    return 1;
}

/* A death on turn T with rest=1 skips T+1 and respawns before input on T+2.
   Occupied spawn cells postpone respawn without causing another death. */
void game_begin_turn(Game *g)
{
    int i, j;
    ++g->turn;
    memset(g->attack, 0, sizeof(g->attack));
    memset(g->damaged, 0, sizeof(g->damaged));
    memset(g->cancelled, 0, sizeof(g->cancelled));
    for (i = 0; i < PLAYER_COUNT; ++i) {
        PlayerState *p = &g->players[i];
        g->last[i] = (Command)-1;
        if (p->hp > 0) continue;
        if (p->rest) { --p->rest; continue; }
        for (j = 0; j < PLAYER_COUNT; ++j)
            if (g->players[j].hp > 0 && same(g->players[j].pos, p->spawn)) break;
        if (j == PLAYER_COUNT) { p->pos = p->spawn; p->hp = MAX_HP; }
    }
}

/* All decisions use one frozen start state. Destination claims are resolved before
   applying movement, then attacks damage final positions. Damage is a boolean,
   so collision plus any number of attacks still costs at most one HP per turn. */
void game_resolve(Game *g, const Command commands[PLAYER_COUNT])
{
    static const int dx[4] = {0, 0, -1, 1}, dy[4] = {-1, 1, 0, 0};
    Position start[PLAYER_COUNT], target[PLAYER_COUNT];
    int active[PLAYER_COUNT], kind[PLAYER_COUNT], valid[PLAYER_COUNT];
    int i, j, k, changed;
    if (g->winner >= 0) return;
    for (i = 0; i < PLAYER_COUNT; ++i) {
        int command = (int)commands[i];
        start[i] = target[i] = g->players[i].pos;
        active[i] = g->players[i].hp > 0;
        valid[i] = active[i];
        kind[i] = -1;
        g->last[i] = commands[i];
        if (!active[i]) continue;
        if (command < 0 || command >= COMMAND_COUNT) { valid[i] = 0; continue; }
        kind[i] = command / 4;
        target[i].x += dx[command % 4]; target[i].y += dy[command % 4];
        if (kind[i] == 1) continue; /* Attacks can point at empty space or boundaries. */
        if (!inside(g, target[i]) || g->terrain[target[i].y][target[i].x] == '#') valid[i] = 0;
        else if (kind[i] == 0 && same(target[i], g->exit) && g->key_owner != i) valid[i] = 0;
        else if (kind[i] == 2 && (g->key_owner != -1 || !same(target[i], g->key))) valid[i] = 0;
        else if (kind[i] == 3 && (g->key_owner != i || same(target[i], g->exit))) valid[i] = 0;
    }
    /* Simultaneous duplicate movement/drop claims and competing pickups all fail.
       Record every conflict before changing valid[], avoiding player-index priority. */
    for (i = 0; i < PLAYER_COUNT; ++i) {
        if (!valid[i]) continue;
        for (j = i + 1; j < PLAYER_COUNT; ++j) {
            if (!valid[j] || !same(target[i], target[j])) continue;
            if (((kind[i] == 0 || kind[i] == 3) && (kind[j] == 0 || kind[j] == 3)) ||
                (kind[i] == 2 && kind[j] == 2)) {
                g->cancelled[i] = g->cancelled[j] = 1;
            }
        }
    }
    for (i = 0; i < PLAYER_COUNT; ++i) if (g->cancelled[i]) valid[i] = 0;
    /* A move/drop into a stationary player's tile fails. Cascading cancellations
       propagate backwards; swaps and longer cycles of successful moves are legal. */
    do {
        changed = 0;
        for (i = 0; i < PLAYER_COUNT; ++i) {
            if (!valid[i] || (kind[i] != 0 && kind[i] != 3)) continue;
            for (j = 0; j < PLAYER_COUNT; ++j) {
                if (i != j && active[j] && same(target[i], start[j]) && !(valid[j] && kind[j] == 0)) {
                    valid[i] = 0; changed = 1; break;
                }
            }
        }
    } while (changed);
    /* Apply only validated actions. No AI is called during mutation. */
    for (i = 0; i < PLAYER_COUNT; ++i) {
        if (!active[i]) continue;
        if (!valid[i]) { g->cancelled[i] = g->damaged[i] = 1; continue; }
        if (kind[i] == 0) g->players[i].pos = target[i];
        else if (kind[i] == 2) g->key_owner = i;
        else if (kind[i] == 3) { g->key_owner = -1; g->key = target[i]; }
    }
    /* Each attack is a three-cell fan from the attacker's START position.
       An attacker fatally hit this turn still executes its chosen attack. */
    for (i = 0; i < PLAYER_COUNT; ++i) {
        if (!valid[i] || kind[i] != 1) continue;
        for (k = -1; k <= 1; ++k) {
            Position hit = target[i];
            if (commands[i] % 4 < 2) hit.x += k; else hit.y += k;
            if (!inside(g, hit) || g->terrain[hit.y][hit.x] == '#') continue;
            g->attack[hit.y][hit.x] = 1;
            for (j = 0; j < PLAYER_COUNT; ++j)
                if (i != j && active[j] && same(g->players[j].pos, hit)) g->damaged[j] = 1;
        }
    }
    /* Resolve all deaths before victory. A key carrier dying on the exit therefore
       cannot win; the key returns to its original location and the door stays locked. */
    for (i = 0; i < PLAYER_COUNT; ++i) {
        PlayerState *p = &g->players[i];
        if (!active[i] || !g->damaged[i]) continue;
        if (--p->hp > 0) continue;
        ++p->deaths;
        p->rest = p->deaths <= 64 ? (UINT64_C(1) << (p->deaths - 1)) : UINT64_MAX;
        if (g->key_owner == i) {
            g->key_owner = -1;
            g->key = same(p->pos, g->exit) ? g->key_start : p->pos;
        }
    }
    if (g->key_owner >= 0 && g->players[g->key_owner].hp > 0 && same(g->players[g->key_owner].pos, g->exit))
        g->winner = g->key_owner;
}
