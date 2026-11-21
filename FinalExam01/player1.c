
#include "player_api.h"

/* 표시할 이름을 이 문자열에 입력하세요 (UTF-8, 최대 63바이트). */
char player1_name[PLAYER_NAME_BYTES] = "Player1";

Command player1_choose_command(const char* state_path)
{
    Snapshot s;
    FILE* file = fopen(state_path, "r");

    int ok, n, me, px, py;
    int x, y, j, d, kind;
    int count;

    int dx[4] = { 0, 0, -1, 1 };
    int dy[4] = { -1, 1, 0, 0 };

    int alive[16], ex[16], ey[16], ehp[16];
    int occ[MAP_MAX][MAP_MAX];

    int dK[MAP_MAX][MAP_MAX];
    int dE[MAP_MAX][MAP_MAX];
    int dMe[MAP_MAX][MAP_MAX];
    int dA[MAP_MAX][MAP_MAX];
    int dT[MAP_MAX][MAP_MAX];
    int route[MAP_MAX][MAP_MAX];

    double goal[MAP_MAX][MAP_MAX];

    int holder, keyValid, keyFloor, myhp;
    int mode, found, contested;
    int tx = -1, ty = -1;

    int hcx[4], hcy[4], hcn = 0;

    unsigned int hv;

    double lf, wprog, pdie;
    double best, score;
    Command bestCmd;

    if (!file)
        return COMMAND_ATTACK_UP;

    ok = snapshot_read(file, &s);
    fclose(file);

    if (!ok)
        return COMMAND_ATTACK_UP;

    n = s.size;
    me = s.self;
    count = PLAYER_COUNT;

    if (n <= 0 || n > MAP_MAX ||
        me < 0 || me >= PLAYER_COUNT ||
        count <= 0 || count > 16)
        return COMMAND_ATTACK_UP;

    px = s.players[me].pos.x;
    py = s.players[me].pos.y;

    if (px < 0 || py < 0 || px >= n || py >= n)
        return COMMAND_ATTACK_UP;

#define P1_ABS(v) ((v) < 0 ? -(v) : (v))
#define P1_CHEB(ax,ay,bx,by) \
    (P1_ABS((ax)-(bx)) > P1_ABS((ay)-(by)) ? \
    P1_ABS((ax)-(bx)) : P1_ABS((ay)-(by)))

#define P1_WALL(xx,yy) \
    ((xx) < 0 || (yy) < 0 || (xx) >= n || (yy) >= n || \
    s.terrain[(yy)][(xx)] == '#')

#define P1_EXIT(xx,yy) \
    (((xx) >= 0 && (yy) >= 0 && (xx) < n && (yy) < n) && \
    (((xx) == s.exit.x && (yy) == s.exit.y) || \
    (s.terrain[(yy)][(xx)] == 'E')))

#define P1_ARC(ax,ay,dir,tx0,ty0) \
    ((dir) < 2 ? \
    ((ty0) == (ay) + dy[(dir)] && \
    P1_ABS((tx0) - (ax)) <= 1) : \
    ((tx0) == (ax) + dx[(dir)] && \
    P1_ABS((ty0) - (ay)) <= 1))

#define P1_ANY_ARC(ax,ay,tx0,ty0) \
    (P1_ARC((ax),(ay),0,(tx0),(ty0)) || \
     P1_ARC((ax),(ay),1,(tx0),(ty0)) || \
     P1_ARC((ax),(ay),2,(tx0),(ty0)) || \
     P1_ARC((ax),(ay),3,(tx0),(ty0)))

#define P1_BFS(SX,SY,DIST) do { \
    int qx[MAP_MAX * MAP_MAX]; \
    int qy[MAP_MAX * MAP_MAX]; \
    int qh = 0, qt = 0; \
    int qx0, qy0, qd; \
    for (qy0 = 0; qy0 < MAP_MAX; qy0++) \
        for (qx0 = 0; qx0 < MAP_MAX; qx0++) \
            (DIST)[qy0][qx0] = 99999; \
    if ((SX) >= 0 && (SY) >= 0 && (SX) < n && (SY) < n) { \
        (DIST)[(SY)][(SX)] = 0; \
        qx[qt] = (SX); qy[qt] = (SY); qt++; \
        while (qh < qt) { \
            int cx = qx[qh]; \
            int cy = qy[qh]; \
            qh++; \
            for (qd = 0; qd < 4; qd++) { \
                int nx = cx + dx[qd]; \
                int ny = cy + dy[qd]; \
                if (P1_WALL(nx,ny) || P1_EXIT(nx,ny)) continue; \
                if ((DIST)[ny][nx] != 99999) continue; \
                (DIST)[ny][nx] = (DIST)[cy][cx] + 1; \
                qx[qt] = nx; qy[qt] = ny; qt++; \
            } \
        } \
    } \
} while (0)

    for (y = 0; y < MAP_MAX; y++) {
        for (x = 0; x < MAP_MAX; x++) {
            occ[y][x] = 0;
            route[y][x] = 0;

            dK[y][x] = 99999;
            dE[y][x] = 99999;
            dMe[y][x] = 99999;
            dA[y][x] = 99999;
            dT[y][x] = 99999;

            goal[y][x] = 1.0e9;
        }
    }

    for (j = 0; j < count; j++) {
        ex[j] = s.players[j].pos.x;
        ey[j] = s.players[j].pos.y;
        ehp[j] = s.players[j].hp;

        alive[j] = (ehp[j] > 0 &&
            ex[j] >= 0 && ey[j] >= 0 &&
            ex[j] < n && ey[j] < n);

        if (j != me && alive[j])
            occ[ey[j]][ex[j]] = 1;
    }

    if (!alive[me])
        return COMMAND_UP;

    myhp = ehp[me];

    holder = -1;

    if (s.key_owner >= 0 &&
        s.key_owner < count &&
        alive[s.key_owner])
        holder = s.key_owner;

    keyValid = (s.key.x >= 0 && s.key.y >= 0 &&
        s.key.x < n && s.key.y < n);

    keyFloor = (holder < 0);

    hv = (unsigned int)s.turn * 73856093u ^
        (unsigned int)me * 19349663u ^
        (unsigned int)px * 83492791u ^
        (unsigned int)py * 2654435u;

    hv ^= hv >> 11;

    if (s.turn <= 80)
        lf = 1.0;
    else if (s.turn >= 160)
        lf = 2.0;
    else
        lf = 1.0 + (double)(s.turn - 80) / 80.0;

    if (keyFloor && keyValid)
        P1_BFS(s.key.x, s.key.y, dK);

    P1_BFS(s.exit.x, s.exit.y, dE);
    P1_BFS(px, py, dMe);

    mode = 0;
    found = 0;
    contested = 0;

    if (holder == me) {
        mode = 2;
    }
    else if (holder >= 0) {
        mode = 3;
    }
    else {
        int tMe = 99999;
        int faster = 0;
        int fastest = 99999;

        if (keyValid) {
            tMe = dK[py][px];

            if (tMe == 0)
                tMe = 2;
        }

        for (j = 0; j < count; j++) {
            int te;

            if (j == me || !alive[j] || !keyValid)
                continue;

            te = dK[ey[j]][ex[j]];

            if (te == 0)
                te = 2;

            if (te < tMe)
                faster++;

            if (te < fastest)
                fastest = te;

            if (dK[ey[j]][ex[j]] == 1)
                contested = 1;
        }

        if (faster == 0 || fastest >= 99999) {
            mode = 0;
        }
        else {
            int minRoute = 99999;
            int haveOK = 0, haveAny = 0;

            int okX = 0, okY = 0;
            int okA = -1, okM = 99999;

            int anyX = 0, anyY = 0;
            int anyGap = 99999, anyA = -1;

            int base = fastest - 1;
            int minDA = 2;

            for (d = 0; d < 4; d++) {
                int ax = s.exit.x + dx[d];
                int ay = s.exit.y + dy[d];

                if (P1_WALL(ax, ay) || P1_EXIT(ax, ay))
                    continue;

                if (dK[ay][ax] < 99999 &&
                    dK[ay][ax] + 1 < minRoute)
                    minRoute = dK[ay][ax] + 1;
            }

            if (minRoute < 99999) {
                for (y = 0; y < n; y++) {
                    for (x = 0; x < n; x++) {
                        int t, gap;

                        if (P1_WALL(x, y) || P1_EXIT(x, y))
                            continue;

                        if (dK[y][x] >= 99999 ||
                            dE[y][x] >= 99999)
                            continue;

                        if (dK[y][x] + dE[y][x] > minRoute + 2)
                            continue;

                        route[y][x] = 1;

                        if (dK[y][x] < minDA ||
                            dMe[y][x] >= 99999)
                            continue;

                        t = base + dK[y][x];
                        gap = dMe[y][x] - t;

                        if (dMe[y][x] <= t - 1) {
                            if (!haveOK ||
                                dK[y][x] > okA ||
                                (dK[y][x] == okA &&
                                    dMe[y][x] < okM)) {
                                haveOK = 1;
                                okX = x;
                                okY = y;
                                okA = dK[y][x];
                                okM = dMe[y][x];
                            }
                        }
                        else {
                            if (!haveAny ||
                                gap < anyGap ||
                                (gap == anyGap &&
                                    dK[y][x] > anyA)) {
                                haveAny = 1;
                                anyX = x;
                                anyY = y;
                                anyGap = gap;
                                anyA = dK[y][x];
                            }
                        }
                    }
                }
            }

            if (haveOK) {
                tx = okX;
                ty = okY;
                found = 1;
            }
            else if (haveAny) {
                tx = anyX;
                ty = anyY;
                found = 1;
            }

            if (!found)
                mode = 0;
            else
                mode = 1;
        }
    }

    if (mode == 3) {
        int hx = ex[holder];
        int hy = ey[holder];
        int cur;

        P1_BFS(hx, hy, dA);

        {
            int minRoute = 99999;
            int haveOK = 0, haveAny = 0;

            int okX = 0, okY = 0;
            int okA = -1, okM = 99999;

            int anyX = 0, anyY = 0;
            int anyGap = 99999, anyA = -1;

            for (d = 0; d < 4; d++) {
                int ax = s.exit.x + dx[d];
                int ay = s.exit.y + dy[d];

                if (P1_WALL(ax, ay) || P1_EXIT(ax, ay))
                    continue;

                if (dA[ay][ax] < 99999 &&
                    dA[ay][ax] + 1 < minRoute)
                    minRoute = dA[ay][ax] + 1;
            }

            if (minRoute < 99999) {
                for (y = 0; y < n; y++) {
                    for (x = 0; x < n; x++) {
                        int gap;

                        if (P1_WALL(x, y) || P1_EXIT(x, y))
                            continue;

                        if (dA[y][x] >= 99999 ||
                            dE[y][x] >= 99999)
                            continue;

                        if (dA[y][x] + dE[y][x] > minRoute + 2)
                            continue;

                        route[y][x] = 1;

                        if (dA[y][x] < 1 ||
                            dMe[y][x] >= 99999)
                            continue;

                        gap = dMe[y][x] - dA[y][x];

                        if (dMe[y][x] <= dA[y][x] - 1) {
                            if (!haveOK ||
                                dA[y][x] > okA ||
                                (dA[y][x] == okA &&
                                    dMe[y][x] < okM)) {
                                haveOK = 1;
                                okX = x;
                                okY = y;
                                okA = dA[y][x];
                                okM = dMe[y][x];
                            }
                        }
                        else {
                            if (!haveAny ||
                                gap < anyGap ||
                                (gap == anyGap &&
                                    dA[y][x] > anyA)) {
                                haveAny = 1;
                                anyX = x;
                                anyY = y;
                                anyGap = gap;
                                anyA = dA[y][x];
                            }
                        }
                    }
                }
            }

            if (haveOK) {
                tx = okX;
                ty = okY;
                found = 1;
            }
            else if (haveAny) {
                tx = anyX;
                ty = anyY;
                found = 1;
            }
            else {
                found = 0;
            }
        }

        cur = dE[hy][hx];

        for (d = 0; d < 4; d++) {
            int nx = hx + dx[d];
            int ny = hy + dy[d];

            if (P1_WALL(nx, ny))
                continue;

            if (occ[ny][nx] || (nx == px && ny == py))
                continue;

            if (P1_EXIT(nx, ny)) {
                hcn = 1;
                hcx[0] = nx;
                hcy[0] = ny;
                break;
            }

            if (dE[ny][nx] < cur) {
                hcx[hcn] = nx;
                hcy[hcn] = ny;
                hcn++;
            }
        }

        if (hcn == 0) {
            int bE = 99999;

            for (d = 0; d < 4; d++) {
                int nx = hx + dx[d];
                int ny = hy + dy[d];

                if (P1_WALL(nx, ny) || P1_EXIT(nx, ny))
                    continue;

                if (occ[ny][nx] || (nx == px && ny == py))
                    continue;

                if (dE[ny][nx] < bE) {
                    bE = dE[ny][nx];
                    hcx[0] = nx;
                    hcy[0] = ny;
                    hcn = 1;
                }
            }
        }
    }

    if (mode == 0) {
        for (y = 0; y < n; y++) {
            for (x = 0; x < n; x++) {
                if (dK[y][x] < 99999)
                    goal[y][x] = (dK[y][x] == 0) ? 2.0 : (double)dK[y][x];
            }
        }
        wprog = 12.0;
    }
    else if (mode == 1) {
        P1_BFS(tx, ty, dT);

        for (y = 0; y < n; y++) {
            for (x = 0; x < n; x++) {
                if (dT[y][x] < 99999)
                    goal[y][x] = (double)dT[y][x];
            }
        }

        wprog = 10.0;
    }
    else if (mode == 2) {
        double cost[MAP_MAX][MAP_MAX];
        int inq[MAP_MAX][MAP_MAX];

        int qx[MAP_MAX * MAP_MAX + 1];
        int qy[MAP_MAX * MAP_MAX + 1];

        int cap = MAP_MAX * MAP_MAX + 1;
        int head = 0, tail = 0;

        double ring = (myhp >= 2) ? 5.0 : 12.0;

        for (y = 0; y < MAP_MAX; y++) {
            for (x = 0; x < MAP_MAX; x++) {
                goal[y][x] = 1.0e9;
                cost[y][x] = 1.0;
                inq[y][x] = 0;
            }
        }

        for (y = 0; y < n; y++) {
            for (x = 0; x < n; x++) {
                double c = 1.0;

                for (j = 0; j < count; j++) {
                    int cd;

                    if (j == me || !alive[j])
                        continue;

                    cd = P1_CHEB(x, y, ex[j], ey[j]);

                    if (cd == 0)
                        c += 12.0;
                    else if (cd == 1)
                        c += ring;
                    else if (cd == 2)
                        c += 1.5;
                }

                cost[y][x] = c;
            }
        }

        if (!P1_WALL(s.exit.x, s.exit.y)) {
            goal[s.exit.y][s.exit.x] = 0.0;

            qx[tail] = s.exit.x;
            qy[tail] = s.exit.y;
            tail = (tail + 1) % cap;

            inq[s.exit.y][s.exit.x] = 1;

            while (head != tail) {
                int cx = qx[head];
                int cy = qy[head];

                head = (head + 1) % cap;
                inq[cy][cx] = 0;

                for (d = 0; d < 4; d++) {
                    int nx = cx + dx[d];
                    int ny = cy + dy[d];
                    double nd;

                    if (P1_WALL(nx, ny) || P1_EXIT(nx, ny))
                        continue;

                    nd = goal[cy][cx] + cost[cy][cx];

                    if (nd < goal[ny][nx]) {
                        goal[ny][nx] = nd;

                        if (!inq[ny][nx]) {
                            inq[ny][nx] = 1;

                            qx[tail] = nx;
                            qy[tail] = ny;

                            tail = (tail + 1) % cap;
                        }
                    }
                }
            }
        }

        wprog = 15.0;
    }
    else {
        if (found) {
            P1_BFS(tx, ty, dT);

            for (y = 0; y < n; y++) {
                for (x = 0; x < n; x++) {
                    if (dT[y][x] < 99999)
                        goal[y][x] = (double)dT[y][x];
                }
            }
        }
        else {
            for (y = 0; y < n; y++) {
                for (x = 0; x < n; x++) {
                    if (dA[y][x] < 99999)
                        goal[y][x] = (double)dA[y][x];
                }
            }
        }

        wprog = (ehp[holder] <= 1) ? 28.0 : 16.0;
    }

    wprog *= lf;

    pdie = 0.0;

    if (mode == 2 && myhp <= 1) {
        double keep = 1.0;

        for (j = 0; j < count; j++) {
            if (j == me || !alive[j])
                continue;

            if (P1_ANY_ARC(ex[j], ey[j], s.exit.x, s.exit.y))
                keep *= 0.5;
        }

        pdie = 1.0 - keep;
    }

    best = -1.0e18;

    bestCmd = (Command)(
        COMMAND_ATTACK_UP + (int)((hv >> 3) % 4)
        );

    for (kind = 0; kind < 3; kind++) {
        for (d = 0; d < 4; d++) {
            int nx = px;
            int ny = py;
            int valid = 1;
            int winMove = 0;
            int attackExpected = 0;

            double pnh = 1.0;
            double dmg;

            score = 0.0;

            if (kind == 0) {
                nx = px + dx[d];
                ny = py + dy[d];

                if (P1_WALL(nx, ny)) {
                    valid = 0;
                }
                else if (P1_EXIT(nx, ny)) {
                    if (mode == 2)
                        winMove = 1;
                    else
                        valid = 0;
                }

                if (valid && occ[ny][nx])
                    valid = 0;
            }

            else if (kind == 1) {
                if (holder == me)
                    valid = 0;
            }

            else {
                int kx = px + dx[d];
                int ky = py + dy[d];

                if (!(keyFloor && keyValid && holder < 0 &&
                    kx == s.key.x && ky == s.key.y))
                    valid = 0;
            }

            if (!valid)
                continue;

            if (winMove) {
                score = 100000.0 * (1.0 - pdie);
            }
            else {
                if (kind == 0) {
                    score -= 1.5;

                    if (goal[py][px] < 5.0e8 &&
                        goal[ny][nx] < 5.0e8) {
                        score += wprog *
                            (goal[py][px] - goal[ny][nx]);
                    }

                    if ((mode == 1 || mode == 3) && route[ny][nx]) {
                        score += (mode == 3) ? 22.0 : 18.0;
                        if (dE[ny][nx] < dE[py][px]) score += 8.0;
                    }

                    if (mode == 3 && hcn > 0) {
                        int qq, nearNow = 99999, nearNext = 99999;
                        int canAttackNext = 0;
                        for (qq = 0; qq < hcn; qq++) {
                            int oldDist = P1_CHEB(px, py, hcx[qq], hcy[qq]);
                            int newDist = P1_CHEB(nx, ny, hcx[qq], hcy[qq]);
                            if (oldDist < nearNow) nearNow = oldDist;
                            if (newDist < nearNext) nearNext = newDist;
                            if (P1_ANY_ARC(nx, ny, hcx[qq], hcy[qq])) canAttackNext = 1;
                        }
                        if (nearNow < 99999 && nearNext < 99999)
                            score += (double)(nearNow - nearNext) * 42.0;
                        if (canAttackNext) score += 85.0;

                        {
                            int oldHolderDist =
                                P1_CHEB(px, py, ex[holder], ey[holder]);
                            int newHolderDist =
                                P1_CHEB(nx, ny, ex[holder], ey[holder]);

                            score += (double)(oldHolderDist - newHolderDist) * 34.0;

                            if (P1_ANY_ARC(nx, ny, ex[holder], ey[holder]))
                                score += 55.0;
                        }
                    }
                }

                if (kind == 2) {
                    score += contested ? 90.0 : 220.0;
                }

                {
                    for (j = 0; j < count; j++) {
                        double p = 0.0;

                        if (j == me || !alive[j])
                            continue;

                        if (P1_ANY_ARC(ex[j], ey[j], nx, ny))
                            p = (j == holder) ? 0.30 : 0.50;

                        if (kind == 0 &&
                            P1_ABS(nx - ex[j]) +
                            P1_ABS(ny - ey[j]) == 1) {
                            p = 1.0 - (1.0 - p) * 0.80;
                        }

                        pnh *= (1.0 - p);
                    }

                    dmg = (myhp >= 2) ? 45.0 : 180.0;
                    if (mode == 2)
                        dmg *= 1.5;
                    else if (mode == 3)
                        dmg = (myhp >= 2) ? 35.0 : 85.0;

                    score -= dmg * (1.0 - pnh) / lf;
                }

                if (kind == 1) {
                    for (j = 0; j < count; j++) {
                        double val = 0.0;
                        double f, base, frac = 0.0;
                        int hitNow;

                        if (j == me || !alive[j])
                            continue;

                        hitNow = P1_ARC(
                            px, py, d, ex[j], ey[j]
                        );

                        if (j == holder && hcn > 0) {
                            int cnt = 0;
                            int qq;

                            for (qq = 0; qq < hcn; qq++) {
                                if (P1_ARC(px, py, d, hcx[qq], hcy[qq]))
                                    cnt++;
                            }

                            frac = (double)cnt / (double)hcn;
                        }
                        else if (mode == 1 && keyFloor && keyValid) {
                            int cnt = 0, total = 0, qq;
                            for (qq = 0; qq < 4; qq++) {
                                int ax = ex[j] + dx[qq];
                                int ay = ey[j] + dy[qq];
                                if (P1_WALL(ax, ay) || P1_EXIT(ax, ay))
                                    continue;
                                if (occ[ay][ax] && !(ax == px && ay == py))
                                    continue;
                                if (dK[ay][ax] < dK[ey[j]][ex[j]] ||
                                    route[ay][ax]) {
                                    total++;
                                    if (P1_ARC(px, py, d, ax, ay))
                                        cnt++;
                                }
                            }
                            if (total > 0)
                                frac = 0.35 * (double)cnt / (double)total;
                        }

                        if (!hitNow && frac <= 0.0)
                            continue;

                        attackExpected = 1;

                        f = 1.0 + 0.5 *
                            (double)(myhp - ehp[j]);
                        if (f < 0.4) f = 0.4;
                        if (f > 1.6) f = 1.6;

                        if (hitNow) {
                            base = (ehp[j] <= 1) ? 150.0 : 105.0;

                            if (j == holder)
                                base += (ehp[j] <= 1) ? 420.0 : 360.0;
                            else if (mode == 1 && keyFloor && keyValid &&
                                dK[ey[j]][ex[j]] < 99999) {
                                base += (dK[ey[j]][ex[j]] <= 2) ? 95.0 : 45.0;
                            }

                            val += base * f;
                        }

                        if (frac > 0.0) {
                            double predictedBase = (ehp[j] <= 1) ? 90.0 : 55.0;
                            if (j == holder)
                                predictedBase += 320.0;
                            val += predictedBase * f * frac;
                        }

                        if (mode == 2)
                            val *= 0.3;

                        score += val;
                    }

                    if (!attackExpected) {
                        score -= 10000.0;
                    }

                }
            }

            score += (double)(
                (hv + (unsigned int)(kind * 5 + d * 3)) % 7u
                ) * 0.05;

            if (score > best) {
                best = score;

                if (kind == 0)
                    bestCmd = (Command)(COMMAND_UP + d);
                else if (kind == 1)
                    bestCmd = (Command)(COMMAND_ATTACK_UP + d);
                else
                    bestCmd = (Command)(COMMAND_PICK_UP + d);
            }
        }
    }

    if (best < -1.0e17 && holder == me)
        return COMMAND_UP;

#undef P1_ABS
#undef P1_CHEB
#undef P1_WALL
#undef P1_EXIT
#undef P1_ARC
#undef P1_ANY_ARC
#undef P1_BFS

    return bestCmd;
}
