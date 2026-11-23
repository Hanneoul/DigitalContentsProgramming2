
#include "player_api.h"
char player1_name[PLAYER_NAME_BYTES] = "sujo";

#ifndef P1_SLACK_DIV
#define P1_SLACK_DIV 100
#endif
#ifndef P1_CHASE
#define P1_CHASE 2
#endif
#ifndef P1_K2
#define P1_K2 35
#endif
#ifndef P1_K1
#define P1_K1 100
#endif
#ifndef P1_HIT2
#define P1_HIT2 90
#endif
#ifndef P1_HIT1
#define P1_HIT1 450
#endif

#define P1_INF 1000000
#define P1_M MAP_MAX

typedef struct P1Ctx {
    Snapshot s;
    Position p, ex;
    int me, hp, n, carrier, owner;
    int occ[P1_M][P1_M];
    int danger[P1_M][P1_M];
    int dme[P1_M][P1_M];
    int dex[P1_M][P1_M];
    int val[P1_M][P1_M];
} P1Ctx;

static const int P1DX[4] = { 0, 0, -1, 1 }, P1DY[4] = { -1, 1, 0, 0 };

static int p1_pass(const P1Ctx* c, int x, int y, int exit_ok)
{
    if (x < 0 || y < 0 || x >= c->n || y >= c->n) return 0;
    if (c->s.terrain[y][x] == '#') return 0;
    if (x == c->ex.x && y == c->ex.y && !exit_ok) return 0;
    return 1;
}

static int p1_cheb(int ax, int ay, int bx, int by)
{
    int a = abs(ax - bx), b = abs(ay - by);
    return a > b ? a : b;
}

static int p1_fan(int px, int py, int dir, int x, int y)
{
    int fx = px + P1DX[dir], fy = py + P1DY[dir];
    return dir < 2 ? (y == fy && abs(x - fx) <= 1) : (x == fx && abs(y - fy) <= 1);
}

static int p1_alive(const P1Ctx* c, int i)
{
    return i != c->me && c->s.players[i].hp > 0 && c->s.players[i].rank == 0;
}

static void p1_bfs(const P1Ctx* c, int sx, int sy, int exit_ok, int dist[P1_M][P1_M])
{
    int qx[P1_M * P1_M], qy[P1_M * P1_M], h = 0, t = 0, x, y, d;
    for (y = 0; y < P1_M; ++y) for (x = 0; x < P1_M; ++x) dist[y][x] = P1_INF;
    dist[sy][sx] = 0;
    qx[t] = sx;
    qy[t++] = sy;

    while (h < t) {
        x = qx[h];
        y = qy[h++];

        for (d = 0; d < 4; ++d) {
            int nx = x + P1DX[d], ny = y + P1DY[d];
            if (!p1_pass(c, nx, ny, exit_ok) || dist[ny][nx] != P1_INF) continue;
            dist[ny][nx] = dist[y][x] + 1;
            qx[t] = nx;
            qy[t++] = ny;
        }
    }
}

static void p1_init(P1Ctx* c)
{
    int i, x, y;
    c->n = c->s.size;
    c->me = c->s.self;
    c->p = c->s.players[c->me].pos;
    c->hp = c->s.players[c->me].hp;
    c->owner = c->s.key_owner;
    c->carrier = c->owner == c->me;
    c->ex = c->s.exit;

    memset(c->occ, 0, sizeof(c->occ));
    memset(c->danger, 0, sizeof(c->danger));

    for (i = 0; i < PLAYER_COUNT; ++i) {
        if (!p1_alive(c, i)) continue;

        c->occ[c->s.players[i].pos.y][c->s.players[i].pos.x] = 1;
        if (i == c->owner) continue;

        for (y = -1; y <= 1; ++y) {
            for (x = -1; x <= 1; ++x) {
                int cx = c->s.players[i].pos.x + x;
                int cy = c->s.players[i].pos.y + y;

                if ((x || y) && p1_pass(c, cx, cy, 1))
                    ++c->danger[cy][cx];
            }
        }
    }

    p1_bfs(c, c->p.x, c->p.y, c->carrier, c->dme);
    p1_bfs(c, c->ex.x, c->ex.y, 1, c->dex);
}

static int p1_imm(const P1Ctx* c, int x, int y)
{
    int dg = c->danger[y][x], base = c->hp == 1 ? P1_HIT1 : P1_HIT2;
    if (dg <= 0) return 0;
    return dg == 1 ? base / 2 : dg == 2 ? base * 3 / 4 : base * 7 / 8;
}

static int p1_contest(const P1Ctx* c, int x, int y)
{
    int i, cnt = 0;
    for (i = 0; i < PLAYER_COUNT; ++i)
        if (p1_alive(c, i) && abs(c->s.players[i].pos.x - x) + abs(c->s.players[i].pos.y - y) == 1)
            ++cnt;
    return cnt;
}

static void p1_field(P1Ctx* c, const unsigned char goal[P1_M][P1_M], int K)
{
    int x, y, d, changed;

    for (y = 0; y < P1_M; ++y)
        for (x = 0; x < P1_M; ++x)
            c->val[y][x] = goal[y][x] ? 0 : P1_INF;

    do {
        changed = 0;

        for (y = 0; y < c->n; ++y) {
            for (x = 0; x < c->n; ++x) {
                int cost;
                if (c->val[y][x] >= P1_INF) continue;

                cost = c->val[y][x] + 10 + K * (c->danger[y][x] > 3 ? 3 : c->danger[y][x]);

                for (d = 0; d < 4; ++d) {
                    int nx = x + P1DX[d], ny = y + P1DY[d];

                    if (!p1_pass(c, nx, ny, 0) || c->occ[ny][nx]) continue;

                    if (cost < c->val[ny][nx]) {
                        c->val[ny][nx] = cost;
                        changed = 1;
                    }
                }
            }
        }
    } while (changed);
}

static int p1_attack_value(const P1Ctx* c, int dir)
{
    int e, total = 0, keyfloor = c->owner < 0;

    for (e = 0; e < PLAYER_COUNT; ++e) {
        Position q = c->s.players[e].pos;
        int qx[5], qy[5], w[5], cnt, wsum = 0, cov = 0, m, d;
        int best = -1, bestd = P1_INF, keyadj, base;

        if (!p1_alive(c, e) || p1_cheb(c->p.x, c->p.y, q.x, q.y) > 2)
            continue;

        keyadj = keyfloor && abs(q.x - c->s.key.x) + abs(q.y - c->s.key.y) == 1;

        qx[0] = q.x;
        qy[0] = q.y;
        w[0] = keyadj ? 6 : 2;
        cnt = 1;

        for (d = 0; d < 4; ++d) {
            int nx = q.x + P1DX[d], ny = q.y + P1DY[d];

            if (!p1_pass(c, nx, ny, e == c->owner) || (nx == c->p.x && ny == c->p.y))
                continue;

            qx[cnt] = nx;
            qy[cnt] = ny;
            w[cnt] = 1;

            if (e == c->owner && c->dex[ny][nx] < bestd) {
                bestd = c->dex[ny][nx];
                best = cnt;
            }

            ++cnt;
        }

        if (best >= 0) w[best] = 5;

        for (m = 0; m < cnt; ++m) {
            wsum += w[m];
            if (p1_fan(c->p.x, c->p.y, dir, qx[m], qy[m]))
                cov += w[m];
        }

        if (!cov) continue;

        if (e == c->owner)
            base = c->s.players[e].hp == 1 ? 900 : 450;
        else if (keyadj)
            base = c->s.players[e].hp == 1 ? 1000 : 100;
        else
            base = c->s.players[e].hp == 1 ? 160 : 70;

        total += base * cov / wsum;
    }

    return total;
}

static int p1_post(const P1Ctx* c, int ox, int oy, int delay, int* gx, int* gy, int* gdir)
{
    int dO[P1_M][P1_M], ax[4], ay[4], aw[4], star[4];
    int na = 0, besta = P1_INF, d, i, x, y, tc, found = 0, best = -1000000000;

    p1_bfs(c, ox, oy, 0, dO);

    for (d = 0; d < 4; ++d) {
        int nx = c->ex.x + P1DX[d], ny = c->ex.y + P1DY[d];

        if (!p1_pass(c, nx, ny, 0) || dO[ny][nx] >= P1_INF)
            continue;

        ax[na] = nx;
        ay[na] = ny;

        if (dO[ny][nx] < besta)
            besta = dO[ny][nx];

        ++na;
    }

    if (!na) return 0;

    for (i = 0; i < na; ++i) {
        int dd = dO[ay[i]][ax[i]];
        aw[i] = dd == besta ? 6 : dd == besta + 1 ? 3 : 1;
        star[i] = dd == besta;
    }

    tc = delay + besta;

    for (y = 0; y < c->n; ++y) {
        for (x = 0; x < c->n; ++x) {
            int ta, isA, dir, mob = 0, dg;

            if (!p1_pass(c, x, y, 0) || c->occ[y][x] || c->dme[y][x] >= P1_INF)
                continue;

            ta = c->dme[y][x];
            isA = abs(x - c->ex.x) + abs(y - c->ex.y) == 1;
            dg = c->danger[y][x] > 3 ? 3 : c->danger[y][x];

            for (d = 0; d < 4; ++d) {
                if (p1_pass(c, x + P1DX[d], y + P1DY[d], 0) &&
                    !c->occ[y + P1DY[d]][x + P1DX[d]])
                    ++mob;
            }

            for (dir = 0; dir < 4; ++dir) {
                int cov = 0, hs = 0, he = 0, sc;

                for (i = 0; i < na; ++i) {
                    if (p1_fan(x, y, dir, ax[i], ay[i])) {
                        cov += aw[i];
                        if (star[i]) hs = 1;
                    }
                }

                if (p1_fan(x, y, dir, c->ex.x, c->ex.y)) {
                    cov += 5;
                    he = 1;
                }

                if (!cov) continue;
                if (hs && he) cov += 8;

                sc = 10 * cov - 5 * ta
                    - (ta > tc - 1 ? 40 * (ta - (tc - 1)) : 0)
                    - (c->hp == 1 ? 90 : 35) * dg
                    - (isA ? 15 : 0)
                    + 3 * mob
                    + (x == c->p.x && y == c->p.y ? 30 : 0);

                if (sc > best) {
                    best = sc;
                    *gx = x;
                    *gy = y;
                    *gdir = dir;
                    found = 1;
                }
            }
        }
    }

    return found;
}

static int p1_pickd(int dk)
{
    return dk >= P1_INF ? P1_INF : (dk == 0 ? 1 : dk - 1);
}

/* Turns until this player can issue a pickup beside a dropped key. */
static int p1_pickup_eta(const P1Ctx* c, int sx, int sy, int kx, int ky)
{
    int dist[P1_M][P1_M], best = P1_INF, d;

    if (!p1_pass(c, sx, sy, 0) || !p1_pass(c, kx, ky, 0))
        return P1_INF;

    p1_bfs(c, sx, sy, 0, dist);
    for (d = 0; d < 4; ++d) {
        int ax = kx + P1DX[d], ay = ky + P1DY[d];
        int eta;

        if (!p1_pass(c, ax, ay, 0) || dist[ay][ax] >= P1_INF)
            continue;

        eta = dist[ay][ax] + 1; /* One more action to pick up. */
        if (eta < best) best = eta;
    }
    return best;
}

static Command p1_hunt(P1Ctx* c)
{
    unsigned char goal[P1_M][P1_M];
    int dkey[P1_M][P1_M];
    int gx = -1, gy = -1, gdir = -1, go = 0, i, d, k;
    int K = c->hp == 1 ? P1_K1 : P1_K2;
    int best = -1000000000, bestcmd = COMMAND_ATTACK_UP, contested = 0, alive = 0;
    int race_slack, late_bonus;
    int tie_dir = (c->p.x + 2 * c->p.y + c->me) & 3;

    memset(goal, 0, sizeof(goal));

    if (c->owner < 0) {
        int myd, ed = P1_INF;

        p1_bfs(c, c->s.key.x, c->s.key.y, 0, dkey);
        myd = p1_pickd(dkey[c->p.y][c->p.x]);

        for (i = 0; i < PLAYER_COUNT; ++i) {
            int de;

            if (!p1_alive(c, i)) continue;

            ++alive;
            de = p1_pickd(dkey[c->s.players[i].pos.y][c->s.players[i].pos.x]);

            if (de < ed) ed = de;

            if (abs(c->s.players[i].pos.x - c->s.key.x) +
                abs(c->s.players[i].pos.y - c->s.key.y) == 1)
                contested = 1;
        }

        race_slack = c->n / 2;
        if (race_slack < 3) race_slack = 3;
        if (race_slack > 8) race_slack = 8;

        late_bonus = c->s.turn / P1_SLACK_DIV;
        if (late_bonus > 3) late_bonus = 3;

        go = myd < P1_INF && (!alive || myd <= ed + race_slack + late_bonus);

        if (go) {
            int cnt = 0;

            for (d = 0; d < 4; ++d) {
                int nx = c->s.key.x + P1DX[d];
                int ny = c->s.key.y + P1DY[d];

                if (p1_pass(c, nx, ny, 0) && !c->occ[ny][nx]) {
                    goal[ny][nx] = 1;
                    ++cnt;
                }
            }

            if (!cnt) go = 0;
        }

        if (!go) {
            if (p1_post(c, c->s.key.x, c->s.key.y,
                (ed < P1_INF ? ed : 0) + 1, &gx, &gy, &gdir))
                goal[gy][gx] = 1;
            else
                goal[c->p.y][c->p.x] = 1;
        }
    }
    else {
        Position o = c->s.players[c->owner].pos;
        int chased = 0;
        int intercept_range = c->n / 3;

        if (intercept_range < 3) intercept_range = 3;
        if (intercept_range > 6) intercept_range = 6;

        if (c->dex[o.y][o.x] <= intercept_range &&
            p1_post(c, o.x, o.y, 0, &gx, &gy, &gdir)) {
            goal[gy][gx] = 1;
            chased = 1;
        }

        if (!chased && p1_cheb(c->p.x, c->p.y, o.x, o.y) <= P1_CHASE) {
            int bx = o.x, by = o.y, bd = c->dex[o.y][o.x], x, y;

            for (d = 0; d < 4; ++d) {
                int nx = o.x + P1DX[d], ny = o.y + P1DY[d];

                if (p1_pass(c, nx, ny, 1) && c->dex[ny][nx] < bd) {
                    bd = c->dex[ny][nx];
                    bx = nx;
                    by = ny;
                }
            }

            for (y = 0; y < c->n; ++y) {
                for (x = 0; x < c->n; ++x) {
                    if (p1_cheb(x, y, bx, by) == 1 &&
                        p1_pass(c, x, y, 0) && !c->occ[y][x]) {
                        goal[y][x] = 1;
                        chased = 1;
                    }
                }
            }
        }

        if (!chased) {
            if (p1_post(c, o.x, o.y, 0, &gx, &gy, &gdir))
                goal[gy][gx] = 1;
            else
                goal[c->p.y][c->p.x] = 1;
        }
    }

    p1_field(c, goal, K);

    for (k = 0; k < 12; ++k) {
        int kind = k / 4, dir = k % 4;
        int nx = c->p.x, ny = c->p.y, score, v;

        if (kind == 3) break;

        if (kind == 0) {
            nx += P1DX[dir];
            ny += P1DY[dir];

            if (!p1_pass(c, nx, ny, 0) || c->occ[ny][nx]) continue;

            v = c->val[ny][nx];
            if (v >= P1_INF) v = 200000;

            score = -(10 + K * (c->danger[ny][nx] > 3 ? 3 : c->danger[ny][nx]) + v)
                - p1_imm(c, nx, ny)
                - p1_contest(c, nx, ny) * (c->hp == 1 ? 150 : 50);
        }
        else if (kind == 1) {
            v = c->val[c->p.y][c->p.x];
            if (v >= P1_INF) v = 200000;

            score = -(10 + K * (c->danger[c->p.y][c->p.x] > 3 ? 3 : c->danger[c->p.y][c->p.x]) + v)
                - p1_imm(c, c->p.x, c->p.y)
                + p1_attack_value(c, dir)
                + (c->p.x == gx && c->p.y == gy && dir == gdir ? 25 : 0);
        }
        else {
            if (c->owner != -1 ||
                c->p.x + P1DX[dir] != c->s.key.x ||
                c->p.y + P1DY[dir] != c->s.key.y)
                continue;

            score = (contested ? 500 : 1500) - p1_imm(c, c->p.x, c->p.y);
        }

        if (dir == tie_dir) ++score;

        if (score > best) {
            best = score;
            bestcmd = k;
        }
    }

    return (Command)bestcmd;
}

static Command p1_carry(P1Ctx* c)
{
    int cv[P1_M][P1_M], x, y, d, changed, allowed;
    int best = 2000000000, bestcmd = -1, bestsafe = -1, retreat = -1;
    int tie_dir = (c->p.x + 2 * c->p.y + c->me) & 3;

    for (y = 0; y < P1_M; ++y)
        for (x = 0; x < P1_M; ++x)
            cv[y][x] = P1_INF * 100;

    cv[c->ex.y][c->ex.x] = 0;

    do {
        changed = 0;

        for (y = 0; y < c->n; ++y) {
            for (x = 0; x < c->n; ++x) {
                int cost;

                if (cv[y][x] >= P1_INF * 100) continue;

                cost = cv[y][x] + 1 + (c->danger[y][x] > 0 ? 1000 : 0);

                for (d = 0; d < 4; ++d) {
                    int nx = x + P1DX[d], ny = y + P1DY[d];

                    if (!p1_pass(c, nx, ny, 1) || c->occ[ny][nx]) continue;

                    if (cost < cv[ny][nx]) {
                        cv[ny][nx] = cost;
                        changed = 1;
                    }
                }
            }
        }
    } while (changed);

    allowed = c->hp - 1;

    for (d = 0; d < 4; ++d) {
        int nx = c->p.x + P1DX[d], ny = c->p.y + P1DY[d];
        int total, D, key, safe, mind = 100, i;

        if (!p1_pass(c, nx, ny, 1) || c->occ[ny][nx] ||
            cv[ny][nx] >= P1_INF * 100)
            continue;

        total = 1 + (c->danger[ny][nx] > 0 ? 1000 : 0) + cv[ny][nx];
        D = total / 1000;

        key = (total % 1000) * 1000 + D * 2000 +
            p1_contest(c, nx, ny) * 500 + (d == tie_dir ? 0 : 1);

        if (D <= allowed && key < best) {
            best = key;
            bestcmd = d;
        }

        for (i = 0; i < PLAYER_COUNT; ++i) {
            if (p1_alive(c, i)) {
                int cd = p1_cheb(nx, ny,
                    c->s.players[i].pos.x, c->s.players[i].pos.y);

                if (cd < mind) mind = cd;
            }
        }

        safe = (c->danger[ny][nx] ? 0 : 100000)
            + (mind > 3 ? 3 : mind) * 1000
            - (cv[ny][nx] % 1000) * 300
            - p1_contest(c, nx, ny) * 5000;

        if (safe > bestsafe) {
            bestsafe = safe;
            retreat = d;
        }
    }

    if (bestcmd >= 0) return (Command)(COMMAND_UP + bestcmd);
    if (retreat >= 0) return (Command)(COMMAND_UP + retreat);

    for (d = 0; d < 4; ++d)
        if (p1_pass(c, c->p.x + P1DX[d], c->p.y + P1DY[d], 1))
            return (Command)(COMMAND_UP + d);

    return COMMAND_UP;
}

Command player1_choose_command(const char* state_path)
{
    P1Ctx c;
    FILE* file = fopen(state_path, "r");
    int ok, d;

    if (!file) return COMMAND_ATTACK_UP;

    ok = snapshot_read(file, &c.s);
    fclose(file);

    if (!ok || c.s.size < 3 || c.s.self < 0 || c.s.self >= PLAYER_COUNT)
        return COMMAND_ATTACK_UP;

    p1_init(&c);

    /* If a one-HP carrier near the exit is in another player's attack range,
       estimate the key-drop race. Move toward a recoverable pickup position
       instead of repeatedly attacking from outside our actual attack fan. */
    if (!c.carrier && c.owner >= 0 && c.owner < PLAYER_COUNT &&
        p1_alive(&c, c.owner) && c.s.players[c.owner].hp == 1 &&
        c.dex[c.s.players[c.owner].pos.y][c.s.players[c.owner].pos.x] <= 2) {
        Position holder = c.s.players[c.owner].pos;
        int drop_x = holder.x, drop_y = holder.y;
        int drop_risk = 0, best_drop_dist = P1_INF, can_hit_holder = 0;
        int my_eta, rival_eta = P1_INF, best_eta = P1_INF;
        int best_move = -1, best_score = P1_INF;
        int e, a, d;

        /* Choose the most exposed plausible end position as the drop point.
           This catches cases where the carrier steps into an attack fan first. */
        for (a = -1; a < 4; ++a) {
            int hx = holder.x, hy = holder.y, hits = 0, hd, exit_dist;
            if (a >= 0) {
                hx += P1DX[a];
                hy += P1DY[a];
                if (!p1_pass(&c, hx, hy, 1) ||
                    (c.occ[hy][hx] && (hx != holder.x || hy != holder.y)) ||
                    c.dex[hy][hx] >= c.dex[holder.y][holder.x])
                    continue;
            }

            for (e = 0; e < PLAYER_COUNT; ++e) {
                Position rival;
                int can_hit = 0;
                if (e == c.me || e == c.owner || !p1_alive(&c, e)) continue;
                rival = c.s.players[e].pos;
                for (hd = 0; hd < 4; ++hd)
                    if (p1_fan(rival.x, rival.y, hd, hx, hy)) can_hit = 1;
                if (can_hit) ++hits;
            }

            exit_dist = c.dex[hy][hx];
            if (hits > drop_risk ||
                (hits == drop_risk && hits > 0 &&
                    exit_dist < best_drop_dist)) {
                drop_risk = hits;
                best_drop_dist = exit_dist;
                drop_x = (hx == c.ex.x && hy == c.ex.y)
                    ? c.s.key_start.x : hx;
                drop_y = (hx == c.ex.x && hy == c.ex.y)
                    ? c.s.key_start.y : hy;
            }
        }

        my_eta = p1_pickup_eta(&c, c.p.x, c.p.y, drop_x, drop_y);

        for (e = 0; e < PLAYER_COUNT; ++e) {
            int eta;
            if (e == c.me || e == c.owner || !p1_alive(&c, e)) continue;
            eta = p1_pickup_eta(&c, c.s.players[e].pos.x,
                c.s.players[e].pos.y, drop_x, drop_y);
            if (eta < rival_eta) rival_eta = eta;
        }

        /* Preserve a direct or predicted hit on the carrier as first priority. */
        for (a = -1; a < 4; ++a) {
            int hx = holder.x, hy = holder.y;
            if (a >= 0) {
                hx += P1DX[a];
                hy += P1DY[a];
                if (!p1_pass(&c, hx, hy, 1) ||
                    (c.occ[hy][hx] && (hx != holder.x || hy != holder.y)) ||
                    c.dex[hy][hx] >= c.dex[holder.y][holder.x])
                    continue;
            }
            for (d = 0; d < 4; ++d) {
                if (p1_fan(c.p.x, c.p.y, d, hx, hy))
                    can_hit_holder = 1;
            }
        }

        if (drop_risk && !can_hit_holder && my_eta < P1_INF &&
            rival_eta <= my_eta + 2 && my_eta > 1) {
            for (d = 0; d < 4; ++d) {
                int nx = c.p.x + P1DX[d], ny = c.p.y + P1DY[d];
                int eta, score;

                if (!p1_pass(&c, nx, ny, 0) || c.occ[ny][nx]) continue;
                eta = p1_pickup_eta(&c, nx, ny, drop_x, drop_y);
                if (eta >= P1_INF) continue;

                score = eta * 1000
                    + p1_imm(&c, nx, ny) * (c.hp == 1 ? 6 : 2)
                    + p1_contest(&c, nx, ny) * 250;
                if (d == ((c.p.x + 2 * c.p.y + c.me) & 3)) --score;

                if (score < best_score) {
                    best_score = score;
                    best_eta = eta;
                    best_move = d;
                }
            }

            if (best_move >= 0 && best_eta <= my_eta)
                return (Command)(COMMAND_UP + best_move);
        }
    }

    if (!c.carrier && c.owner >= 0 && c.owner < PLAYER_COUNT &&
        p1_alive(&c, c.owner) && c.s.players[c.owner].hp >= 2 &&
        c.dex[c.s.players[c.owner].pos.y][c.s.players[c.owner].pos.x] <= 1) {

        int dstart[P1_M][P1_M];
        int best_route = P1_INF;
        int best_dir = -1;

        p1_bfs(&c, c.s.key_start.x, c.s.key_start.y, 0, dstart);

        for (d = 0; d < 4; ++d) {
            int nx = c.p.x + P1DX[d];
            int ny = c.p.y + P1DY[d];
            int route;

            if (!p1_pass(&c, nx, ny, 0) || c.occ[ny][nx] ||
                dstart[ny][nx] >= P1_INF)
                continue;

            route = dstart[ny][nx] * 1000
                + p1_imm(&c, nx, ny) * 4
                + p1_contest(&c, nx, ny) * 100;

            if (route < best_route) {
                best_route = route;
                best_dir = d;
            }
        }

        if (best_dir >= 0)
            return (Command)(COMMAND_UP + best_dir);
    }

    if (!c.carrier && c.owner >= 0 &&
        c.owner < PLAYER_COUNT && p1_alive(&c, c.owner)) {

        Position target = c.s.players[c.owner].pos;
        Position predicted[5];
        int weight[5], count = 0, total_weight = 0;
        int target_exit_distance = c.dex[target.y][target.x];
        int goal_x = -1, goal_y = -1, goal_dir = -1;
        int dGoal[P1_M][P1_M];
        int has_post, old_near = 100, best_score = -100000;
        int best_action = -1, tie_dir = (c.p.x + 2 * c.p.y + c.me) & 3;
        int i;

        if (target_exit_distance <= 4 &&
            !(c.s.players[c.owner].hp >= 2 && target_exit_distance <= 1)) {

            predicted[count] = target;
            weight[count++] = 1;

            for (d = 0; d < 4; ++d) {
                int nx = target.x + P1DX[d];
                int ny = target.y + P1DY[d];
                int next_distance, w;

                if (!p1_pass(&c, nx, ny, 1)) continue;

                next_distance = c.dex[ny][nx];
                if (next_distance >= P1_INF) continue;

                if (nx == c.ex.x && ny == c.ex.y)
                    w = target_exit_distance <= 2 ? 12 : 7;
                else if (next_distance < target_exit_distance)
                    w = 6;
                else if (next_distance == target_exit_distance)
                    w = 2;
                else
                    w = 1;

                if (c.occ[ny][nx] && (nx != c.p.x || ny != c.p.y))
                    w = 1;

                predicted[count].x = nx;
                predicted[count].y = ny;
                weight[count++] = w;
            }

            for (i = 0; i < count; ++i) {
                int cd = p1_cheb(c.p.x, c.p.y,
                    predicted[i].x, predicted[i].y);

                total_weight += weight[i];

                if (cd < old_near)
                    old_near = cd;
            }

            has_post = p1_post(&c, target.x, target.y, 0,
                &goal_x, &goal_y, &goal_dir);

            if (has_post)
                p1_bfs(&c, goal_x, goal_y, 0, dGoal);

            if (total_weight > 0) {
                for (d = 0; d < 4; ++d) {
                    int nx = c.p.x + P1DX[d];
                    int ny = c.p.y + P1DY[d];
                    int near_next = 100, ready_weight = 0, score;

                    if (!p1_pass(&c, nx, ny, 0) || c.occ[ny][nx])
                        continue;

                    for (i = 0; i < count; ++i) {
                        int cd = p1_cheb(nx, ny,
                            predicted[i].x, predicted[i].y);

                        if (cd < near_next)
                            near_next = cd;

                        if (cd == 1)
                            ready_weight += weight[i];
                    }

                    score = 0;

                    if (has_post && dGoal[c.p.y][c.p.x] < P1_INF &&
                        dGoal[ny][nx] < P1_INF) {
                        score += 70 * (dGoal[c.p.y][c.p.x] - dGoal[ny][nx]);
                    }

                    if (old_near < 100 && near_next < 100)
                        score += 18 * (old_near - near_next);

                    score += 90 * ready_weight / total_weight;
                    score -= (c.hp == 1) ? p1_imm(&c, nx, ny) / 8 : p1_imm(&c, nx, ny) / 2;
                    score -= p1_contest(&c, nx, ny) *
                        (c.hp == 1 ? 45 : 18);

                    if (d == tie_dir) ++score;

                    if (score > best_score) {
                        best_score = score;
                        best_action = d;
                    }
                }

                {
                    for (d = 0; d < 4; ++d) {
                        int covered = 0, score, side_bonus = 0;

                        for (i = 0; i < count; ++i) {
                            if (p1_fan(c.p.x, c.p.y, d,
                                predicted[i].x, predicted[i].y))
                                covered += weight[i];
                        }

                        /* When the carrier cannot exit now, punish a nearby rival
                           that moves into this fan. Starting two cells away keeps
                           the rival outside its attack range for this turn. */
                        if (target_exit_distance > 1) {
                            int e;
                            for (e = 0; e < PLAYER_COUNT; ++e) {
                                Position rival;
                                int old_exit, candidates = 0, hit_moves = 0;
                                int base;

                                if (e == c.me || e == c.owner || !p1_alive(&c, e))
                                    continue;

                                rival = c.s.players[e].pos;
                                {
                                    int rival_dist = p1_cheb(c.p.x, c.p.y,
                                        rival.x, rival.y);
                                    if (rival_dist > 2) continue;

                                    /* Direct hits on nearby non-carriers are
                                       useful too; discount them when low HP makes
                                       the exchange dangerous below. */
                                    if (rival_dist == 1 &&
                                        p1_fan(c.p.x, c.p.y, d,
                                            rival.x, rival.y))
                                        side_bonus += c.s.players[e].hp == 1
                                            ? 200 : 100;

                                    if (rival_dist != 2) continue;
                                }

                                old_exit = c.dex[rival.y][rival.x];
                                base = 240;
                                if (old_exit <= 4)
                                    base += 80;

                                for (i = 0; i < 4; ++i) {
                                    int rx = rival.x + P1DX[i];
                                    int ry = rival.y + P1DY[i];
                                    int approaches;

                                    if (!p1_pass(&c, rx, ry, 0) || c.occ[ry][rx])
                                        continue;

                                    approaches = c.dex[ry][rx] < old_exit ||
                                        p1_cheb(c.p.x, c.p.y, rx, ry) < 2;
                                    if (!approaches) continue;

                                    ++candidates;
                                    if (p1_fan(c.p.x, c.p.y, d, rx, ry))
                                        ++hit_moves;
                                }

                                if (candidates > 0)
                                    side_bonus += base * hit_moves / candidates;
                            }
                        }
                        if ((covered * 100 < total_weight * 25 && side_bonus < 50) ||
                            (c.hp == 1 && c.danger[c.p.y][c.p.x] > 0 &&
                                covered * 100 < total_weight * 60 && side_bonus < 120))
                            continue;

                        score = 150 * covered / total_weight + side_bonus;

                        if (c.s.players[c.owner].hp == 1) {
                            score += 180 * covered / total_weight;
                            if (target_exit_distance <= 2)
                                score += 180 * covered / total_weight;
                        }
                        else if (target_exit_distance <= 2) {
                            score += 90 * covered / total_weight;
                        }

                        if (!(c.hp == 1 && c.danger[c.p.y][c.p.x] > 0 &&
                            c.s.players[c.owner].hp == 1 &&
                            covered * 100 >= total_weight * 60))
                            score -= p1_imm(&c, c.p.x, c.p.y) /
                                (c.hp == 1 ? 1 : 2);

                        if (d == tie_dir) ++score;

                        if (score > best_score) {
                            best_score = score;
                            best_action = 4 + d;
                        }
                    }
                }
            }
        }

        if (best_action >= 0 && best_score >= 35) {
            if (best_action < 4)
                return (Command)(COMMAND_UP + best_action);

            return (Command)(COMMAND_ATTACK_UP + best_action - 4);
        }
    }

    return c.carrier ? p1_carry(&c) : p1_hunt(&c);
}
