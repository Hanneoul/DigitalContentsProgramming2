#include "player_api.h"

/* 표시할 이름을 이 문자열에 입력하세요 (UTF-8, 최대 63바이트). */
char player5_name[PLAYER_NAME_BYTES] = "Player5";
#include <time.h>

/* 다른 플레이어의 행동을 여섯 가지 성격(열쇠 직행, 사냥, 길목 차단, 방어 등)으로 가정해
   예측하는 모델. 매 턴 공개되는 상태 파일만 사용하며, 다른 플레이어의 함수나 파일은
   호출·열람하지 않는다(다른 조의 코드를 참조한 것이 아니다). */
static Command p5_model(const Snapshot *state)
{
    const int style = state->self;
    const int dx[4] = {0, 0, -1, 1}, dy[4] = {-1, 1, 0, 0};
    Snapshot s = *state;

    /* Fields 0..5: player distances; 6: pickup cells; 7: exit. */
    int field[8][MAP_MAX][MAP_MAX], occupied[MAP_MAX][MAP_MAX];
    int used[MAP_MAX][MAP_MAX] = {0};
    double route[MAP_MAX][MAP_MAX], threat[MAP_MAX][MAP_MAX];
    Position queue[MAP_MAX * MAP_MAX], predicted[PLAYER_COUNT], me, goal;
    int i, j, k, d, x, y, nx, ny, head, tail, carrying, owner;
    int target = -1, mode = 0, rival = 9999, eta, nearest = 9999;
    int ax, ay, bx, by, hit, future_hit, steps, choice, count;
    double best, value, cost, risk, attack, conflict, pickup_risk, stationary;
    double aggression, caution, weight, candidate;
    Command result = COMMAND_ATTACK_UP;
    me = s.players[s.self].pos;
    carrying = s.key_owner == s.self;
    owner = s.key_owner;
    memset(occupied, -1, sizeof(occupied));
    for (j = 0; j < PLAYER_COUNT; ++j)
        if (s.players[j].hp > 0)
            occupied[s.players[j].pos.y][s.players[j].pos.x] = j;

    /* BFS uses terrain, not Manhattan distance. A locked exit is never a bridge.
       Field 7 permits the exit so it can also predict a carrier's route. */
    for (k = 0; k < 8; ++k) {
        for (y = 0; y < s.size; ++y)
            for (x = 0; x < s.size; ++x) field[k][y][x] = 9999;
        head = tail = 0;
        goal = k < 6 ? s.players[k].pos : (k == 6 ? s.key : s.exit);
        for (i = 0; i < (k == 6 ? 4 : 1); ++i) {
            x = goal.x + (k == 6 ? dx[i] : 0);
            y = goal.y + (k == 6 ? dy[i] : 0);
            if (x < 0 || y < 0 || x >= s.size || y >= s.size) continue;
            if (s.terrain[y][x] == '#') continue;
            if (k != 7 && !carrying && s.terrain[y][x] == 'E') continue;
            field[k][y][x] = 0;
            queue[tail].x = x; queue[tail++].y = y;
        }
        while (head < tail) {
            Position p = queue[head++];
            for (d = 0; d < 4; ++d) {
                x = p.x + dx[d]; y = p.y + dy[d];
                if (x < 0 || y < 0 || x >= s.size || y >= s.size) continue;
                if (field[k][y][x] != 9999 || s.terrain[y][x] == '#') continue;
                if (k != 7 && !carrying && s.terrain[y][x] == 'E') continue;
                field[k][y][x] = field[k][p.y][p.x] + 1;
                queue[tail].x = x; queue[tail++].y = y;
            }
        }
    }
    for (j = 0; j < PLAYER_COUNT; ++j) {
        if (j == s.self || s.players[j].hp <= 0) continue;
        x = s.players[j].pos.x; y = s.players[j].pos.y;
        if (field[6][y][x] < rival) rival = field[6][y][x];
        eta = field[j][me.y][me.x];
        if (eta < nearest) { nearest = eta; target = j; }
    }
    /* Modes: 0 race for key; 1 hunt; 2 intercept along the exit route.
       P5 switches by race timing. P1 is its predictive interception counter. */
    if (owner >= 0 && !carrying) { target = owner; mode = 1; }
    if (!carrying && owner < 0) {
        if (style == 2 && nearest <= 5) mode = 1;
        if (style == 4 && rival < field[6][me.y][me.x] && nearest <= 5) mode = 1;
        if (style == 3 && rival + 2 < field[6][me.y][me.x]) mode = 2;
        if (style == 5 && rival + 2 < field[6][me.y][me.x]) mode = 2;
        if (style == 1 && rival < field[6][me.y][me.x]) mode = 2;
    }
    if (!carrying && owner >= 0 && (style == 1 || style == 3 || style == 5)) mode = 2;
    /* Opportunist prefers wounded targets, using path distance to break ties. */
    if (style == 4 && mode == 1 && owner < 0) {
        best = 1e30;
        for (j = 0; j < PLAYER_COUNT; ++j) {
            if (j == s.self || s.players[j].hp <= 0) continue;
            value = field[j][me.y][me.x] + 3 * s.players[j].hp;
            if (value < best) { best = value; target = j; }
        }
    }
    goal = s.key;
    if (carrying) goal = s.exit;
    else if (mode == 1 && target >= 0) goal = s.players[target].pos;
    else if (mode == 2) {
        /* Predict several steps along the carrier's shortest exit route.
           Before pickup, use the key as the expected starting region. */
        Position p = owner >= 0 ? s.players[owner].pos : s.key;
        goal = p;
        best = 1e30;
        for (steps = 0; steps < s.size * s.size; ++steps) {
            int self_eta = field[s.self][p.y][p.x];
            if (s.terrain[p.y][p.x] != 'E' && self_eta < 9999) {
                value = abs(self_eta - steps) * 4.0 + self_eta;
                if (self_eta <= steps + 1) value -= 8;
                if (value < best) { best = value; goal = p; }
            }
            choice = -1;
            for (d = 0; d < 4; ++d) {
                x = p.x + dx[d]; y = p.y + dy[d];
                if (x < 0 || y < 0 || x >= s.size || y >= s.size) continue;
                if (field[7][y][x] < field[7][p.y][p.x]) { choice = d; break; }
            }
            if (choice < 0) break;
            p.x += dx[choice]; p.y += dy[choice];
        }
    }
    /* Predict opponents independently from their public objectives.
       This is an estimate, never access to their actual commands. */
    for (j = 0; j < PLAYER_COUNT; ++j) {
        predicted[j] = s.players[j].pos;
        if (s.players[j].hp <= 0 || j == s.self) continue;
        k = owner == j ? 7 : (owner < 0 ? 6 : owner);
        x = predicted[j].x; y = predicted[j].y;
        choice = -1; count = field[k][y][x];
        for (i = 0; i < 4; ++i) {
            d = (i + j + s.turn % 4) % 4;
            nx = x + dx[d]; ny = y + dy[d];
            if (nx < 0 || ny < 0 || nx >= s.size || ny >= s.size) continue;
            if (s.terrain[ny][nx] == 'E' && owner != j) continue;
            if (occupied[ny][nx] >= 0) continue;
            if (field[k][ny][nx] < count) { count = field[k][ny][nx]; choice = d; }
        }
        if (choice >= 0) { predicted[j].x += dx[choice]; predicted[j].y += dy[choice]; }
    }
    caution = carrying ? 1.4 : (style == 0 || style == 2 ? 0.35 : 0.8);
    if (s.players[s.self].hp == 1) caution *= 1.6;
    aggression = style == 2 ? 1.8 : (style == 4 ? 1.3 : 1.0);
    if (carrying) aggression = 0.25;
    for (y = 0; y < s.size; ++y) for (x = 0; x < s.size; ++x) {
        threat[y][x] = 0;
        route[y][x] = 1e30;
        for (j = 0; j < PLAYER_COUNT; ++j) {
            if (j == s.self || s.players[j].hp <= 0) continue;
            ax = abs(x - s.players[j].pos.x); ay = abs(y - s.players[j].pos.y);
            if (ax <= 1 && ay <= 1 && ax + ay) threat[y][x] = 1;
        }
        if (x == goal.x && y == goal.y && (carrying || mode == 2)) route[y][x] = 0;
        if ((mode == 0 && !carrying && field[6][y][x] == 0) ||
            (mode == 1 && abs(x - goal.x) + abs(y - goal.y) == 1)) route[y][x] = 0;
        if (s.terrain[y][x] == '#' || (!carrying && s.terrain[y][x] == 'E')) route[y][x] = 1e30;
    }
    /* Weighted Dijkstra includes danger and occupancy in the whole route,
       avoiding the approach/retreat oscillation of a local danger penalty. */
    for (steps = 0; steps < s.size * s.size; ++steps) {
        best = 1e30; bx = by = -1;
        for (y = 0; y < s.size; ++y) for (x = 0; x < s.size; ++x)
            if (!used[y][x] && route[y][x] < best) { best = route[y][x]; bx = x; by = y; }
        if (bx < 0) break;
        used[by][bx] = 1;
        cost = 10 + 9 * caution * threat[by][bx];
        if (occupied[by][bx] >= 0 && occupied[by][bx] != s.self) cost += 25;
        for (d = 0; d < 4; ++d) {
            x = bx + dx[d]; y = by + dy[d];
            if (x < 0 || y < 0 || x >= s.size || y >= s.size) continue;
            if (s.terrain[y][x] == '#' || (!carrying && s.terrain[y][x] == 'E')) continue;
            if (best + cost < route[y][x]) route[y][x] = best + cost;
        }
    }
    best = -1e30;
    for (i = 0; i < 12; ++i) {
        d = (i % 4 + s.turn % 4 + s.self) % 4;
        x = me.x; y = me.y; attack = conflict = pickup_risk = 0;
        if (i < 4) {
            x += dx[d]; y += dy[d];
            if (x < 0 || y < 0 || x >= s.size || y >= s.size) continue;
            if (s.terrain[y][x] == '#' || occupied[y][x] >= 0) continue;
            if (!carrying && s.terrain[y][x] == 'E') continue;
            for (j = 0; j < PLAYER_COUNT; ++j)
                if (j != s.self && s.players[j].hp > 0 &&
                    predicted[j].x == x && predicted[j].y == y) conflict = 0.7;
        } else if (i >= 8) {
            if (owner != -1 || me.x + dx[d] != s.key.x || me.y + dy[d] != s.key.y) continue;
            for (j = 0; j < PLAYER_COUNT; ++j)
                if (j != s.self && s.players[j].hp > 0 &&
                    field[6][s.players[j].pos.y][s.players[j].pos.x] == 0) pickup_risk = 1;
            attack = pickup_risk ? (style == 0 ? 18 : -35) : 110;
        } else {
            for (j = 0; j < PLAYER_COUNT; ++j) {
                if (j == s.self || s.players[j].hp <= 0) continue;
                ax = s.players[j].pos.x - me.x; ay = s.players[j].pos.y - me.y;
                bx = predicted[j].x - me.x; by = predicted[j].y - me.y;
                hit = d < 2 ? (ay == dy[d] && abs(ax) <= 1) : (ax == dx[d] && abs(ay) <= 1);
                future_hit = d < 2 ? (by == dy[d] && abs(bx) <= 1) : (bx == dx[d] && abs(by) <= 1);
                stationary = predicted[j].x == s.players[j].pos.x && predicted[j].y == s.players[j].pos.y ? 1.0 : 0.45;
                weight = 24 + (s.players[j].hp == 1 ? 16 : 0) + (owner == j ? 40 : 0);
                if (owner < 0 && field[6][s.players[j].pos.y][s.players[j].pos.x] == 0) weight += 22;
                attack += aggression * weight * (stationary * hit + (1 - stationary) * future_hit);
            }
        }
        risk = threat[y][x];
        /* Multiple attacks still cost only one HP: never sum damage counts. */
        value = route[y][x] >= 1e29 ? -100000 : -route[y][x];
        value += attack - caution * (12 * risk + 24 * conflict);
        if (i < 4) value -= 1; /* Stationary attack is a legal fallback. */
        if (carrying && i < 4 && x == s.exit.x && y == s.exit.y) {
            if (s.players[s.self].hp == 2) value += 10000;
            else value += risk ? 0 : 10000;
        }
        /* Late tied decisions vary with the turn and self, without a fixed spawn. */
        candidate = value;
        if (candidate > best) { best = candidate; result = (Command)((i / 4) * 4 + d); }
    }
    return result;
}


/* Search helpers below use only player_api.h. They do not call or modify the
   live game core. All storage is reset/released during each decision. */
enum { P5_WIDTH = 48, P5_DEPTH = 12, P5_MODELS = 6, P5_INF = 1000000 };
enum { P5_PLAN_NONE, P5_PLAN_SKIRMISH, P5_PLAN_INTERCEPT };
/* 과제 규칙: 교착 판정 턴 수. 남은 상대가 1명 이하면 18턴, 상대 수와 무관하게는 60턴 동안 진척이 없을 때 */
enum { P5X_STALL_TURNS = 18, P5X_STALL_LONG = 60 };
static const int p5_dx[4] = {0, 0, -1, 1};
static const int p5_dy[4] = {-1, 1, 0, 0};
typedef struct P5Node {
    Snapshot state;
    int first, winner, cost, released;
    Position last, previous;
    int travel_history;
    double score;
} P5Node;
typedef struct P5Map {
    const Snapshot *terrain;
    int cells;
    unsigned char ready[2][MAP_MAX * MAP_MAX];
    int *distance;
    unsigned int guards;
    int escape_cost[MAP_MAX * MAP_MAX];
} P5Map;

/* Disposable workers cannot retain globals. This bounded, versioned record
   contains ONLY this AI's public observations and its own previous command. */
typedef struct P5Memory {
    uint32_t magic;
    Snapshot observed;
    int action, trail_count, stagnation, best_distance;
    int stationary[PLAYER_COUNT];
    int tactic, target, until, plan_start, plan_progress, last_eta, cooldown;
    Position waypoint, aim;
    Position trail[8];
    double belief[P5_MODELS];
    int breaker; /* 과제 규칙: 교착 돌파 중이면 1 (죽거나 누가 탈출하면 해제) */
} P5Memory;

static int p5_same(Position a, Position b) { return a.x == b.x && a.y == b.y; }
static int p5_inside(const Snapshot *s, Position p)
{
    return p.x >= 0 && p.y >= 0 && p.x < s->size && p.y < s->size;
}

/* Cache by terrain goal and lock state, not player number or starting position. */
static const int *p5_distances(P5Map *map, Position goal, int unlocked)
{
    const Snapshot *s = map->terrain;
    int queue[MAP_MAX * MAP_MAX], head = 0, tail = 0, i, d, x, y, at;
    int index = goal.y * s->size + goal.x;
    int *distance = map->distance + (unlocked * map->cells + index) * map->cells;
    if (map->ready[unlocked][index]) return distance;
    for (i = 0; i < map->cells; ++i) distance[i] = P5_INF;
    distance[index] = 0; queue[tail++] = index;
    while (head < tail) {
        int cell = queue[head++];
        for (d = 0; d < 4; ++d) {
            x = cell % s->size + p5_dx[d]; y = cell / s->size + p5_dy[d];
            if (x < 0 || y < 0 || x >= s->size || y >= s->size) continue;
            at = y * s->size + x;
            if (s->terrain[y][x] == '#' || (!unlocked && s->terrain[y][x] == 'E')) continue;
            if (distance[at] != P5_INF) continue;
            distance[at] = distance[cell] + 1; queue[tail++] = at;
        }
    }
    map->ready[unlocked][index] = 1;
    return distance;
}

static int p5_steps(P5Map *map, Position from, Position to, int unlocked)
{
    const int *distance;
    if (!p5_inside(map->terrain, to)) return 1000;
    distance = p5_distances(map, to, unlocked);
    return distance[from.y * map->terrain->size + from.x] > 1000 ?
        1000 : distance[from.y * map->terrain->size + from.x];
}

static int p5_pickup_steps(P5Map *map, const Snapshot *s, Position from)
{
    int d, distance = 1000;
    for (d = 0; d < 4; ++d) {
        Position p = {s->key.x + p5_dx[d], s->key.y + p5_dy[d]};
        int steps;
        if (!p5_inside(s, p) || s->terrain[p.y][p.x] == '#' || s->terrain[p.y][p.x] == 'E') continue;
        steps = p5_steps(map, from, p, 0);
        if (steps < distance) distance = steps;
    }
    return distance;
}

/* A local copy of the PUBLIC simultaneous rules, used only for prediction.
   Duplicate claims, cascading occupancy, movement-before-fan attacks, boolean
   damage, death drops and death-before-victory all match the real resolver. */
static int p5_simulate(Snapshot *s, const Command commands[PLAYER_COUNT], int *cancelled_self)
{
    Position start[PLAYER_COUNT], target[PLAYER_COUNT];
    int active[PLAYER_COUNT], kind[PLAYER_COUNT], valid[PLAYER_COUNT];
    int cancelled[PLAYER_COUNT] = {0}, damaged[PLAYER_COUNT] = {0};
    int i, j, d, changed;
    *cancelled_self = 0;
    for (i = 0; i < PLAYER_COUNT; ++i) {
        int command = commands[i];
        start[i] = target[i] = s->players[i].pos;
        active[i] = valid[i] = s->players[i].hp > 0;
        kind[i] = -1;
        if (!active[i]) continue;
        if (command < 0 || command >= COMMAND_COUNT) { valid[i] = 0; continue; }
        kind[i] = command / 4;
        if (kind[i] == 1 && s->key_owner == i) { valid[i] = 0; continue; } /* 과제 규칙: 열쇠 소유자의 공격은 취소되고 피해 1 */
        target[i].x += p5_dx[command % 4]; target[i].y += p5_dy[command % 4];
        if (kind[i] == 1) continue;
        if (!p5_inside(s, target[i]) || s->terrain[target[i].y][target[i].x] == '#') valid[i] = 0;
        else if (kind[i] == 0 && p5_same(target[i], s->exit) && s->key_owner != i) valid[i] = 0;
        else if (kind[i] == 2 && (s->key_owner != -1 || !p5_same(target[i], s->key))) valid[i] = 0;
        else if (kind[i] == 3 && (s->key_owner != i || p5_same(target[i], s->exit))) valid[i] = 0;
    }
    for (i = 0; i < PLAYER_COUNT; ++i) for (j = i + 1; j < PLAYER_COUNT; ++j) {
        if (!valid[i] || !valid[j] || !p5_same(target[i], target[j])) continue;
        if (((kind[i] == 0 || kind[i] == 3) && (kind[j] == 0 || kind[j] == 3)) ||
            (kind[i] == 2 && kind[j] == 2)) cancelled[i] = cancelled[j] = 1;
    }
    for (i = 0; i < PLAYER_COUNT; ++i) if (cancelled[i]) valid[i] = 0;
    do {
        changed = 0;
        for (i = 0; i < PLAYER_COUNT; ++i) {
            if (!valid[i] || (kind[i] != 0 && kind[i] != 3)) continue;
            for (j = 0; j < PLAYER_COUNT; ++j)
                if (i != j && active[j] && p5_same(target[i], start[j]) && !(valid[j] && kind[j] == 0)) {
                    valid[i] = 0; changed = 1; break;
                }
        }
    } while (changed);
    for (i = 0; i < PLAYER_COUNT; ++i) {
        if (!active[i]) continue;
        if (!valid[i]) { damaged[i] = 1; if (i == s->self) *cancelled_self = 1; continue; }
        if (kind[i] == 0) s->players[i].pos = target[i];
        else if (kind[i] == 2) s->key_owner = i;
        else if (kind[i] == 3) { s->key_owner = -1; s->key = target[i]; }
    }
    for (i = 0; i < PLAYER_COUNT; ++i) {
        if (!valid[i] || kind[i] != 1) continue;
        for (d = -1; d <= 1; ++d) {
            Position hit = target[i];
            if (commands[i] % 4 < 2) hit.x += d; else hit.y += d;
            if (!p5_inside(s, hit) || s->terrain[hit.y][hit.x] == '#') continue;
            for (j = 0; j < PLAYER_COUNT; ++j)
                if (i != j && active[j] && p5_same(s->players[j].pos, hit)) damaged[j] = 1;
        }
    }
    for (i = 0; i < PLAYER_COUNT; ++i) {
        PlayerState *p = &s->players[i];
        if (!active[i] || !damaged[i]) continue;
        if (--p->hp > 0) continue;
        ++p->deaths;
        p->rest = p->deaths <= 64 ? UINT64_C(1) << (p->deaths - 1) : UINT64_MAX;
        if (s->key_owner == i) { s->key_owner = -1; s->key = p5_same(p->pos, s->exit) ? s->key_start : p->pos; }
    }
    if (s->key_owner >= 0 && s->players[s->key_owner].hp > 0 &&
        p5_same(s->players[s->key_owner].pos, s->exit)) return s->key_owner;
    return -1;
}

static void p5_next_turn(Snapshot *s)
{
    int i, j;
    ++s->turn;
    for (i = 0; i < PLAYER_COUNT; ++i) {
        PlayerState *p = &s->players[i];
        if (p->hp > 0 || p->rank) continue; /* 과제 규칙: 탈출한 플레이어는 부활하지 않는다 */
        if (p->rest) { --p->rest; continue; }
        for (j = 0; j < PLAYER_COUNT; ++j)
            if (s->players[j].hp > 0 && p5_same(s->players[j].pos, p->spawn)) break;
        if (j == PLAYER_COUNT) { p->pos = p->spawn; p->hp = MAX_HP; }
    }
}

static Command p5_counter_model(const Snapshot *state)
{
    const int style = state->self;
    const int dx[4] = {0, 0, -1, 1}, dy[4] = {-1, 1, 0, 0};
    Snapshot s = *state;

    int field[8][MAP_MAX][MAP_MAX], occupied[MAP_MAX][MAP_MAX];
    int used[MAP_MAX][MAP_MAX] = {0};
    double route[MAP_MAX][MAP_MAX], threat[MAP_MAX][MAP_MAX];
    Position queue[MAP_MAX * MAP_MAX], predicted[PLAYER_COUNT], me, goal;
    int i, j, k, d, x, y, nx, ny, head, tail, owner, carrying;
    int nearest = 9999, target = -1, rival = 9999, mode = 0, distance;
    int choice, steps, bx, by, ax, ay, hit, future_hit, contender = 0;
    double best, value, cost, caution, aggression, attack, conflict, weight;
    uint32_t seed, noise;
    Command result = COMMAND_ATTACK_UP;
    me = s.players[s.self].pos; owner = s.key_owner; carrying = owner == s.self;
    memset(occupied,-1,sizeof(occupied));
    seed = (uint32_t)s.turn * 747796405u + (uint32_t)(s.self + 1) * 2891336453u;
    for (j = 0; j < PLAYER_COUNT; ++j) {
        if (s.players[j].hp > 0) occupied[s.players[j].pos.y][s.players[j].pos.x] = j;
        seed ^= ((uint32_t)s.players[j].pos.x + 37u * (uint32_t)s.players[j].pos.y +
            103u * (uint32_t)s.players[j].hp) * (uint32_t)(j + 11) * 277803737u;
    }
    seed ^= (uint32_t)(owner + 2) * 1597334677u;
    /* BFS: player distances, cells from which a key can be picked up, exit.
       The locked exit is not a shortcut for non-carriers. */
    for (k = 0; k < 8; ++k) {
        for (y = 0; y < s.size; ++y) for (x = 0; x < s.size; ++x) field[k][y][x] = 9999;
        goal = k < 6 ? s.players[k].pos : (k == 6 ? s.key : s.exit);
        head = tail = 0;
        for (i = 0; i < (k == 6 ? 4 : 1); ++i) {
            x = goal.x + (k == 6 ? dx[i] : 0); y = goal.y + (k == 6 ? dy[i] : 0);
            if (x < 0 || y < 0 || x >= s.size || y >= s.size || s.terrain[y][x] == '#') continue;
            if (k != 7 && !carrying && s.terrain[y][x] == 'E') continue;
            field[k][y][x] = 0; queue[tail].x = x; queue[tail++].y = y;
        }
        while (head < tail) {
            Position p = queue[head++];
            for (d = 0; d < 4; ++d) {
                x = p.x + dx[d]; y = p.y + dy[d];
                if (x < 0 || y < 0 || x >= s.size || y >= s.size) continue;
                if (field[k][y][x] != 9999 || s.terrain[y][x] == '#') continue;
                if (k != 7 && !carrying && s.terrain[y][x] == 'E') continue;
                field[k][y][x] = field[k][p.y][p.x] + 1;
                queue[tail].x = x; queue[tail++].y = y;
            }
        }
    }
    for (j = 0; j < PLAYER_COUNT; ++j) {
        if (j == s.self || s.players[j].hp <= 0) continue;
        x = s.players[j].pos.x; y = s.players[j].pos.y;
        if (field[6][y][x] < rival) rival = field[6][y][x];
        if (owner < 0 && field[6][y][x] == 0) ++contender;
        distance = field[j][me.y][me.x];
        if (distance < nearest) { nearest = distance; target = j; }
    }
    if (!carrying && owner >= 0) { mode = 1; target = owner; }
    if (!carrying && owner < 0) {
        if (style == 1 && rival + 1 < field[6][me.y][me.x]) mode = 2;
        if (style == 2 && nearest <= 3 && (field[6][me.y][me.x] <= 3 || rival == 0)) mode = 1;
        if (style == 4 && nearest <= 3 && field[6][me.y][me.x] > rival + 1) mode = 1;
    }
    if (!carrying && owner >= 0 && (style == 1 || style == 4)) mode = 2;
    goal = carrying ? s.exit : (mode == 1 && target >= 0 ? s.players[target].pos : s.key);
    if (mode == 2) {
        Position p = owner >= 0 ? s.players[owner].pos : s.key;
        best = 1e30; goal = p;
        for (steps = 0; steps < s.size * s.size; ++steps) {
            distance = field[s.self][p.y][p.x];
            if (s.terrain[p.y][p.x] != 'E' && distance < 9999) {
                value = abs(distance - steps) * 3.0 + distance;
                if (distance <= steps + 1) value -= 10;
                /* Stop the carrier BEFORE it reaches the exit at full HP. */
                if (field[7][p.y][p.x] >= 2 && field[7][p.y][p.x] <= 4) value -= 6;
                if (value < best) { best = value; goal = p; }
            }
            choice = -1;
            for (d = 0; d < 4; ++d) {
                x = p.x + dx[d]; y = p.y + dy[d];
                if (x < 0 || y < 0 || x >= s.size || y >= s.size) continue;
                if (field[7][y][x] < field[7][p.y][p.x]) { choice = d; break; }
            }
            if (choice < 0) break;
            p.x += dx[choice]; p.y += dy[choice];
        }
    }
    /* Estimate goal-directed movement without assuming that the enemy MUST
       move. Attack scoring below considers both current and future positions. */
    for (j = 0; j < PLAYER_COUNT; ++j) {
        predicted[j] = s.players[j].pos;
        if (j == s.self || s.players[j].hp <= 0) continue;
        k = owner == j ? 7 : (owner < 0 ? 6 : owner);
        x = predicted[j].x; y = predicted[j].y; distance = field[k][y][x]; choice = -1;
        for (i = 0; i < 4; ++i) {
            d = (i + j + s.turn % 4) % 4;
            nx = x + dx[d]; ny = y + dy[d];
            if (nx < 0 || ny < 0 || nx >= s.size || ny >= s.size) continue;
            if (occupied[ny][nx] >= 0 || (s.terrain[ny][nx] == 'E' && owner != j)) continue;
            if (field[k][ny][nx] < distance) { distance = field[k][ny][nx]; choice = d; }
        }
        if (choice >= 0) { predicted[j].x += dx[choice]; predicted[j].y += dy[choice]; }
    }
    caution = style == 3 ? 1.2 : (style == 0 ? 0.4 : 0.7);
    if (s.players[s.self].hp == 1) caution *= 1.5;
    aggression = style == 2 ? 1.6 : (style == 4 ? 1.2 : 0.9);
    if (carrying) { caution *= 1.2; aggression *= 0.3; }
    for (y = 0; y < s.size; ++y) for (x = 0; x < s.size; ++x) {
        threat[y][x] = 0; route[y][x] = 1e30;
        for (j = 0; j < PLAYER_COUNT; ++j) {
            if (j == s.self || s.players[j].hp <= 0) continue;
            ax = abs(x - s.players[j].pos.x); ay = abs(y - s.players[j].pos.y);
            if (ax <= 1 && ay <= 1 && ax + ay) threat[y][x] = 1;
        }
        if ((carrying || mode == 2) && x == goal.x && y == goal.y) route[y][x] = 0;
        if (!carrying && mode == 0 && field[6][y][x] == 0) route[y][x] = 0;
        if (mode == 1 && abs(x-goal.x) + abs(y-goal.y) == 1) route[y][x] = 0;
        if (s.terrain[y][x] == '#' || (!carrying && s.terrain[y][x] == 'E')) route[y][x] = 1e30;
    }
    /* Weighted path cost encourages safer routes without abandoning progress. */
    for (steps = 0; steps < s.size * s.size; ++steps) {
        best = 1e30; bx = by = -1;
        for (y = 0; y < s.size; ++y) for (x = 0; x < s.size; ++x)
            if (!used[y][x] && route[y][x] < best) { best = route[y][x]; bx = x; by = y; }
        if (bx < 0) break;
        used[by][bx] = 1; cost = 10 + 20 * caution * threat[by][bx];
        if (occupied[by][bx] >= 0 && occupied[by][bx] != s.self) cost += 30;
        for (d = 0; d < 4; ++d) {
            x = bx + dx[d]; y = by + dy[d];
            if (x < 0 || y < 0 || x >= s.size || y >= s.size) continue;
            if (s.terrain[y][x] == '#' || (!carrying && s.terrain[y][x] == 'E')) continue;
            if (best + cost < route[y][x]) route[y][x] = best + cost;
        }
    }
    best = -1e30;
    for (i = 0; i < 12; ++i) {
        d = (i % 4 + s.turn % 4 + s.self) % 4;
        x = me.x; y = me.y; attack = conflict = 0;
        if (i < 4) {
            x += dx[d]; y += dy[d];
            if (x < 0 || y < 0 || x >= s.size || y >= s.size) continue;
            if (s.terrain[y][x] == '#' || occupied[y][x] >= 0) continue;
            if (!carrying && s.terrain[y][x] == 'E') continue;
            for (j = 0; j < PLAYER_COUNT; ++j)
                if (j != s.self && s.players[j].hp > 0 && predicted[j].x == x && predicted[j].y == y) conflict = 1;
        } else if (i >= 8) {
            if (owner != -1 || me.x + dx[d] != s.key.x || me.y + dy[d] != s.key.y) continue;
            attack = contender ? (style == 0 ? 15 : -45) : 135;
        } else {
            for (j = 0; j < PLAYER_COUNT; ++j) {
                if (j == s.self || s.players[j].hp <= 0) continue;
                ax = s.players[j].pos.x - me.x; ay = s.players[j].pos.y - me.y;
                bx = predicted[j].x - me.x; by = predicted[j].y - me.y;
                hit = d < 2 ? (ay == dy[d] && abs(ax) <= 1) : (ax == dx[d] && abs(ay) <= 1);
                future_hit = d < 2 ? (by == dy[d] && abs(bx) <= 1) : (bx == dx[d] && abs(by) <= 1);
                weight = 25 + (s.players[j].hp == 1 ? 25 : 0) + (owner == j ? 55 : 0);
                if (owner < 0 && field[6][s.players[j].pos.y][s.players[j].pos.x] == 0) weight += 30;
                if (j == 5 && (owner == 5 || field[6][s.players[5].pos.y][s.players[5].pos.x] <= 1)) weight += 12;
                attack += aggression * weight * (0.55 * hit + 0.45 * future_hit);
            }
        }
        value = route[y][x] < 1e29 ? -route[y][x] : -100000;
        value += attack - 20 * caution * threat[y][x] - (s.players[s.self].hp == 1 ? 60 : 22) * conflict;
        /* Change the move/attack preference only in tactically relevant fights.
           Hash-based mixing is repeatable across tests and separate processes. */
        noise = seed ^ ((uint32_t)i + 1u) * 2246822519u;
        noise ^= noise >> 16; noise *= 3266489917u; noise ^= noise >> 13;
        if (nearest <= 2 && (i < 4 || attack > 0)) {
            value += ((double)(noise & 65535u) / 65535.0 - 0.5) * (style == 4 ? 65 : (style == 3 ? 35 : 25));
            if (style == 4 && i < 4 && (seed & 1u)) value += 15;
        }
        if (carrying && i < 4 && x == s.exit.x && y == s.exit.y) {
            if (s.players[s.self].hp == 2 || !threat[y][x]) value += 10000;
        }
        if (value > best) { best = value; result = (Command)((i / 4) * 4 + d); }
    }
    return result;
}



/* Broad behavioral hypotheses. These are not predictions of hidden commands:
   race, combat, defensive hold and evasive movement supplement the two known
   public-policy families. The same engine is used at EVERY spawn. */
static Command p5_general_policy(P5Map *map, const Snapshot *s, int role)
{
    const PlayerState *me=&s->players[s->self];
    Position goal=s->key, target, next;
    int occupied[MAP_MAX][MAP_MAX]={0}, nearest=1000, rival=-1, i,d,adjacent=1;
    int current_distance=1000, contenders=0, goal_distance, ax,ay;
    double scores[12],best=-1e30,attack,risk,value;
    Command actions[12],result=COMMAND_ATTACK_UP;
    int count=0;
    if(me->hp<=0)return result;
    for(i=0;i<PLAYER_COUNT;++i){
        int distance;
        if(i==s->self||s->players[i].hp<=0)continue;
        occupied[s->players[i].pos.y][s->players[i].pos.x]=1;
        distance=p5_steps(map,me->pos,s->players[i].pos,s->key_owner==s->self);
        if(distance<nearest){nearest=distance;rival=i;}
        if(s->key_owner<0&&abs(s->players[i].pos.x-s->key.x)+abs(s->players[i].pos.y-s->key.y)==1)++contenders;
    }
    if(s->key_owner==s->self){goal=s->exit;adjacent=0;}
    else if(s->key_owner>=0)goal=s->players[s->key_owner].pos;
    else if(role==1&&nearest<=4&&rival>=0)goal=s->players[rival].pos;
    if(role==2&&s->key_owner!=s->self){
        /* Defenders already covering a route may stand and attack instead of
           obeying the goal-chasing assumption. Far defenders seek a cut-off. */
        int exit_steps=p5_steps(map,me->pos,s->exit,1);
        int key_steps=p5_steps(map,me->pos,s->key,1);
        if(exit_steps<=4||key_steps<=2){goal=me->pos;adjacent=0;}
        else {
            Position p=s->key_owner>=0?s->players[s->key_owner].pos:s->key;
            const int *exit_distance=p5_distances(map,s->exit,1);
            int step,chosen=-1;
            double best_point=1e30;
            for(step=0;step<s->size*s->size;++step){
                int eta=p5_steps(map,me->pos,p,0);
                double point=eta+3.0*abs(eta-step);
                if(s->terrain[p.y][p.x]!='E'&&point<best_point){best_point=point;goal=p;adjacent=0;}
                chosen=-1;
                for(d=0;d<4;++d){next.x=p.x+p5_dx[d];next.y=p.y+p5_dy[d];
                    if(p5_inside(s,next)&&exit_distance[next.y*s->size+next.x]<exit_distance[p.y*s->size+p.x]){chosen=d;break;}}
                if(chosen<0)break;p.x+=p5_dx[chosen];p.y+=p5_dy[chosen];
            }
        }
    }
    if(adjacent){
        for(d=0;d<4;++d){target.x=goal.x+p5_dx[d];target.y=goal.y+p5_dy[d];
            if(!p5_inside(s,target)||s->terrain[target.y][target.x]=='#'||(s->terrain[target.y][target.x]=='E'&&s->key_owner!=s->self))continue;
            goal_distance=p5_steps(map,me->pos,target,s->key_owner==s->self);
            if(goal_distance<current_distance)current_distance=goal_distance;
        }
    }else current_distance=p5_steps(map,me->pos,goal,s->key_owner==s->self);
    for(i=0;i<12;++i){
        d=(i%4+s->self+s->turn%4)%4;target=me->pos;attack=0;risk=0;
        if(i<4){
            target.x+=p5_dx[d];target.y+=p5_dy[d];
            if(!p5_inside(s,target)||s->terrain[target.y][target.x]=='#'||occupied[target.y][target.x])continue;
            if(s->terrain[target.y][target.x]=='E'&&s->key_owner!=s->self)continue;
        }else if(i>=8){
            if(s->key_owner!=-1||me->pos.x+p5_dx[d]!=s->key.x||me->pos.y+p5_dy[d]!=s->key.y)continue;
            attack=contenders?(role==0?65:-20):130;
        }else{
            int j;
            for(j=0;j<PLAYER_COUNT;++j){
                int hit;double weight;
                if(j==s->self||s->players[j].hp<=0)continue;
                ax=s->players[j].pos.x-me->pos.x;ay=s->players[j].pos.y-me->pos.y;
                hit=d<2?(ay==p5_dy[d]&&abs(ax)<=1):(ax==p5_dx[d]&&abs(ay)<=1);
                weight=25+(s->players[j].hp==1?25:0)+(s->key_owner==j?50:0);
                attack+=hit*weight*(role==1?1.8:(role==2?1.3:0.65));
            }
            if(s->key_owner==s->self)attack*=0.25;
        }
        goal_distance=current_distance;
        if(i<4){
            goal_distance=1000;
            if(adjacent){
                for(d=0;d<4;++d){next.x=goal.x+p5_dx[d];next.y=goal.y+p5_dy[d];
                    if(!p5_inside(s,next)||s->terrain[next.y][next.x]=='#'||(s->terrain[next.y][next.x]=='E'&&s->key_owner!=s->self))continue;
                    ax=p5_steps(map,target,next,s->key_owner==s->self);if(ax<goal_distance)goal_distance=ax;}
            }else goal_distance=p5_steps(map,target,goal,s->key_owner==s->self);
        }
        for(d=0;d<PLAYER_COUNT;++d){
            if(d==s->self||s->players[d].hp<=0)continue;
            ax=abs(target.x-s->players[d].pos.x);ay=abs(target.y-s->players[d].pos.y);
            if(ax<=1&&ay<=1&&ax+ay)risk=1;
        }
        value=-12*goal_distance+attack-risk*(role==3?(me->hp==1?70:28):8);
        if(i<4&&s->key_owner==s->self&&p5_same(target,s->exit)&&(me->hp==2||!risk))value+=12000;
        d=(i%4+s->self+s->turn%4)%4;
        actions[count]=(Command)((i/4)*4+d);scores[count++]=value;
        if(value>best){best=value;result=actions[count-1];}
    }
    if(role==3&&nearest<=2){
        /* Deterministic alternatives represent uncertainty, not a claim to
           know the seed of a genuinely stochastic opponent. */
        unsigned int phase=(unsigned int)s->turn*747796405u+(unsigned int)s->self*2891336453u;
        if((phase&3u)==0)for(i=count-1;i>=0;--i)if(scores[i]>=best-18){result=actions[i];break;}
    }
    return result;
}

static void p5_hypothesis(P5Map *map,const Snapshot *s,int model,Command commands[PLAYER_COUNT])
{
    Snapshot view=*s;int i;
    for(i=0;i<PLAYER_COUNT;++i){
        view.self=i;commands[i]=COMMAND_ATTACK_UP;
        if(i==s->self||s->players[i].hp<=0)continue;
        if(model==0)commands[i]=p5_counter_model(&view);
        else if(model==1)commands[i]=p5_model(&view);
        else commands[i]=p5_general_policy(map,&view,model-2);
    }
}

/* Identify living opponents on the frontier of paths blocked by occupancy.
   A blocked key/exit triggers a siege objective instead of endless retreat. */
static int p5_reachable_goal(const Snapshot *s,Position start,int ignored,unsigned int *frontier)
{
    int occupied[MAP_MAX][MAP_MAX]={0},seen[MAP_MAX][MAP_MAX]={0};
    Position queue[MAP_MAX*MAP_MAX];int head=0,tail=0,i,d,reached=0;
    unsigned int blockers=0;
    for(i=0;i<PLAYER_COUNT;++i)if(i!=s->self&&i!=ignored&&s->players[i].hp>0)occupied[s->players[i].pos.y][s->players[i].pos.x]=i+1;
    queue[tail++]=start;seen[start.y][start.x]=1;
    while(head<tail){Position p=queue[head++];
        if((s->key_owner==s->self&&p5_same(p,s->exit))||
            (s->key_owner<0&&abs(p.x-s->key.x)+abs(p.y-s->key.y)==1)||
            (s->key_owner>=0&&s->key_owner!=s->self&&abs(p.x-s->players[s->key_owner].pos.x)+abs(p.y-s->players[s->key_owner].pos.y)==1))reached=1;
        for(d=0;d<4;++d){Position q={p.x+p5_dx[d],p.y+p5_dy[d]};
            if(!p5_inside(s,q)||seen[q.y][q.x]||s->terrain[q.y][q.x]=='#'||(s->terrain[q.y][q.x]=='E'&&s->key_owner!=s->self))continue;
            if(occupied[q.y][q.x]){blockers|=1u<<(occupied[q.y][q.x]-1);continue;}
            seen[q.y][q.x]=1;queue[tail++]=q;
        }
    }
    if(frontier)*frontier=blockers;
    return reached;
}

static unsigned int p5_blockers(const Snapshot *s)
{
    unsigned int frontier=0,useful=0;int i;
    if(p5_reachable_goal(s,s->players[s->self].pos,-1,&frontier))return 0;
    /* Ignore unrelated enemies around the reachable room: prioritize a guard
       whose removal actually reconnects the room to the key/exit. */
    for(i=0;i<PLAYER_COUNT;++i)if(frontier&(1u<<i))
        if(p5_reachable_goal(s,s->players[i].pos,i,NULL))useful|=1u<<i;
    return useful?useful:frontier;
}

/* A stateless likelihood estimate: compare a short reconstruction from the
   public spawn state with the CURRENT observed state. It never assumes access
   to hidden past commands, and retains a floor for every hypothesis. */
static void p5_model_weights(P5Map *map,const Snapshot *root,unsigned int blockers,double weights[P5_MODELS])
{
    int model,i,step;
    for(model=0;model<P5_MODELS;++model){
        weights[model]=model<2?1.2:1.0;
        if(root->turn>1&&root->turn<=7){
            Snapshot replay=*root;double error=0;int samples=0;
            replay.turn=1;replay.key=replay.key_start;replay.key_owner=-1;
            for(i=0;i<PLAYER_COUNT;++i){replay.players[i].pos=replay.players[i].spawn;replay.players[i].hp=MAX_HP;replay.players[i].deaths=0;replay.players[i].rest=0;}
            for(step=1;step<root->turn;++step){Command commands[PLAYER_COUNT];int cancelled,winner;
                p5_hypothesis(map,&replay,model,commands);
                commands[root->self]=p5_general_policy(map,&replay,0);
                winner=p5_simulate(&replay,commands,&cancelled);
                if(winner>=0)break;
                p5_next_turn(&replay);
            }
            for(i=0;i<PLAYER_COUNT;++i){
                if(i==root->self)continue;
                error+=p5_steps(map,root->players[i].pos,replay.players[i].pos,0);
                error+=2*abs(root->players[i].hp-replay.players[i].hp);++samples;
            }
            error/=samples?samples:1;
            weights[model]=0.2+3.0/(1.0+error*error);
        }
        if(blockers&&root->turn>=12&&model==4)weights[model]*=3.0;
        if(root->key_owner>=0&&model==5&&root->players[root->key_owner].hp==1)weights[model]*=1.5;
    }
}

static int p5_memory_load(const char *path,const Snapshot *root,P5Memory *memory)
{
    FILE *file;int ok,i;
    memset(memory,0,sizeof(*memory));
    if(root->turn<=1||!path[0])return 0;
    file=fopen(path,"rb");if(!file)return 0;
    ok=fread(memory,sizeof(*memory),1,file)==1;fclose(file);
    if(!ok||memory->magic!=UINT32_C(0x50354D35)||memory->observed.size!=root->size||
        memory->observed.self!=root->self||memory->observed.turn<1||memory->observed.turn>root->turn||
        memory->observed.key_owner < -1||memory->observed.key_owner>=PLAYER_COUNT||
        memory->action<0||memory->action>=COMMAND_COUNT||memory->trail_count<0||memory->trail_count>8||
        memory->stagnation<0||memory->stagnation>10000||memory->best_distance<0||memory->best_distance>1000||
        memory->tactic<P5_PLAN_NONE||memory->tactic>P5_PLAN_INTERCEPT||memory->target < -1||memory->target>=PLAYER_COUNT||
        memory->until<0||memory->until>memory->observed.turn+6||memory->cooldown<0||memory->cooldown>memory->observed.turn+6||
        memory->plan_start<0||memory->plan_start>memory->observed.turn||memory->plan_progress<0||memory->plan_progress>memory->observed.turn||
        memory->last_eta<0||memory->last_eta>1000||memory->breaker<0||memory->breaker>1||
        !p5_inside(root,memory->observed.key)||!p5_same(memory->observed.key_start,root->key_start)||
        !p5_same(memory->observed.exit,root->exit)||memcmp(memory->observed.terrain,root->terrain,sizeof(root->terrain)))return 0;
    for(i=0;i<PLAYER_COUNT;++i)
        if(memory->stationary[i]<0||memory->stationary[i]>10000||!p5_inside(root,memory->observed.players[i].pos)||!p5_same(memory->observed.players[i].spawn,root->players[i].spawn)||
            memory->observed.players[i].hp<0||memory->observed.players[i].hp>MAX_HP)return 0;
    for(i=0;i<P5_MODELS;++i)if(!(memory->belief[i]>=0&&memory->belief[i]<=1))return 0;
    for(i=0;i<memory->trail_count;++i)if(!p5_inside(root,memory->trail[i]))return 0;
    if(memory->tactic!=P5_PLAN_NONE&&(!p5_inside(root,memory->waypoint)||!p5_inside(root,memory->aim)))return 0;
    if(memory->observed.turn==root->turn&&memcmp(&memory->observed,root,sizeof(*root)))return 0;
    return 1;
}

static void p5_observe(P5Map *map,const Snapshot *root,P5Memory *memory,int valid,double weights[P5_MODELS])
{
    int i,model,consecutive=valid&&memory->observed.turn+1==root->turn,metric;
    double sum=0;
    if(root->key_owner==root->self)metric=p5_steps(map,root->players[root->self].pos,root->exit,1);
    else if(root->key_owner<0)metric=p5_pickup_steps(map,root,root->players[root->self].pos);
    else metric=p5_steps(map,root->players[root->self].pos,root->players[root->key_owner].pos,0);
    if(consecutive){
        int changed=root->key_owner!=memory->observed.key_owner||
            (root->key_owner<0&&!p5_same(root->key,memory->observed.key));
        int progress=changed;
        for(i=0;i<PLAYER_COUNT;++i)if(i!=root->self&&root->players[i].hp<memory->observed.players[i].hp)progress=1;
        /* A return from B to A is not new progress if A was already reached. */
        if(changed||metric<memory->best_distance){memory->best_distance=metric;progress=1;}
        if(progress)memory->stagnation=0;else if(memory->stagnation<10000)++memory->stagnation;
        for(model=0;model<P5_MODELS;++model){
            Snapshot predicted=memory->observed;Command commands[PLAYER_COUNT];
            double error=0;int cancelled,winner;
            p5_hypothesis(map,&predicted,model,commands);commands[root->self]=(Command)memory->action;
            winner=p5_simulate(&predicted,commands,&cancelled);p5_next_turn(&predicted);
            for(i=0;i<PLAYER_COUNT;++i){
                if(i!=root->self)error+=p5_steps(map,predicted.players[i].pos,root->players[i].pos,0);
                error+=(i==root->self?4:2)*abs(predicted.players[i].hp-root->players[i].hp);
                error+=2.0*(predicted.players[i].deaths!=root->players[i].deaths);
            }
            error+=3.0*(predicted.key_owner!=root->key_owner)+4.0*(winner>=0);
            error/=PLAYER_COUNT;
            weights[model]=(0.8*memory->belief[model]+0.2/P5_MODELS)/(1+error*error);sum+=weights[model];
        }
        if(sum>0)for(model=0;model<P5_MODELS;++model)weights[model]=0.025+0.85*weights[model]/sum;
    }
    if(valid&&!consecutive)for(model=0;model<P5_MODELS;++model)
        weights[model]=0.5*memory->belief[model]+0.5/P5_MODELS;
    if(!consecutive||memory->observed.players[root->self].deaths!=root->players[root->self].deaths){
        memory->trail_count=0;memory->stagnation=0;memory->best_distance=metric;
        memory->tactic=P5_PLAN_NONE;memory->target=-1;memory->until=memory->cooldown=0;
    }
    for(i=0;i<PLAYER_COUNT;++i){
        if(consecutive&&root->players[i].hp>0&&memory->observed.players[i].hp>0&&
            root->players[i].deaths==memory->observed.players[i].deaths&&
            p5_same(root->players[i].pos,memory->observed.players[i].pos)){
            if(memory->stationary[i]<10000)++memory->stationary[i];
        }else memory->stationary[i]=0;
    }
    if(memory->trail_count==8){for(i=1;i<8;++i)memory->trail[i-1]=memory->trail[i];--memory->trail_count;}
    memory->trail[memory->trail_count++]=root->players[root->self].pos;
    sum=0;for(model=0;model<P5_MODELS;++model)sum+=weights[model];
    for(model=0;model<P5_MODELS;++model)memory->belief[model]=weights[model]/sum;
}

static void p5_memory_save(const char *path,const Snapshot *root,Command action,P5Memory *memory)
{
    FILE *file;if(!path[0])return;
    memory->magic=UINT32_C(0x50354D35);memory->observed=*root;memory->action=action;
    file=fopen(path,"wb");if(!file)return;
    fwrite(memory,sizeof(*memory),1,file);fclose(file);
}

static int p5_fan_covers(Position from,Command action,Position cell)
{
    int d=(int)action-4,ax=cell.x-from.x,ay=cell.y-from.y;
    if(d<0||d>=4)return 0;
    return d<2?(ay==p5_dy[d]&&abs(ax)<=1):(ax==p5_dx[d]&&abs(ay)<=1);
}

static int p5_free_post(const Snapshot *s,Position p)
{
    int i;
    if(!p5_inside(s,p)||s->terrain[p.y][p.x]=='#'||s->terrain[p.y][p.x]=='E')return 0;
    for(i=0;i<PLAYER_COUNT;++i)
        if(i!=s->self&&s->players[i].hp>0&&p5_same(p,s->players[i].pos))return 0;
    return 1;
}

static int p5_near_threats(const Snapshot *s,Position p)
{
    int i,count=0;
    for(i=0;i<PLAYER_COUNT;++i)if(i!=s->self&&s->players[i].hp>0&&
        abs(p.x-s->players[i].pos.x)<=1&&abs(p.y-s->players[i].pos.y)<=1)++count;
    return count;
}

static int p5_post_exits(const Snapshot *s,Position p,int *safe)
{
    int d,count=0;*safe=0;
    for(d=0;d<4;++d){Position q={p.x+p5_dx[d],p.y+p5_dy[d]};
        if(!p5_free_post(s,q))continue;++count;if(!p5_near_threats(s,q))++*safe;}
    return count;
}

static int p5_battle_count(P5Map *map,const Snapshot *s)
{
    int i,j,count=0;
    for(i=0;i<PLAYER_COUNT;++i){
        if(i==s->self||s->players[i].hp<=0||p5_steps(map,s->players[i].pos,s->key,0)>3)continue;
        for(j=0;j<PLAYER_COUNT;++j)
            if(j!=i&&j!=s->self&&s->players[j].hp>0&&p5_steps(map,s->players[j].pos,s->key,0)<=3&&
                abs(s->players[i].pos.x-s->players[j].pos.x)<=2&&abs(s->players[i].pos.y-s->players[j].pos.y)<=2)break;
        if(j<PLAYER_COUNT)++count;
    }
    return count;
}

static int p5_precarrier_eta(P5Map *map,const Snapshot *s,int leader,Position aim)
{
    int d,best=1000;
    if(leader<0||s->players[leader].hp<=0)return best;
    for(d=0;d<4;++d){Position p={s->key.x+p5_dx[d],s->key.y+p5_dy[d]};int eta;
        if(!p5_inside(s,p)||s->terrain[p.y][p.x]=='#'||s->terrain[p.y][p.x]=='E')continue;
        eta=p5_steps(map,s->players[leader].pos,p,0)+1+p5_steps(map,p,aim,1);
        if(eta<best)best=eta;
    }
    return best;
}

static int p5_plan_eta(P5Map *map,const Snapshot *s,const P5Memory *memory)
{
    if(s->key_owner>=0&&s->key_owner!=s->self)
        return p5_steps(map,s->players[s->key_owner].pos,memory->aim,1);
    if(s->key_owner<0&&memory->target>=0&&s->players[memory->target].hp>0)
        return p5_precarrier_eta(map,s,memory->target,memory->aim);
    return 1000;
}

static int p5_plan_relevant(P5Map *map,const Snapshot *s,const P5Memory *memory)
{
    int eta,ours;
    if(memory->tactic==P5_PLAN_NONE||s->turn>memory->until||s->key_owner==s->self||
        !p5_free_post(s,memory->waypoint)||s->players[s->self].hp<=0)return 0;
    if(s->players[s->self].hp==1&&p5_near_threats(s,memory->waypoint))return 0;
    if(memory->tactic==P5_PLAN_SKIRMISH)
        return s->key_owner<0&&s->players[s->self].hp==2&&p5_battle_count(map,s)>=2&&
            memory->target>=0&&s->players[memory->target].hp>0&&
            p5_steps(map,s->players[s->self].pos,memory->waypoint,0)<=4;
    if(s->key_owner<0||memory->target<0||s->players[memory->target].hp<=0||s->key_owner!=memory->target)return 0;
    eta=p5_plan_eta(map,s,memory);ours=p5_steps(map,s->players[s->self].pos,memory->waypoint,0);
    if(eta>10||(eta==0?ours!=0:ours+1>eta))return 0;
    if(s->key_owner>=0){
        Position carrier=s->players[s->key_owner].pos;
        int left=p5_steps(map,carrier,s->exit,1),post_left=p5_steps(map,memory->aim,s->exit,1);
        if(left+1<post_left||eta+post_left>left+2)return 0;
    }
    return 1;
}

/* Empty attacks covering a live tactical approach are legal space-holding,
   not failed attacks. Expired, bypassed and unoccupied approaches do not qualify. */
static int p5_holding_attack(P5Map *map,const Snapshot *s,const P5Memory *memory,Command action)
{
    if(!p5_same(s->players[s->self].pos,memory->waypoint)||
        !p5_fan_covers(memory->waypoint,action,memory->aim)||!p5_plan_relevant(map,s,memory))return 0;
    return memory->tactic==P5_PLAN_SKIRMISH||p5_plan_eta(map,s,memory)<=5;
}

static int p5_skirmish_post(P5Map *map,const Snapshot *s,P5Memory *plan)
{
    Position me=s->players[s->self].pos;double best=-1e30;int x,y,i,d,found=0;
    for(y=0;y<s->size;++y)for(x=0;x<s->size;++x){
        Position p={x,y},aim={0,0};int safe,exits,eta,threats,target=-1;double fan_best=0;
        if(abs(x-me.x)+abs(y-me.y)>4||p5_same(p,s->key)||!p5_free_post(s,p))continue;
        eta=p5_steps(map,me,p,0);if(eta>4)continue;
        exits=p5_post_exits(s,p,&safe);threats=p5_near_threats(s,p);
        if(exits<2||safe<1||threats>2)continue;
        for(d=0;d<4;++d){double fan=0;int victim=-1;
            for(i=0;i<PLAYER_COUNT;++i){
                if(i==s->self||s->players[i].hp<=0||p5_steps(map,s->players[i].pos,s->key,0)>3)continue;
                if(p5_fan_covers(p,(Command)(4+d),s->players[i].pos)){
                    fan+=s->players[i].hp==1?1.8:1.0;
                    if(victim<0||s->players[i].hp<s->players[victim].hp)victim=i;
                }
            }
            if(fan>fan_best){fan_best=fan;target=victim;aim=s->players[victim].pos;}
        }
        if(target>=0){
            double score=55*fan_best+18*safe-22*threats-18*eta-2*p5_steps(map,p,s->exit,1);
            if(score>best){best=score;plan->waypoint=p;plan->aim=aim;plan->target=target;found=1;}
        }
    }
    return found;
}

static int p5_intercept_coverage(P5Map *map,const Snapshot *s,Position post,Position hit)
{
    int a,d,best=0,left=p5_steps(map,hit,s->exit,1);
    for(a=4;a<8;++a){int cells=0;
        if(!p5_fan_covers(post,(Command)a,hit))continue;
        for(d=0;d<4;++d){Position q={hit.x+p5_dx[d],hit.y+p5_dy[d]};
            if(p5_inside(s,q)&&s->terrain[q.y][q.x]!='#'&&
                p5_steps(map,q,s->exit,1)==left-1&&p5_fan_covers(post,(Command)a,q))++cells;}
        if(p5_fan_covers(post,(Command)a,s->exit))++cells;
        if(cells>best)best=cells;
    }
    return best;
}

static int p5_intercept_post(P5Map *map,const Snapshot *s,const double weights[P5_MODELS],int leader,P5Memory *plan)
{
    Position origin=s->key,me=s->players[s->self].pos;
    int delay=0,hp=2,x,y,i,d,found=0;double best=-1e30,votes[4]={0.15,0.15,0.15,0.15};
    if(s->key_owner>=0){
        leader=s->key_owner;origin=s->players[leader].pos;hp=s->players[leader].hp;
        for(i=0;i<P5_MODELS;++i){Command commands[PLAYER_COUNT];p5_hypothesis(map,s,i,commands);
            if(commands[leader]>=0&&commands[leader]<4)votes[commands[leader]]+=weights[i];}
    }else{
        int best_pick=10000;
        if(leader<0)return 0;
        hp=s->players[leader].hp;delay=p5_pickup_steps(map,s,s->players[leader].pos)+1;
        for(d=0;d<4;++d){Position p={s->key.x+p5_dx[d],s->key.y+p5_dy[d]};int cost;
            if(!p5_inside(s,p)||s->terrain[p.y][p.x]=='#'||s->terrain[p.y][p.x]=='E')continue;
            cost=10*p5_steps(map,s->players[leader].pos,p,0)+p5_steps(map,p,s->exit,1);
            if(cost<best_pick){best_pick=cost;origin=p;}
        }
    }
    for(y=0;y<s->size;++y)for(x=0;x<s->size;++x){
        Position hit={x,y};int arrival,left=p5_steps(map,origin,s->exit,1),remaining,eta;
        double support=0;
        if(s->terrain[y][x]=='#')continue;
        arrival=p5_steps(map,origin,hit,1);remaining=p5_steps(map,hit,s->exit,1);
        if(arrival+delay>10||arrival+remaining!=left||(hp==2&&remaining<2))continue;
        eta=s->key_owner>=0?arrival:p5_precarrier_eta(map,s,leader,hit);
        if(eta>10)continue;
        if(s->key_owner>=0&&arrival>0){
            for(d=0;d<4;++d){Position q={origin.x+p5_dx[d],origin.y+p5_dy[d]};
                if(p5_inside(s,q)&&s->terrain[q.y][q.x]!='#'&&p5_steps(map,q,hit,1)==arrival-1)support+=votes[d];}
        }else support=1;
        for(i=-1;i<=1;++i)for(d=-1;d<=1;++d){
            Position post={x+i,y+d};int ours,safe,exits,threats;double score;
            if((i==0&&d==0)||!p5_free_post(s,post))continue;
            ours=p5_steps(map,me,post,0);
            if(ours>6||(eta==0?ours!=0:ours+1>eta))continue;
            threats=p5_near_threats(s,post);if(s->players[s->self].hp==1&&threats)continue;
            exits=p5_post_exits(s,post,&safe);if(exits<2)continue;
            score=35*support-14*ours-4*(eta-ours)+8*safe-22*threats+
                12*p5_intercept_coverage(map,s,post,hit);
            if(score>best){best=score;plan->waypoint=post;plan->aim=hit;plan->target=leader;found=1;}
        }
    }
    return found;
}


/* 칸 p의 상하좌우 중 벽이 아닌 칸의 수. 2이면 폭 1칸 통로(길목)이다. */
static int p5x_floor_neighbors(const Snapshot *s, Position p)
{
    int d, n = 0;
    for (d = 0; d < 4; ++d) { Position q = {p.x + p5_dx[d], p.y + p5_dy[d]};
        if (p5_inside(s, q) && s->terrain[q.y][q.x] != '#') ++n; }
    return n;
}

/* 규칙 기반 행동에 쓰는 작은 보조 함수들. */
static int p5x_dx(int d){ return p5_dx[d]; }
static int p5x_dy(int d){ return p5_dy[d]; }
/* 칸 p에 살아 있는 상대가 서 있는가. */
static int p5x_occupied(const Snapshot *s, Position p)
{
    int i;
    for (i = 0; i < PLAYER_COUNT; ++i) if (i != s->self && s->players[i].hp > 0 && p5_same(s->players[i].pos, p)) return 1;
    return 0;
}
/* 칸 p 기준 체비셰프 거리 r 이내의 살아 있는 상대 수. r=1이면 "이번 턴에 p를 때릴 수 있는 상대 수"다. */
static int p5x_enemies_near(const Snapshot *s, Position p, int r)
{
    int i, n = 0;
    for (i = 0; i < PLAYER_COUNT; ++i)
        if (i != s->self && s->players[i].hp > 0 && abs(s->players[i].pos.x - p.x) <= r && abs(s->players[i].pos.y - p.y) <= r) ++n;
    return n;
}

/* 모든 길목: 열쇠 시작점->출구 최단경로 위의 폭 1칸 통로. */
static int p5x_gates(P5Map *map, const Snapshot *s, Position *out, int cap)
{
    int L = p5_steps(map, s->key_start, s->exit, 1), x, y, n = 0;
    for (y = 0; y < s->size && n < cap; ++y) for (x = 0; x < s->size && n < cap; ++x) {
        Position c = {x, y};
        if (s->terrain[y][x] == '#' || s->terrain[y][x] == 'E' || p5_same(c, s->key_start)) continue;
        if (p5x_floor_neighbors(s, c) != 2) continue;
        if (p5_steps(map, s->key_start, c, 1) + p5_steps(map, c, s->exit, 1) != L) continue;
        out[n++] = c;
    }
    return n;
}

static void p5_update_plan(P5Map *map,const Snapshot *root,const double weights[P5_MODELS],P5Memory *memory)
{
    P5Memory prior=*memory;int i,leader=-1,rival=1000,ours,battle,found=0;
    memory->tactic=P5_PLAN_NONE;memory->target=-1;memory->until=0;
    if(root->key_owner==root->self)return;
    ours=p5_pickup_steps(map,root,root->players[root->self].pos);
    for(i=0;i<PLAYER_COUNT;++i)if(i!=root->self&&root->players[i].hp>0){
        int eta=p5_pickup_steps(map,root,root->players[i].pos);
        if(eta<rival){rival=eta;leader=i;}
    }
    if(root->key_owner<0&&ours<rival)return; /* Do not abandon a winning key race. */
    battle=root->key_owner<0?p5_battle_count(map,root):0;
    if(p5_plan_relevant(map,root,&prior)){
        int eta=p5_plan_eta(map,root,&prior);
        if(prior.tactic==P5_PLAN_INTERCEPT){
            if(eta<prior.last_eta)prior.plan_progress=root->turn;
            if(p5_same(root->players[root->self].pos,prior.waypoint)&&root->turn-prior.plan_progress>=5){
                memory->cooldown=root->turn+3;return;
            }
            prior.last_eta=eta;
        }
        memory->tactic=prior.tactic;memory->target=prior.target;memory->waypoint=prior.waypoint;memory->aim=prior.aim;
        memory->until=prior.until;memory->plan_start=prior.plan_start;memory->plan_progress=prior.plan_progress;memory->last_eta=prior.last_eta;
        return;
    }
    if(root->key_owner<0&&root->players[root->self].hp==2&&battle>=2&&ours<=5){
        found=p5_skirmish_post(map,root,memory);if(found)memory->tactic=P5_PLAN_SKIRMISH;
    }
    /* Do not camp for a guessed future winner of an unresolved key fight.
       Lock interception only after the public snapshot identifies a carrier. */
    if(!found&&root->turn>memory->cooldown&&root->key_owner>=0){
        found=p5_intercept_post(map,root,weights,leader,memory);if(found)memory->tactic=P5_PLAN_INTERCEPT;
    }
    if(found){
        memory->until=root->turn+(memory->tactic==P5_PLAN_SKIRMISH?3:4);
        memory->plan_start=root->turn;memory->plan_progress=root->turn;memory->last_eta=p5_plan_eta(map,root,memory);
        if(prior.tactic==memory->tactic&&prior.target==memory->target&&p5_same(prior.waypoint,memory->waypoint)){
            memory->plan_start=prior.plan_start;memory->plan_progress=prior.plan_progress;
            if(memory->last_eta<prior.last_eta)memory->plan_progress=root->turn;
        }
    }
}

static double p5_plan_value(P5Map *map,const Snapshot *s,const Snapshot *root,const P5Memory *memory)
{
    int i,eta;double value=0;
    if(!p5_plan_relevant(map,s,memory))return 0;
    eta=p5_steps(map,s->players[s->self].pos,memory->waypoint,0);
    value-=(memory->tactic==P5_PLAN_SKIRMISH?35:50)*eta;
    if(eta==0)value+=60;
    if(memory->tactic==P5_PLAN_SKIRMISH){
        for(i=0;i<PLAYER_COUNT;++i){
            if(i==s->self||root->players[i].hp<=0||p5_steps(map,root->players[i].pos,root->key,0)>3)continue;
            if(s->players[i].hp<=0)value+=160;
            else if(p5_steps(map,s->players[i].pos,root->key,0)<=4)value+=100*(root->players[i].hp-s->players[i].hp);
        }
    }else if(eta+1<=p5_plan_eta(map,s,memory))value+=80;
    return value;
}

/* Persistent public observations identify guards. Cache weighted escape paths
   once per decision; the joint simulator still decides whether to fight. */
static void p5_escape_routes(P5Map *map,const Snapshot *s,const P5Memory *memory)
{
    int i,x,y,d,step,used[MAP_MAX*MAP_MAX]={0},danger[MAP_MAX*MAP_MAX]={0};
    int blocked[MAP_MAX*MAP_MAX]={0};
    map->guards=0;
    for(i=0;i<PLAYER_COUNT;++i){
        Position p=s->players[i].pos;
        if(i==s->self||s->players[i].hp<=0||s->key_owner==i||memory->stationary[i]<3||
            p5_steps(map,p,s->exit,1)>5)continue;
        map->guards|=1u<<i;blocked[p.y*s->size+p.x]=1;
        for(y=p.y-1;y<=p.y+1;++y)for(x=p.x-1;x<=p.x+1;++x){
            Position q={x,y};
            if(p5_inside(s,q)&&!p5_same(q,p))danger[y*s->size+x]=1;
        }
    }
    for(i=0;i<map->cells;++i)map->escape_cost[i]=P5_INF;
    map->escape_cost[s->exit.y*s->size+s->exit.x]=0;
    for(step=0;step<map->cells;++step){
        int best=P5_INF,at=-1;
        for(i=0;i<map->cells;++i)if(!used[i]&&map->escape_cost[i]<best){best=map->escape_cost[i];at=i;}
        if(at<0)break;used[at]=1;
        for(d=0;d<4;++d){
            x=at%s->size+p5_dx[d];y=at/s->size+p5_dy[d];
            if(x<0||y<0||x>=s->size||y>=s->size||s->terrain[y][x]=='#')continue;
            i=y*s->size+x;if(blocked[i])continue;
            {
                int cost=best+1+3*danger[at];
                if(cost<map->escape_cost[i])map->escape_cost[i]=cost;
            }
        }
    }
}

static double p5_escape_value(P5Map *map,const Snapshot *s,const Snapshot *root)
{
    int i,d,excess=1000,live=0;double value=0;
    if(!map->guards||s->players[s->self].hp<=0)return 0;
    for(i=0;i<PLAYER_COUNT;++i)if(map->guards&(1u<<i)){
        if(s->players[i].hp>0&&p5_same(s->players[i].pos,root->players[i].pos)){
            ++live;value+=180*(root->players[i].hp-s->players[i].hp);
        }else value+=300;
    }
    if(!live)return value;
    if(s->key_owner==s->self){
        Position p=s->players[s->self].pos;
        excess=map->escape_cost[p.y*s->size+p.x]-p5_steps(map,p,s->exit,1);
    }else if(s->key_owner<0){
        for(d=0;d<4;++d){Position p={s->key.x+p5_dx[d],s->key.y+p5_dy[d]};int cost;
            if(!p5_inside(s,p)||s->terrain[p.y][p.x]=='#'||s->terrain[p.y][p.x]=='E')continue;
            cost=map->escape_cost[p.y*s->size+p.x]-p5_steps(map,p,s->exit,1);
            if(cost<excess)excess=cost;
        }
    }else return 0; /* An escaping rival takes priority over clearing a post. */
    if(excess<0)excess=0;if(excess>12)excess=12;
    value-=excess*(s->key_owner==s->self?30:12)*live;
    return value;
}

static double p5_evaluate(P5Map *map,const P5Node *node,const Snapshot *root,unsigned int blockers,int reset)
{
    const Snapshot *s=&node->state;const PlayerState *me=&s->players[s->self];
    double value;int i,steps,rival=1000,cleared=0;
    if(node->winner>=0)return node->winner==s->self?12000-node->cost:-12000;
    value=(blockers?250:450)*me->hp-(reset?80.0:(blockers?550.0:1400.0))*(me->deaths-root->players[s->self].deaths);
    if(me->hp<=0)value-=reset?50:(blockers?650:2200);
    for(i=0;i<PLAYER_COUNT;++i){
        if(i==s->self)continue;
        value-=65*s->players[i].hp;
        if(blockers&(1u<<i)){
            int window=p5_steps(map,root->players[i].spawn,root->players[i].pos,0)-
                p5_steps(map,root->players[root->self].spawn,root->players[i].pos,0);
            double leverage=500+40.0*window;
            if(leverage<200)leverage=200;if(leverage>1000)leverage=1000;
            value+=leverage*(root->players[i].hp-s->players[i].hp);
            if(s->players[i].hp<=0||p5_steps(map,s->players[i].pos,root->players[i].pos,0)>1){
                /* A guard respawning away from its post is still dislodged,
                   even if it has recovered HP. Reward the real opening. */
                value+=(0.7+s->players[i].hp)*leverage;cleared=1;
            }
        }
    }
    if(s->key_owner==s->self){
        steps=p5_steps(map,me->pos,s->exit,1);value+=2900-85*steps;
    }else if(s->key_owner<0){
        int best_trip=100000,d;
        steps=p5_pickup_steps(map,s,me->pos);value-=65*steps;
        for(i=0;i<PLAYER_COUNT;++i){int eta;
            if(i==s->self||s->players[i].hp<=0)continue;
            eta=p5_pickup_steps(map,s,s->players[i].pos);if(eta<rival)rival=eta;
        }
        if(me->hp>0&&steps<rival&&steps<=1)value+=650;
        for(d=0;d<4;++d){Position p={s->key.x+p5_dx[d],s->key.y+p5_dy[d]};int trip;
            if(!p5_inside(s,p)||s->terrain[p.y][p.x]=='#'||s->terrain[p.y][p.x]=='E')continue;
            trip=3*p5_steps(map,me->pos,p,0)+p5_steps(map,p,s->exit,1);if(trip<best_trip)best_trip=trip;
        }
        value-=8*best_trip;
    }else{
        Position carrier=s->players[s->key_owner].pos;
        value-=1050+30*p5_steps(map,me->pos,carrier,0);
        value+=30*p5_steps(map,carrier,s->exit,1)-130*s->players[s->key_owner].hp;
    }
    if(blockers&&me->hp>0){
        int closest=1000;
        for(i=0;i<PLAYER_COUNT;++i)if((blockers&(1u<<i))&&s->players[i].hp>0){
            steps=p5_steps(map,me->pos,s->players[i].pos,s->key_owner==s->self);if(steps<closest)closest=steps;}
        if(closest<1000)value-=35*closest;
        if(cleared&&p5_reachable_goal(s,me->pos,-1,NULL))value+=1000;
    }
    return value-node->cost;
}

/* Up to two replies per relevant opponent: a fan hitting the destination,
   a duplicate move claim or competing pickup, plus its nominal command.
   Joint deviations of two rivals are resolved TOGETHER, including HP1 exits. */
static void p5_root_uncertainty(P5Map *map,const Snapshot *root,const double weights[P5_MODELS],double risk[COMMAND_COUNT],int fatal[COMMAND_COUNT])
{
    int action,model,j,k;
    Command forecasts[P5_MODELS][PLAYER_COUNT];
    for(model=0;model<P5_MODELS;++model)p5_hypothesis(map,root,model,forecasts[model]);
    for(action=0;action<COMMAND_COUNT;++action){
        double damage_sum=0,fatal_sum=0,cancel_sum=0,total=0;
        fatal[action]=0;
        for(model=0;model<P5_MODELS;++model){
            Command commands[PLAYER_COUNT],replies[2][3];int enemies[2],counts[2]={1,1},found=0;
            Position destination=root->players[root->self].pos;
            memcpy(commands,forecasts[model],sizeof(commands));commands[root->self]=(Command)action;
            if(action<4){destination.x+=p5_dx[action];destination.y+=p5_dy[action];}
            for(j=0;j<PLAYER_COUNT;++j){
                int d,n=1,closer=0,other,distance;Position p;
                if(j==root->self||root->players[j].hp<=0||found>=2)continue;
                p=root->players[j].pos;
                if(abs(p.x-destination.x)+abs(p.y-destination.y)>3&&
                    !(action>=8&&action<12&&abs(p.x-root->key.x)+abs(p.y-root->key.y)==1))continue;
                distance=abs(p.x-destination.x)+abs(p.y-destination.y);
                for(other=0;other<PLAYER_COUNT;++other){int qdistance;
                    if(other==root->self||other==j||root->players[other].hp<=0)continue;
                    qdistance=abs(root->players[other].pos.x-destination.x)+abs(root->players[other].pos.y-destination.y);
                    if(qdistance<distance||(qdistance==distance&&other<j))++closer;
                }
                if(closer>=2)continue;
                enemies[found]=j;replies[found][0]=commands[j];
                for(d=0;d<4&&n<3;++d){int ax=destination.x-p.x,ay=destination.y-p.y;
                    int hit=d<2?(ay==p5_dy[d]&&abs(ax)<=1):(ax==p5_dx[d]&&abs(ay)<=1);
                    if(hit&&(Command)(4+d)!=commands[j]){replies[found][n++]=(Command)(4+d);break;}}
                for(d=0;d<4&&n<3;++d){Position q={p.x+p5_dx[d],p.y+p5_dy[d]};Command alternative=(Command)d;
                    int useful=action<4&&p5_same(q,destination);
                    if(action>=8&&action<12&&root->key_owner<0&&p5_same(q,root->key)){alternative=(Command)(8+d);useful=1;}
                    if(useful&&p5_inside(root,q)&&root->terrain[q.y][q.x]!='#'&&
                        !(alternative<4&&p5_same(q,root->exit)&&root->key_owner!=j)&&alternative!=commands[j]){replies[found][n++]=alternative;break;}}
                counts[found]=n;++found;
            }
            for(j=0;j<counts[0];++j)for(k=0;k<counts[1];++k){
                Snapshot after=*root;int cancelled,winner;double weight=weights[model]*(j==0?0.7:0.15)*(k==0?0.7:0.15);
                Command saved0=COMMAND_ATTACK_UP,saved1=COMMAND_ATTACK_UP;
                if(found>0){saved0=commands[enemies[0]];commands[enemies[0]]=replies[0][j];}
                if(found>1){saved1=commands[enemies[1]];commands[enemies[1]]=replies[1][k];}
                winner=p5_simulate(&after,commands,&cancelled);(void)winner;
                total+=weight;
                if(after.players[root->self].hp<root->players[root->self].hp)damage_sum+=weight;
                if(root->players[root->self].hp>0&&after.players[root->self].hp<=0){fatal_sum+=weight;fatal[action]=1;}
                if(cancelled)cancel_sum+=weight;
                if(found>0)commands[enemies[0]]=saved0;if(found>1)commands[enemies[1]]=saved1;
            }
        }
        risk[action]=total?(280*damage_sum+1800*fatal_sum+180*cancel_sum)/total:0;
    }
}

Command player5_choose_command(const char *state_path)
{
    Snapshot root;P5Map map;P5Node *beams,*next,*swap;P5Memory memory;
    FILE *file=fopen(state_path,"r");clock_t started=clock();
    int ok,model,depth,i,j,action,n,at,cancelled,counts[P5_MODELS];
    double risk[COMMAND_COUNT],values[P5_MODELS][COMMAND_COUNT],weights[P5_MODELS],loop_cost[COMMAND_COUNT]={0},plan_bonus[COMMAND_COUNT]={0};
    int fatal[COMMAND_COUNT],reset,memory_valid,defending=0;unsigned int blockers;
    char memory_path[1024];
    Command result=COMMAND_ATTACK_UP;
    int p5x_forced=-1; /* 규칙이 행동을 확정하면 0 이상: 탐색을 건너뛴다 */
    if(!file)return result;ok=snapshot_read(file,&root);fclose(file);
    if(!ok||root.turn<0||!p5_inside(&root,root.key)||!p5_inside(&root,root.key_start)||!p5_inside(&root,root.exit))return result;
    /* This is the only guaranteed immediate escape shortcut. HP1 exits still
       undergo every model and joint alternative check. */
    if(root.key_owner==root.self&&root.players[root.self].hp==2){
        for(i=0;i<4;++i){Position p={root.players[root.self].pos.x+p5_dx[i],root.players[root.self].pos.y+p5_dy[i]};
            if(p5_same(p,root.exit))return (Command)i;}
    }
    result=p5_model(&root);
    memset(&map,0,sizeof(map));map.terrain=&root;map.cells=root.size*root.size;
    map.distance=(int *)malloc((size_t)2*map.cells*map.cells*sizeof(int));
    beams=(P5Node *)calloc(P5_MODELS*P5_WIDTH,sizeof(*beams));next=(P5Node *)calloc(P5_MODELS*P5_WIDTH,sizeof(*next));
    if(!map.distance||!beams||!next){free(map.distance);free(beams);free(next);return result;}
    memory_path[0]='\0';
    if(strlen(state_path)<sizeof(memory_path)-32)snprintf(memory_path,sizeof(memory_path),"%s.p5-%d-memory",state_path,root.self);
    memory_valid=p5_memory_load(memory_path,&root,&memory);
    if(memory_valid&&memory.observed.turn==root.turn){
        result=(Command)memory.action;
        free(map.distance);free(beams);free(next);return result;
    }
    blockers=p5_blockers(&root);p5_model_weights(&map,&root,blockers,weights);
    p5_observe(&map,&root,&memory,memory_valid,weights);
    p5_escape_routes(&map,&root,&memory);
    p5_update_plan(&map,&root,weights,&memory);
    if(p5x_forced<0&&root.key_owner==root.self&&root.players[root.self].hp==2){
        /* 운반 규칙: 열쇠를 들고 HP 2면 한 번 맞아도 죽지 않으므로 탐색 대신 최단경로로 출구까지 전진한다. */
        Position me=root.players[root.self].pos,gates[16],target=root.exit;int ng=p5x_gates(&map,&root,gates,16),g,d,best=100000,bd=-1,bestv;
        int direct=p5_steps(&map,me,root.exit,1),past=1;
        for(g=0;g<ng;++g)if(p5_steps(&map,gates[g],root.exit,1)<direct)past=0; /* 아직 길목 위쪽인가 */
        if(!past){
            for(g=0;g<ng;++g){int cost=p5_steps(&map,me,gates[g],1)+p5_steps(&map,gates[g],root.exit,1);
                cost+=4*p5x_enemies_near(&root,gates[g],2); /* 주변이 조용한 길목을 고른다 */
                if(cost<best){best=cost;target=gates[g];}}
        }
        bestv=p5_steps(&map,me,target,1)+(p5_same(target,root.exit)?0:p5_steps(&map,target,root.exit,1));
        for(d=0;d<4;++d){Position n={me.x+p5x_dx(d),me.y+p5x_dy(d)};int v;
            if(!p5_inside(&root,n)||root.terrain[n.y][n.x]=='#'||p5x_occupied(&root,n))continue;
            v=p5_steps(&map,n,target,1)+(p5_same(target,root.exit)?0:p5_steps(&map,target,root.exit,1));
            if(v<bestv){bestv=v;bd=d;}}
        if(bd>=0)p5x_forced=bd;
        /* 과제 규칙: 열쇠 소유 중 공격은 취소+피해이므로, 막히면 공격하지 않고 탐색에 맡긴다 */
    }
    {
        /* 과제 규칙: 턴 제한이 없어서, 상대를 기다리며 같은 칸을 오가면 경기가 끝나지 않을 수 있다.
           남은 상대가 1명 이하인데 18턴, 또는 상대 수와 무관하게 60턴 동안 열쇠/출구 쪽 진척이 없으면
           교착으로 보고, 위험을 감수하고 최단경로로 열쇠(소지 중이면 출구)로 간다.
           한 번 시작하면 죽거나 누가 탈출할 때까지 유지한다. 소지 중에는 절대 공격하지 않는다. */
        Position me=root.players[root.self].pos;int left=0,left_before=0,d,here,bd=-1,bv=1000,bt=100,blocked=-1;
        for(i=0;i<PLAYER_COUNT;++i)if(i!=root.self&&root.players[i].rank==0)++left;
        for(i=0;i<PLAYER_COUNT;++i)if(i!=root.self&&memory.observed.players[i].rank==0)++left_before;
        if(!memory_valid||memory.observed.turn+1!=root.turn||left!=left_before||
            memory.observed.players[root.self].deaths!=root.players[root.self].deaths)memory.breaker=0;
        if((left<=1&&memory.stagnation>=P5X_STALL_TURNS)||memory.stagnation>=P5X_STALL_LONG)memory.breaker=1;
        if(p5x_forced<0&&memory.breaker&&root.key_owner<0){
            here=p5_pickup_steps(&map,&root,me);
            for(d=0;d<4;++d){Position n={me.x+p5_dx[d],me.y+p5_dy[d]};int v,t;
                if(here==0){if(p5_same(n,root.key))bd=8+d;continue;} /* 열쇠 옆이면 바로 집는다 */
                if(!p5_inside(&root,n)||root.terrain[n.y][n.x]=='#'||root.terrain[n.y][n.x]=='E')continue;
                v=p5_pickup_steps(&map,&root,n);if(v>=here)continue;
                if(p5x_occupied(&root,n)){if(blocked<0)blocked=4+d;continue;} /* 길을 막은 상대는 때린다(비소지) */
                t=p5x_enemies_near(&root,n,1);
                if(v<bv||(v==bv&&t<bt)){bv=v;bt=t;bd=d;}
            }
            if(bd<0)bd=blocked;
        }else if(p5x_forced<0&&memory.breaker&&root.key_owner==root.self){
            here=p5_steps(&map,me,root.exit,1);
            for(d=0;d<4;++d){Position n={me.x+p5_dx[d],me.y+p5_dy[d]};int v,t;
                if(!p5_inside(&root,n)||root.terrain[n.y][n.x]=='#'||p5x_occupied(&root,n))continue;
                v=p5_steps(&map,n,root.exit,1);if(v>=here)continue;
                t=p5x_enemies_near(&root,n,1);
                if(v<bv||(v==bv&&t<bt)){bv=v;bt=t;bd=d;}
            }
        }
        if(bd>=0)p5x_forced=bd;
    }
    p5_root_uncertainty(&map,&root,weights,risk,fatal);
    for(action=4;action<8;++action)if(p5_holding_attack(&map,&root,&memory,(Command)action))defending=1;
    if(p5_plan_relevant(&map,&root,&memory)){
        int before=p5_steps(&map,root.players[root.self].pos,memory.waypoint,0);
        for(action=0;action<12;++action){
            if(action<4){Position p={root.players[root.self].pos.x+p5_dx[action],root.players[root.self].pos.y+p5_dy[action]};
                if(p5_inside(&root,p))plan_bonus[action]=40*(before-p5_steps(&map,p,memory.waypoint,0));
            }else if(action<8){
                if(p5_holding_attack(&map,&root,&memory,(Command)action))plan_bonus[action]+=35;
                if(memory.tactic==P5_PLAN_SKIRMISH)for(i=0;i<PLAYER_COUNT;++i)
                    if(i!=root.self&&root.players[i].hp>0&&p5_steps(&map,root.players[i].pos,root.key,0)<=3&&
                        p5_fan_covers(root.players[root.self].pos,(Command)action,root.players[i].pos))
                        plan_bonus[action]+=root.players[i].hp==1?25:15;
            }else plan_bonus[action]=50;
        }
    }
    reset=root.key_owner<0&&root.players[root.self].hp==1&&root.players[root.self].deaths<3&&
        !defending&&((blockers&&root.turn>=16)||memory.stagnation>=10);
    if(memory.stagnation>=4){
        for(action=0;action<8;++action){Position destination=root.players[root.self].pos;int visits=0;
            if(action<4){destination.x+=p5_dx[action];destination.y+=p5_dy[action];}
            for(i=0;i<memory.trail_count;++i)if(p5_same(destination,memory.trail[i]))++visits;
            loop_cost[action]=20*visits;
            if(action>=4)loop_cost[action]+=20+3*(memory.stagnation>30?30:memory.stagnation);
            if(p5_holding_attack(&map,&root,&memory,(Command)action))loop_cost[action]=0;
            else if(action<4&&p5_plan_relevant(&map,&root,&memory)&&
                p5_inside(&root,destination)&&p5_steps(&map,destination,memory.waypoint,0)<
                p5_steps(&map,root.players[root.self].pos,memory.waypoint,0))loop_cost[action]=0;
        }
    }
    for(model=0;model<P5_MODELS;++model){
        counts[model]=1;beams[model*P5_WIDTH].state=root;beams[model*P5_WIDTH].winner=-1;
        beams[model*P5_WIDTH].last=beams[model*P5_WIDTH].previous=root.players[root.self].pos;
        beams[model*P5_WIDTH].travel_history=1;
    }
    for(depth=0;depth<P5_DEPTH&&p5x_forced<0;++depth){ /* 규칙이 행동을 확정했으면 탐색 생략 */
        int complete=1,next_counts[P5_MODELS];
        for(model=0;model<P5_MODELS&&complete;++model){
            int offset=model*P5_WIDTH;n=0;
            for(action=0;action<COMMAND_COUNT;++action)values[model][action]=-1e30;
            for(i=0;i<counts[model];++i){
                P5Node *parent=&beams[offset+i];Snapshot start=parent->state;Command commands[PLAYER_COUNT];
                if(depth&&(double)(clock()-started)/CLOCKS_PER_SEC>0.85){complete=0;break;}
                if(depth&&parent->winner<0)p5_next_turn(&start);
                p5_hypothesis(&map,&start,model,commands);
                for(action=0;action<(parent->winner<0?COMMAND_COUNT:1);++action){
                    P5Node candidate=*parent;int moved=0,hits=0;
                    if(action>=12)continue; /* 열쇠를 스스로 버리는 명령은 탐색하지 않는다 */
                    if(action>=4&&action<8&&start.key_owner==start.self)continue; /* 과제 규칙: 열쇠 소유 중 공격은 취소+피해이므로 탐색하지 않는다 */
                    candidate.state=start;commands[start.self]=(Command)action;
                    if(parent->winner<0){
                        candidate.winner=p5_simulate(&candidate.state,commands,&cancelled);
                        if(start.players[start.self].hp>0&&cancelled)continue;
                        candidate.cost+=7;
                        if(action>=12){candidate.cost+=160;candidate.released=1;}
                        for(j=0;j<PLAYER_COUNT;++j)if(j!=start.self&&candidate.state.players[j].hp<start.players[j].hp)hits=1;
                        if(action>=4&&action<8&&!hits&&!p5_holding_attack(&map,&start,&memory,(Command)action))candidate.cost+=20;
                        moved=!p5_same(candidate.state.players[start.self].pos,start.players[start.self].pos);
                        if(moved&&parent->travel_history>=2&&p5_same(candidate.state.players[start.self].pos,parent->previous)&&
                            candidate.state.key_owner==start.key_owner&&!hits&&
                            !(p5_plan_relevant(&map,&start,&memory)&&
                              p5_steps(&map,candidate.state.players[start.self].pos,memory.waypoint,0)<
                              p5_steps(&map,start.players[start.self].pos,memory.waypoint,0)))candidate.cost+=80;
                        if(candidate.state.players[start.self].hp>0){
                            candidate.previous=parent->last;candidate.last=candidate.state.players[start.self].pos;
                            candidate.travel_history=parent->travel_history+1;
                        }else candidate.travel_history=0;
                        if(candidate.state.key_owner==start.self)candidate.cost+=2*p5_steps(&map,candidate.state.players[start.self].pos,start.exit,1);
                        else if(candidate.state.key_owner<0)candidate.cost+=2*p5_pickup_steps(&map,&candidate.state,candidate.state.players[start.self].pos);
                    }
                    if(!depth)candidate.first=action;
                    candidate.score=p5_evaluate(&map,&candidate,&root,blockers,reset);
                    if(candidate.winner<0)candidate.score+=p5_escape_value(&map,&candidate.state,&root);
                    if(candidate.winner<0)candidate.score+=p5_plan_value(&map,&candidate.state,&root,&memory);
                    {
                        int same_first=0,worst=-1;
                        for(j=0;j<n;++j)if(next[offset+j].first==candidate.first){++same_first;worst=j;}
                        if(same_first>=3){ /* 첫 행동당 노드 상한 */
                            if(next[offset+worst].score>=candidate.score)continue;
                            for(j=worst;j+1<n;++j)next[offset+j]=next[offset+j+1];--n;
                        }
                    }
                    for(j=0;j<n;++j)if(next[offset+j].first==candidate.first&&next[offset+j].winner==candidate.winner&&
                        !memcmp(&next[offset+j].state,&candidate.state,sizeof(Snapshot)))break;
                    if(j<n){
                        if(next[offset+j].score>=candidate.score)continue;
                        for(at=j;at+1<n;++at)next[offset+at]=next[offset+at+1];--n;
                    }
                    at=n;while(at>0&&candidate.score>next[offset+at-1].score)--at;
                    if(at>=P5_WIDTH)continue;if(n<P5_WIDTH)++n;
                    for(j=n-1;j>at;--j)next[offset+j]=next[offset+j-1];next[offset+at]=candidate;
                }
            }
            next_counts[model]=n;
            for(i=0;i<n;++i)if(next[offset+i].score>values[model][next[offset+i].first])values[model][next[offset+i].first]=next[offset+i].score;
        }
        if(!complete)break;
        {
            double best=-1e30;
            for(action=0;action<COMMAND_COUNT;++action){
                double mean=0,worst=1e30,total=0,value,ordered[P5_MODELS],mass[P5_MODELS],cumulative=0;int viable=0;
                for(model=0;model<P5_MODELS;++model){
                    double v=values[model][action];int index=model;
                    if(v<-1e20)v=-10000;else ++viable;
                    mean+=weights[model]*v;total+=weights[model];
                    while(index>0&&v<ordered[index-1]){ordered[index]=ordered[index-1];mass[index]=mass[index-1];--index;}
                    ordered[index]=v;mass[index]=weights[model];
                }
                if(!viable)continue;
                for(model=0;model<P5_MODELS;++model){cumulative+=mass[model];worst=ordered[model];if(cumulative>=total*0.2)break;}
                if(!reset&&fatal[action]&&root.players[root.self].hp==1&&worst>-6500)worst=-6500;
                value=0.65*(mean/total)+0.35*worst-risk[action]*(reset?0.12:1.0)-loop_cost[action]+plan_bonus[action];
                if(value>best){best=value;result=(Command)action;}
            }
        }
        swap=beams;beams=next;next=swap;
        for(model=0;model<P5_MODELS;++model)counts[model]=next_counts[model];
    }
    if(p5x_forced>=0)result=(Command)p5x_forced;
    if(root.key_owner==root.self&&result>=4&&result<8){ /* 과제 규칙: 열쇠 소유 중에는 공격 대신 출구에 가까워지는 빈 칸으로 */
        int d,best=100000;Position me=root.players[root.self].pos;
        for(d=0;d<4;++d){Position n={me.x+p5_dx[d],me.y+p5_dy[d]};int v;
            if(!p5_inside(&root,n)||root.terrain[n.y][n.x]=='#'||p5x_occupied(&root,n))continue;
            v=p5_steps(&map,n,root.exit,1);if(v<best){best=v;result=(Command)d;}}
    }
    p5_memory_save(memory_path,&root,result,&memory);
    free(map.distance);free(beams);free(next);return result;
}

