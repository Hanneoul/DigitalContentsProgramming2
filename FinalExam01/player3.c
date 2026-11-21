#include "player_api.h"

/* 표시할 이름을 이 문자열에 입력하세요 (UTF-8, 최대 63바이트). */
char player3_name[PLAYER_NAME_BYTES] = "Player3";
#include <stdlib.h>
#include <string.h>

#define P3_INF 1000000
#define P3_WIN 100000.0
#define P3_KEY_VALUE 480.0
#define P3_LATE_TURN 110
#define P3_LEAD 2
#define P3_ARR 0.45
#define P3_SAMPLES 160

typedef struct P3Sim {
    Position pos[PLAYER_COUNT];
    int hp[PLAYER_COUNT];
    int owner, escaped;
    Position key;
} P3Sim;

typedef struct P3Brain {
    Snapshot s;
    Position me;
    int self, hp, carrying, carrier;
    int act[PLAYER_COUNT];
    unsigned int rng;
    unsigned char occ[MAP_MAX][MAP_MAX];
    int danger[MAP_MAX][MAP_MAX];
    int dExit[MAP_MAX][MAP_MAX];
    int dKey[MAP_MAX][MAP_MAX];
    int dCar[MAP_MAX][MAP_MAX];
    int dMe[MAP_MAX][MAP_MAX];
    int dHunt[MAP_MAX][MAP_MAX];
    int dX[MAP_MAX][MAP_MAX];
    Command ecmd[P3_SAMPLES][PLAYER_COUNT];
} P3Brain;

static const int P3_DX[4] = {0, 0, -1, 1};
static const int P3_DY[4] = {-1, 1, 0, 0};

static unsigned int p3_rand(unsigned int *r)
{
    unsigned int x = *r;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    *r = x;
    return x;
}

static double p3_unit(unsigned int *r) { return (p3_rand(r) >> 8) / 16777216.0; }

static int p3_cheb(Position a, Position b)
{
    int dx = abs(a.x - b.x), dy = abs(a.y - b.y);
    return dx > dy ? dx : dy;
}

static int p3_same(Position a, Position b) { return a.x == b.x && a.y == b.y; }

static int p3_in(const P3Brain *b, int x, int y)
{
    return x >= 0 && y >= 0 && x < b->s.size && y < b->s.size;
}

static int p3_pass(const P3Brain *b, int x, int y, int exit_ok)
{
    if (!p3_in(b, x, y)) return 0;
    if (b->s.terrain[y][x] == '#') return 0;
    if (b->s.terrain[y][x] == 'E' && !exit_ok) return 0;
    return 1;
}

static void p3_bfs(const P3Brain *b, Position src, int exit_ok, int block_enemy, int dist[MAP_MAX][MAP_MAX])
{
    Position queue[MAP_MAX * MAP_MAX];
    int head = 0, tail = 0, x, y, d;
    for (y = 0; y < MAP_MAX; ++y) for (x = 0; x < MAP_MAX; ++x) dist[y][x] = P3_INF;
    dist[src.y][src.x] = 0;
    queue[tail++] = src;
    while (head < tail) {
        Position c = queue[head++];
        for (d = 0; d < 4; ++d) {
            int nx = c.x + P3_DX[d], ny = c.y + P3_DY[d];
            if (!p3_pass(b, nx, ny, exit_ok) || dist[ny][nx] != P3_INF) continue;
            if (block_enemy && b->occ[ny][nx]) continue;
            dist[ny][nx] = dist[c.y][c.x] + 1;
            queue[tail].x = nx; queue[tail].y = ny; ++tail;
        }
    }
}

static void p3_dijkstra(const P3Brain *b, Position src, int exit_ok, int cost[MAP_MAX][MAP_MAX])
{
    unsigned char done[MAP_MAX][MAP_MAX];
    int x, y, d;
    memset(done, 0, sizeof(done));
    for (y = 0; y < MAP_MAX; ++y) for (x = 0; x < MAP_MAX; ++x) cost[y][x] = P3_INF;
    cost[src.y][src.x] = 0;
    for (;;) {
        int bx = -1, by = -1, best = P3_INF;
        for (y = 0; y < b->s.size; ++y) for (x = 0; x < b->s.size; ++x)
            if (!done[y][x] && cost[y][x] < best) { best = cost[y][x]; bx = x; by = y; }
        if (bx < 0) break;
        done[by][bx] = 1;
        for (d = 0; d < 4; ++d) {
            int nx = bx + P3_DX[d], ny = by + P3_DY[d], next;
            if (!p3_pass(b, nx, ny, exit_ok) || done[ny][nx] || b->occ[ny][nx]) continue;
            next = best + 10 + 6 * b->danger[ny][nx];
            if (next < cost[ny][nx]) cost[ny][nx] = next;
        }
    }
}

static int p3_threatened(const P3Brain *b, int x, int y, int radius)
{
    int e, n = 0;
    Position f;
    f.x = x; f.y = y;
    for (e = 0; e < PLAYER_COUNT; ++e)
        if (b->act[e] && b->s.key_owner != e && p3_cheb(b->s.players[e].pos, f) <= radius) ++n;
    return n;
}

static Position p3_kiting_target(const P3Brain *b)
{
    int x, y, e, d;
    Position best = b->me;
    double best_score = -1e18;

    for (y = 0; y < b->s.size; ++y) {
        for (x = 0; x < b->s.size; ++x) {
            int min_en = P3_INF, exits = 0;
            double score;
            if (!p3_pass(b, x, y, 0) || b->occ[y][x] || b->dMe[y][x] >= P3_INF) continue;
            for (e = 0; e < PLAYER_COUNT; ++e) {
                int dist;
                if (!b->act[e]) continue;
                dist = abs(x - b->s.players[e].pos.x) + abs(y - b->s.players[e].pos.y);
                if (dist < min_en) min_en = dist;
            }
            for (d = 0; d < 4; ++d) {
                if (p3_pass(b, x + P3_DX[d], y + P3_DY[d], 0) && !b->occ[y + P3_DY[d]][x + P3_DX[d]]) ++exits;
            }
            score = min_en * 18.0 - b->danger[y][x] * 25.0 + (exits >= 3 ? 12.0 : (exits == 2 ? 4.0 : -15.0))
                  - b->dExit[y][x] * 1.5 - b->dMe[y][x] * 0.5;
            if (score > best_score) {
                best_score = score;
                best.x = x;
                best.y = y;
            }
        }
    }
    return best;
}

static int p3_is_exit_blocked(const P3Brain *b)
{
    int d;
    for (d = 0; d < 4; ++d) {
        int nx = b->s.exit.x + P3_DX[d];
        int ny = b->s.exit.y + P3_DY[d];
        if (!p3_pass(b, nx, ny, 0)) continue;
        if (b->occ[ny][nx]) return 1;
    }
    return (p3_threatened(b, b->s.exit.x, b->s.exit.y, 1) > 0);
}

static Position p3_ambush_cell(P3Brain *b, int carrier_present)
{
    int layer[MAP_MAX * MAP_MAX + 2];
    double weight[MAP_MAX][MAP_MAX];
    int x, y, i, j, d, found = 0;
    Position best = b->me;
    double best_score = -1e18;
    memset(weight, 0, sizeof(weight));
    memset(layer, 0, sizeof(layer));
    if (carrier_present) {
        int total;
        p3_bfs(b, b->s.players[b->s.key_owner].pos, 1, 0, b->dX);
        total = b->dX[b->s.exit.y][b->s.exit.x];
        if (total < P3_INF) {
            for (y = 0; y < b->s.size; ++y) for (x = 0; x < b->s.size; ++x)
                if (b->dX[y][x] < P3_INF && b->dX[y][x] + b->dExit[y][x] == total &&
                    b->dExit[y][x] <= 4 && b->dX[y][x] >= 2) ++layer[b->dX[y][x]];
            for (y = 0; y < b->s.size; ++y) for (x = 0; x < b->s.size; ++x)
                if (b->dX[y][x] < P3_INF && b->dX[y][x] + b->dExit[y][x] == total &&
                    b->dExit[y][x] <= 4 && b->dX[y][x] >= 2) {
                    weight[y][x] = 10.0 / layer[b->dX[y][x]];
                    if (b->dExit[y][x] == 0) weight[y][x] *= 1.5;
                    found = 1;
                }
        }
    }
    if (!found) {
        for (y = 0; y < b->s.size; ++y) for (x = 0; x < b->s.size; ++x)
            if (b->dExit[y][x] <= 3) weight[y][x] = b->dExit[y][x] == 0 ? 10.0 : (4 - b->dExit[y][x]) * 3.0;
    }
    for (y = 0; y < b->s.size; ++y) for (x = 0; x < b->s.size; ++x) {
        double cover = 0.0, score;
        int arrive = b->dMe[y][x], exits = 0;
        if (!p3_pass(b, x, y, 0) || b->occ[y][x] || arrive >= P3_INF) continue;
        for (i = -1; i <= 1; ++i) for (j = -1; j <= 1; ++j) {
            int cx = x + i, cy = y + j;
            if ((!i && !j) || !p3_in(b, cx, cy)) continue;
            if (found && b->dX[cy][cx] - 1 < arrive) continue;
            cover += weight[cy][cx];
        }
        for (d = 0; d < 4; ++d) if (p3_pass(b, x + P3_DX[d], y + P3_DY[d], 0)) ++exits;
        score = cover - P3_ARR * arrive - 9.5 * p3_threatened(b, x, y, 2) + (exits >= 2 ? 3.5 : -5.0);
        if (score > best_score) { best_score = score; best.x = x; best.y = y; }
    }
    return best;
}

static void p3_resolve(const P3Brain *b, P3Sim *st, const Command cmd[PLAYER_COUNT])
{
    Position start[PLAYER_COUNT], target[PLAYER_COUNT];
    int active[PLAYER_COUNT], kind[PLAYER_COUNT], valid[PLAYER_COUNT];
    int cancelled[PLAYER_COUNT], damaged[PLAYER_COUNT];
    int i, j, k, changed;
    Position exit = b->s.exit;
    memset(cancelled, 0, sizeof(cancelled));
    memset(damaged, 0, sizeof(damaged));
    for (i = 0; i < PLAYER_COUNT; ++i) {
        int c = (int)cmd[i];
        start[i] = target[i] = st->pos[i];
        active[i] = st->hp[i] > 0;
        valid[i] = active[i];
        kind[i] = -1;
        if (!active[i]) continue;
        kind[i] = c / 4;
        if (kind[i] == 1 && st->owner == i) { valid[i] = 0; continue; }
        target[i].x += P3_DX[c % 4]; target[i].y += P3_DY[c % 4];
        if (kind[i] == 1) continue;
        if (!p3_in(b, target[i].x, target[i].y) || b->s.terrain[target[i].y][target[i].x] == '#') valid[i] = 0;
        else if (kind[i] == 0 && p3_same(target[i], exit) && st->owner != i) valid[i] = 0;
        else if (kind[i] == 2 && (st->owner != -1 || !p3_same(target[i], st->key))) valid[i] = 0;
        else if (kind[i] == 3 && (st->owner != i || p3_same(target[i], exit))) valid[i] = 0;
    }
    for (i = 0; i < PLAYER_COUNT; ++i) {
        if (!valid[i]) continue;
        for (j = i + 1; j < PLAYER_COUNT; ++j) {
            if (!valid[j] || !p3_same(target[i], target[j])) continue;
            if (((kind[i] == 0 || kind[i] == 3) && (kind[j] == 0 || kind[j] == 3)) ||
                (kind[i] == 2 && kind[j] == 2)) cancelled[i] = cancelled[j] = 1;
        }
    }
    for (i = 0; i < PLAYER_COUNT; ++i) if (cancelled[i]) valid[i] = 0;
    do {
        changed = 0;
        for (i = 0; i < PLAYER_COUNT; ++i) {
            if (!valid[i] || (kind[i] != 0 && kind[i] != 3)) continue;
            for (j = 0; j < PLAYER_COUNT; ++j)
                if (i != j && active[j] && p3_same(target[i], start[j]) && !(valid[j] && kind[j] == 0)) {
                    valid[i] = 0; changed = 1; break;
                }
        }
    } while (changed);
    for (i = 0; i < PLAYER_COUNT; ++i) {
        if (!active[i]) continue;
        if (!valid[i]) { damaged[i] = 1; continue; }
        if (kind[i] == 0) st->pos[i] = target[i];
        else if (kind[i] == 2) st->owner = i;
        else if (kind[i] == 3) { st->owner = -1; st->key = target[i]; }
    }
    for (i = 0; i < PLAYER_COUNT; ++i) {
        if (!valid[i] || kind[i] != 1) continue;
        for (k = -1; k <= 1; ++k) {
            Position hit = target[i];
            if ((int)cmd[i] % 4 < 2) hit.x += k; else hit.y += k;
            if (!p3_in(b, hit.x, hit.y) || b->s.terrain[hit.y][hit.x] == '#') continue;
            for (j = 0; j < PLAYER_COUNT; ++j)
                if (i != j && active[j] && p3_same(st->pos[j], hit)) damaged[j] = 1;
        }
    }
    for (i = 0; i < PLAYER_COUNT; ++i) {
        if (!active[i] || !damaged[i]) continue;
        if (--st->hp[i] > 0) continue;
        if (st->owner == i) {
            st->owner = -1;
            st->key = p3_same(st->pos[i], exit) ? b->s.key_start : st->pos[i];
        }
    }
    st->escaped = -1;
    if (st->owner >= 0 && st->hp[st->owner] > 0 && p3_same(st->pos[st->owner], exit)) st->escaped = st->owner;
}

static int p3_dir_to(int dx, int dy, unsigned int *r)
{
    if (!dx) return dy < 0 ? 0 : 1;
    if (!dy) return dx < 0 ? 2 : 3;
    return (p3_rand(r) & 1) ? (dy < 0 ? 0 : 1) : (dx < 0 ? 2 : 3);
}

static Command p3_enemy_command(P3Brain *b, int e)
{
    Position ep = b->s.players[e].pos;
    int carrier = b->s.key_owner == e, i, d, nadv = 0, adv[4];
    int (*goal)[MAP_MAX];
    double u = p3_unit(&b->rng);
    if (u < 0.06) return (Command)(p3_rand(&b->rng) % 12);
    if (carrier) {
        goal = b->dExit;
        for (d = 0; d < 4; ++d) {
            int nx = ep.x + P3_DX[d], ny = ep.y + P3_DY[d];
            if (p3_pass(b, nx, ny, 1) && goal[ny][nx] < goal[ep.y][ep.x]) adv[nadv++] = d;
        }
        if (nadv && p3_unit(&b->rng) < 0.90) return (Command)adv[p3_rand(&b->rng) % nadv];
        for (d = 0; d < 4; ++d) {
            int nx = ep.x + P3_DX[d], ny = ep.y + P3_DY[d];
            if (p3_pass(b, nx, ny, 1)) adv[nadv++] = d;
        }
        return nadv ? (Command)adv[p3_rand(&b->rng) % nadv] : COMMAND_UP;
    }
    if (b->s.key_owner < 0) {
        for (d = 0; d < 4; ++d)
            if (ep.x + P3_DX[d] == b->s.key.x && ep.y + P3_DY[d] == b->s.key.y && p3_unit(&b->rng) < 0.88)
                return (Command)(COMMAND_PICK_UP + d);
    }
    {
        double wsum = 0.0, wt[PLAYER_COUNT];
        int tx = 0, ty = 0;
        for (i = 0; i < PLAYER_COUNT; ++i) {
            Position tp = b->s.players[i].pos;
            wt[i] = 0.0;
            if (i == e || b->s.players[i].hp <= 0 || b->s.players[i].rank) continue;
            if (p3_cheb(ep, tp) != 1) continue;
            wt[i] = b->s.key_owner == i ? 3.5 : i == b->self ? 2.2 : 1.0;
            wsum += wt[i];
        }
        if (wsum > 0.0 && p3_unit(&b->rng) < 0.68) {
            double pick = p3_unit(&b->rng) * wsum;
            for (i = 0; i < PLAYER_COUNT; ++i) {
                if (wt[i] <= 0.0) continue;
                pick -= wt[i];
                if (pick <= 0.0 || i == PLAYER_COUNT - 1) { tx = b->s.players[i].pos.x; ty = b->s.players[i].pos.y; break; }
            }
            return (Command)(COMMAND_ATTACK_UP + p3_dir_to(tx - ep.x, ty - ep.y, &b->rng));
        }
    }
    if (p3_cheb(ep, b->me) <= 4 && p3_unit(&b->rng) < 0.2) goal = b->dHunt;
    else if (b->s.key_owner < 0) goal = b->dKey;
    else if (b->s.key_owner == b->self) goal = b->dHunt;
    else goal = b->dCar;
    for (d = 0; d < 4; ++d) {
        int nx = ep.x + P3_DX[d], ny = ep.y + P3_DY[d];
        if (p3_pass(b, nx, ny, 0) && goal[ny][nx] < goal[ep.y][ep.x]) adv[nadv++] = d;
    }
    if (nadv) return (Command)adv[p3_rand(&b->rng) % nadv];
    return (Command)(COMMAND_ATTACK_UP + (int)(p3_rand(&b->rng) % 4));
}

Command player3_choose_command(const char *state_path)
{
    P3Brain brain;
    P3Brain *b = &brain;
    FILE *file = fopen(state_path, "r");
    int ok, e, x, y, d, cmd, sample, best_cmd = -1;
    int cost[MAP_MAX][MAP_MAX], fieldT[MAP_MAX][MAP_MAX], fieldX[MAP_MAX][MAP_MAX];
    double best_value = -1e18;
    Position target;
    P3Sim init;
    if (!file) return COMMAND_ATTACK_UP;
    ok = snapshot_read(file, &b->s);
    fclose(file);
    if (!ok) return COMMAND_ATTACK_UP;

    b->self = b->s.self;
    b->me = b->s.players[b->self].pos;
    b->hp = b->s.players[b->self].hp;
    b->carrying = b->s.key_owner == b->self;
    b->carrier = b->s.key_owner;
    b->rng = 2463534242u ^ (unsigned int)(b->s.turn * 2654435761u) ^ (unsigned int)(b->self * 40503u + 17u);
    if (!b->rng) b->rng = 1u;
    memset(b->occ, 0, sizeof(b->occ));
    memset(b->danger, 0, sizeof(b->danger));
    for (e = 0; e < PLAYER_COUNT; ++e) {
        b->act[e] = e != b->self && b->s.players[e].hp > 0 && !b->s.players[e].rank;
        if (!b->act[e]) continue;
        b->occ[b->s.players[e].pos.y][b->s.players[e].pos.x] = 1;
        if (b->s.key_owner == e) continue;
        for (y = 0; y < b->s.size; ++y) for (x = 0; x < b->s.size; ++x) {
            Position c;
            int dist;
            c.x = x; c.y = y;
            dist = p3_cheb(b->s.players[e].pos, c);
            if (dist <= 1) b->danger[y][x] += 4;
            else if (dist == 2) b->danger[y][x] += 1;
        }
    }
    p3_bfs(b, b->s.exit, 1, 0, b->dExit);
    p3_bfs(b, b->s.key, 0, 0, b->dKey);
    p3_bfs(b, b->me, b->carrying, 1, b->dMe);
    p3_bfs(b, b->me, 0, 0, b->dHunt);
    for (y = 0; y < MAP_MAX; ++y) for (x = 0; x < MAP_MAX; ++x) b->dCar[y][x] = P3_INF;
    if (b->carrier >= 0 && b->carrier != b->self) p3_bfs(b, b->s.players[b->carrier].pos, 1, 0, b->dCar);

    if (b->carrying) {
        int blocked = p3_is_exit_blocked(b);
        int near_exit = (b->dExit[b->me.y][b->me.x] <= 2);
        if (blocked && !near_exit) {
            target = p3_kiting_target(b);
        } else if (b->hp == 1 && blocked) {
            target = p3_kiting_target(b);
        } else {
            target = b->s.exit;
        }
    } else if (b->carrier >= 0) {
        target = p3_ambush_cell(b, 1);
    } else {
        int mine = b->dKey[b->me.y][b->me.x], rival = P3_INF, alive = 0;
        int my_exit_d = b->dExit[b->me.y][b->me.x];
        for (e = 0; e < PLAYER_COUNT; ++e) {
            int r;
            if (!b->act[e]) continue;
            ++alive;
            r = b->dKey[b->s.players[e].pos.y][b->s.players[e].pos.x];
            if (r < rival) rival = r;
        }
        if (mine <= my_exit_d + 1 || mine <= rival + P3_LEAD || !alive || b->s.turn >= P3_LATE_TURN) {
            int bestc = P3_INF;
            target = b->me;
            p3_dijkstra(b, b->me, 0, cost);
            for (d = 0; d < 4; ++d) {
                int kx = b->s.key.x + P3_DX[d], ky = b->s.key.y + P3_DY[d];
                if (!p3_pass(b, kx, ky, 0) || b->occ[ky][kx]) continue;
                if (cost[ky][kx] < bestc) { bestc = cost[ky][kx]; target.x = kx; target.y = ky; }
            }
        } else {
            target = p3_ambush_cell(b, 0);
        }
    }
    p3_dijkstra(b, target, b->carrying, fieldT);
    p3_dijkstra(b, b->s.exit, 1, fieldX);

    for (sample = 0; sample < P3_SAMPLES; ++sample)
        for (e = 0; e < PLAYER_COUNT; ++e)
            b->ecmd[sample][e] = b->act[e] ? p3_enemy_command(b, e) : COMMAND_UP;

    for (e = 0; e < PLAYER_COUNT; ++e) {
        init.pos[e] = b->s.players[e].pos;
        init.hp[e] = b->s.players[e].rank ? 0 : b->s.players[e].hp;
    }
    init.owner = b->s.key_owner; init.key = b->s.key; init.escaped = -1;

    for (cmd = 0; cmd < COMMAND_COUNT; ++cmd) {
        double total = 0.0;
        for (sample = 0; sample < P3_SAMPLES; ++sample) {
            P3Sim st = init;
            Command all[PLAYER_COUNT];
            double v = 0.0;
            memcpy(all, b->ecmd[sample], sizeof(all));
            all[b->self] = (Command)cmd;
            p3_resolve(b, &st, all);
            if (st.escaped == b->self) {
                v = P3_WIN;
            } else {
                unsigned int deaths = b->s.players[b->self].deaths;
                if (st.escaped >= 0) v -= 650.0;
                if (st.hp[b->self] <= 0) {
                    v -= 260.0 + 60.0 * (deaths > 4 ? 4 : deaths);
                    if (b->carrying) v -= 420.0;
                } else {
                    int car = st.owner == b->self;
                    Position f = st.pos[b->self];
                    if (st.hp[b->self] < b->hp) v -= 45.0;
                    if (car) v += P3_KEY_VALUE;
                    if (car && !p3_is_exit_blocked(b)) {
                        v -= fieldX[f.y][f.x];
                    } else {
                        v -= fieldT[f.y][f.x];
                    }
                }
                for (e = 0; e < PLAYER_COUNT; ++e) {
                    int carr;
                    if (!b->act[e] || st.hp[e] >= b->s.players[e].hp) continue;
                    carr = b->carrier == e;
                    v += carr ? 65.0 : 12.0;
                    if (st.hp[e] <= 0) v += carr ? 130.0 : 18.0;
                }
            }
            total += v;
        }
        if (total > best_value) { best_value = total; best_cmd = cmd; }
    }
    if (best_cmd < 0) return b->carrying ? COMMAND_UP : COMMAND_ATTACK_UP;
    return (Command)best_cmd;
}
