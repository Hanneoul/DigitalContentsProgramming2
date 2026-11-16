#include "player_api.h"

/*
 * P4 independent AI.
 * The assignment permits changing only player4_choose_command(), so every
 * decision and every search is implemented locally inside this one function.
 * No shared AI-header dependency, helper function, global variable, or other source
 * file is used.
 */
Command player4_choose_command(const char *state_path)
{
    Snapshot s;
    FILE *fp;
    const int dx[4] = { 0, 0, -1, 1 };
    const int dy[4] = { -1, 1, 0, 0 };
    const int INF = 1000000000;
    unsigned char active[PLAYER_COUNT] = { 0 };
    unsigned char occupied[MAP_MAX][MAP_MAX] = { 0 };
    unsigned char danger[MAP_MAX][MAP_MAX] = { 0 };
    unsigned char attackZone[MAP_MAX][MAP_MAX] = { 0 };
    unsigned char visited[MAP_MAX][MAP_MAX] = { 0 };
    unsigned char route[MAP_MAX][MAP_MAX] = { 0 };
    int pathCost[MAP_MAX][MAP_MAX];
    int firstMove[MAP_MAX][MAP_MAX];
    int bfsDist[MAP_MAX][MAP_MAX];
    int prevX[MAP_MAX][MAP_MAX];
    int prevY[MAP_MAX][MAP_MAX];
    int queueX[MAP_MAX * MAP_MAX];
    int queueY[MAP_MAX * MAP_MAX];
    Position me, spawn;
    int i, x, y, d, finished = 0, carrying;
    int safeDirection = -1;

    if (state_path == NULL) return COMMAND_ATTACK_UP;
    fp = fopen(state_path, "r");
    if (fp == NULL) return COMMAND_ATTACK_UP;
    if (!snapshot_read(fp, &s)) {
        fclose(fp);
        return COMMAND_ATTACK_UP;
    }
    fclose(fp);

    if (s.size < 3 || s.size > MAP_MAX || s.self < 0 || s.self >= PLAYER_COUNT)
        return COMMAND_ATTACK_UP;

    me = s.players[s.self].pos;
    spawn = s.players[s.self].spawn;
    carrying = (s.key_owner == s.self);

    for (i = 0; i < PLAYER_COUNT; ++i) {
        if (s.players[i].rank > 0) ++finished;
        if (i != s.self && s.players[i].hp > 0 && s.players[i].rank == 0) {
            int ex = s.players[i].pos.x;
            int ey = s.players[i].pos.y;
            if (ex >= 0 && ey >= 0 && ex < s.size && ey < s.size) {
                active[i] = 1;
                occupied[ey][ex] = 1;
            }
        }
    }

    /* Build a local threat map from the current public snapshot. */
    for (y = 0; y < s.size; ++y) {
        for (x = 0; x < s.size; ++x) {
            int dangerValue = 0;
            for (i = 0; i < PLAYER_COUNT; ++i) {
                int ax, ay;
                /* A key carrier cannot attack this turn, so do not treat that
                   player as an immediate attack source. */
                if (!active[i] || i == s.key_owner) continue;
                ax = abs(x - s.players[i].pos.x);
                ay = abs(y - s.players[i].pos.y);
                if (ax <= 1 && ay <= 1) {
                    dangerValue += 3;
                    attackZone[y][x] = 1;
                } else if (ax + ay <= 3) {
                    ++dangerValue;
                }
            }
            danger[y][x] = (unsigned char)dangerValue;
        }
    }

    /* Prefer an empty attack fan as a stationary WAIT; if all fans contain an
       enemy, use UP as the deterministic fallback. */
    for (d = 0; d < 4; ++d) {
        int hits = 0;
        for (i = 0; i < PLAYER_COUNT; ++i) {
            int ax, ay;
            if (!active[i]) continue;
            ax = s.players[i].pos.x - me.x;
            ay = s.players[i].pos.y - me.y;
            if ((d == 0 && ay == -1 && abs(ax) <= 1) ||
                (d == 1 && ay == 1 && abs(ax) <= 1) ||
                (d == 2 && ax == -1 && abs(ay) <= 1) ||
                (d == 3 && ax == 1 && abs(ay) <= 1)) {
                hits = 1;
                break;
            }
        }
        if (!hits) {
            safeDirection = d;
            break;
        }
    }
    if (safeDirection < 0) safeDirection = 0;

    /*
     * TACTICAL RECOVERY PHASE
     * A deliberate P4 drop is inferred from the visible state because each
     * player's process restarts every turn. Fight only while the key is safe,
     * P4 is at full HP, and a more-deaths exit-side opponent is still present.
     */
    if (!carrying && s.key_owner == -1 && finished == 1 &&
        s.players[s.self].hp == MAX_HP &&
        (s.key.x != s.key_start.x || s.key.y != s.key_start.y) &&
        abs(s.key.x - spawn.x) + abs(s.key.y - spawn.y) <=
            abs(s.key.x - s.exit.x) + abs(s.key.y - s.exit.y) &&
        abs(me.x - s.key.x) + abs(me.y - s.key.y) <= 8) {
        int keyThreatened = 0;
        int myKeyDistance = abs(me.x - s.key.x) + abs(me.y - s.key.y);
        int target = -1;
        int targetScore = -INF;

        for (i = 0; i < PLAYER_COUNT; ++i) {
            int enemyDistance;
            if (!active[i]) continue;
            enemyDistance = abs(s.players[i].pos.x - s.key.x) +
                            abs(s.players[i].pos.y - s.key.y);
            if (enemyDistance <= 2 || enemyDistance + 1 < myKeyDistance)
                keyThreatened = 1;
        }

        if (!keyThreatened) {
            for (i = 0; i < PLAYER_COUNT; ++i) {
                int toExit, score, deathGap;
                if (!active[i] || s.players[i].deaths <= s.players[s.self].deaths)
                    continue;
                toExit = abs(s.players[i].pos.x - s.exit.x) +
                         abs(s.players[i].pos.y - s.exit.y);
                deathGap = (int)(s.players[i].deaths - s.players[s.self].deaths);
                if (toExit > 5) continue;
                /* Prefer a one-hit target; accept a two-HP target only with a
                   larger respawn disadvantage, avoiding reckless duels. */
                if (s.players[i].hp > 1 && deathGap < 2) continue;
                score = 40 - 6 * toExit + 3 * deathGap;
                if (s.players[i].hp == 1) score += 10;
                if (score > targetScore) {
                    targetScore = score;
                    target = i;
                }
            }
        }

        if (target >= 0) {
            int attackDirection = -1;
            int bestFirst = -1;
            int bestCost = INF;
            Position enemy = s.players[target].pos;

            for (d = 0; d < 4; ++d) {
                int ax = enemy.x - me.x;
                int ay = enemy.y - me.y;
                int hit = (d == 0 && ay == -1 && abs(ax) <= 1) ||
                          (d == 1 && ay == 1 && abs(ax) <= 1) ||
                          (d == 2 && ax == -1 && abs(ay) <= 1) ||
                          (d == 3 && ax == 1 && abs(ay) <= 1);
                if (hit) {
                    attackDirection = d;
                    break;
                }
            }
            if (attackDirection >= 0)
                return (Command)(COMMAND_ATTACK_UP + attackDirection);

            /* Dijkstra to the cheapest currently reachable attack tile. */
            for (y = 0; y < s.size; ++y) {
                for (x = 0; x < s.size; ++x) {
                    pathCost[y][x] = INF;
                    firstMove[y][x] = -1;
                    visited[y][x] = 0;
                }
            }
            if (me.x >= 0 && me.y >= 0 && me.x < s.size && me.y < s.size)
                pathCost[me.y][me.x] = 0;

            for (;;) {
                int bx = -1, by = -1, best = INF;
                for (y = 0; y < s.size; ++y)
                    for (x = 0; x < s.size; ++x)
                        if (!visited[y][x] && pathCost[y][x] < best) {
                            best = pathCost[y][x]; bx = x; by = y;
                        }
                if (bx < 0) break;
                visited[by][bx] = 1;
                for (i = 0; i < 4; ++i) {
                    int nd = (i + s.self + s.turn) % 4;
                    int nx = bx + dx[nd], ny = by + dy[nd];
                    int nextCost;
                    if (nx < 0 || ny < 0 || nx >= s.size || ny >= s.size) continue;
                    if (s.terrain[ny][nx] == '#' || s.terrain[ny][nx] == 'E') continue;
                    if (occupied[ny][nx] || visited[ny][nx]) continue;
                    nextCost = best + 10 + danger[ny][nx];
                    if (nextCost < pathCost[ny][nx]) {
                        pathCost[ny][nx] = nextCost;
                        firstMove[ny][nx] = (bx == me.x && by == me.y) ? nd : firstMove[by][bx];
                    }
                }
            }

            for (y = enemy.y - 1; y <= enemy.y + 1; ++y) {
                for (x = enemy.x - 1; x <= enemy.x + 1; ++x) {
                    int ax, ay;
                    if (x < 0 || y < 0 || x >= s.size || y >= s.size) continue;
                    if (x == enemy.x && y == enemy.y) continue;
                    if (s.terrain[y][x] == '#' || s.terrain[y][x] == 'E' || occupied[y][x]) continue;
                    ax = enemy.x - x;
                    ay = enemy.y - y;
                    if (!(abs(ax) <= 1 && abs(ay) <= 1)) continue;
                    if (pathCost[y][x] < bestCost) {
                        bestCost = pathCost[y][x];
                        bestFirst = firstMove[y][x];
                    }
                }
            }
            if (bestFirst >= 0 && bestFirst < 4) return (Command)bestFirst;
        }
    }

    /*
     * CARRYING THE KEY: compare the direct route with a route that treats
     * occupied enemies as blocked. A strong exit-side blocker can justify a
     * carefully selected adjacent key drop and a counterattack.
     */
    if (carrying && finished == 1 && me.x >= 0 && me.y >= 0 &&
        me.x < s.size && me.y < s.size && s.exit.x >= 0 && s.exit.y >= 0 &&
        s.exit.x < s.size && s.exit.y < s.size) {
        int directLength, detourLength;
        int bestBlocker = -1, bestBlockerScore = 0, detourBonus = 0;

        for (y = 0; y < s.size; ++y) {
            for (x = 0; x < s.size; ++x) {
                bfsDist[y][x] = -1;
                prevX[y][x] = -1;
                prevY[y][x] = -1;
                route[y][x] = 0;
            }
        }
        {
            int head = 0, tail = 0;
            queueX[tail] = me.x; queueY[tail] = me.y; ++tail;
            bfsDist[me.y][me.x] = 0;
            while (head < tail) {
                int bx = queueX[head], by = queueY[head]; ++head;
                for (d = 0; d < 4; ++d) {
                    int nx = bx + dx[d], ny = by + dy[d];
                    if (nx < 0 || ny < 0 || nx >= s.size || ny >= s.size) continue;
                    if (s.terrain[ny][nx] == '#' || bfsDist[ny][nx] >= 0) continue;
                    bfsDist[ny][nx] = bfsDist[by][bx] + 1;
                    prevX[ny][nx] = bx; prevY[ny][nx] = by;
                    queueX[tail] = nx; queueY[tail] = ny; ++tail;
                }
            }
        }
        directLength = bfsDist[s.exit.y][s.exit.x];
        if (directLength >= 0) {
            x = s.exit.x; y = s.exit.y;
            route[y][x] = 1;
            while (x != me.x || y != me.y) {
                int px = prevX[y][x], py = prevY[y][x];
                if (px < 0 || py < 0) break;
                x = px; y = py; route[y][x] = 1;
            }

            for (y = 0; y < s.size; ++y)
                for (x = 0; x < s.size; ++x) bfsDist[y][x] = -1;
            {
                int head = 0, tail = 0;
                queueX[tail] = me.x; queueY[tail] = me.y; ++tail;
                bfsDist[me.y][me.x] = 0;
                while (head < tail) {
                    int bx = queueX[head], by = queueY[head]; ++head;
                    for (d = 0; d < 4; ++d) {
                        int nx = bx + dx[d], ny = by + dy[d];
                        if (nx < 0 || ny < 0 || nx >= s.size || ny >= s.size) continue;
                        if (s.terrain[ny][nx] == '#' || occupied[ny][nx] || bfsDist[ny][nx] >= 0) continue;
                        bfsDist[ny][nx] = bfsDist[by][bx] + 1;
                        queueX[tail] = nx; queueY[tail] = ny; ++tail;
                    }
                }
            }
            detourLength = bfsDist[s.exit.y][s.exit.x];
            if (detourLength < 0) detourBonus = 30;
            else if (detourLength - directLength >= 3) detourBonus = 25;
            else if (detourLength > directLength) detourBonus = 10;

            for (i = 0; i < PLAYER_COUNT; ++i) {
                int toExit, onRoute = 0, score;
                Position enemy;
                if (!active[i] || s.players[i].deaths <= s.players[s.self].deaths) continue;
                enemy = s.players[i].pos;
                toExit = abs(enemy.x - s.exit.x) + abs(enemy.y - s.exit.y);
                if (toExit > 5) continue;
                if (route[enemy.y][enemy.x]) onRoute = 2;
                else {
                    for (y = 0; y < s.size && !onRoute; ++y)
                        for (x = 0; x < s.size; ++x)
                            if (route[y][x] && abs(x - enemy.x) + abs(y - enemy.y) == 1) {
                                onRoute = 1; break;
                            }
                }
                score = 0;
                if (onRoute == 2) score += 45;
                else if (onRoute == 1) score += 20;
                if (toExit == 1) score += 30;
                else if (toExit == 2) score += 15;
                else if (toExit <= 4) score += 6;
                if (s.players[i].hp == 1) score += 5;
                if (score > bestBlockerScore) {
                    bestBlockerScore = score;
                    bestBlocker = i;
                }
            }
        }

        if (bestBlocker >= 0 && bestBlockerScore + detourBonus >= 55 &&
            abs(s.players[bestBlocker].pos.x - s.exit.x) +
            abs(s.players[bestBlocker].pos.y - s.exit.y) <= 5 &&
            s.players[s.self].hp == MAX_HP) {
            int combatTarget = bestBlocker;
            int deathGap = (int)(s.players[bestBlocker].deaths - s.players[s.self].deaths);
            /* Attack the detected blocker itself, not an unrelated opponent. */
            if (s.players[combatTarget].hp > 1 && deathGap < 2)
                combatTarget = -1;

            if (combatTarget >= 0) {
                int bestDropDirection = -1, bestDropScore = -INF;
                for (d = 0; d < 4; ++d) {
                    int nx = me.x + dx[d], ny = me.y + dy[d];
                    int minEnemy = 1000, toSpawn, toExit, score;
                    if (nx < 0 || ny < 0 || nx >= s.size || ny >= s.size) continue;
                    if (s.terrain[ny][nx] != ' ' || occupied[ny][nx]) continue;
                    for (i = 0; i < PLAYER_COUNT; ++i) {
                        int md;
                        if (!active[i]) continue;
                        md = abs(nx - s.players[i].pos.x) + abs(ny - s.players[i].pos.y);
                        if (md < minEnemy) minEnemy = md;
                    }
                    if (minEnemy <= 2) continue;
                    toSpawn = abs(nx - spawn.x) + abs(ny - spawn.y);
                    toExit = abs(nx - s.exit.x) + abs(ny - s.exit.y);
                    if (toSpawn > toExit) continue;
                    score = 5 * toExit - 3 * toSpawn + 2 * minEnemy;
                    for (i = 0; i < PLAYER_COUNT; ++i) {
                        int ax, ay;
                        if (!active[i]) continue;
                        ax = nx - s.players[i].pos.x;
                        ay = ny - s.players[i].pos.y;
                        if ((ay == -1 && abs(ax) <= 1) ||
                            (ay == 1 && abs(ax) <= 1) ||
                            (ax == -1 && abs(ay) <= 1) ||
                            (ax == 1 && abs(ay) <= 1)) score -= 12;
                    }
                    if (score > bestDropScore) {
                        bestDropScore = score;
                        bestDropDirection = d;
                    }
                }
                if (bestDropDirection >= 0)
                    return (Command)(COMMAND_DROP_UP + bestDropDirection);
            }
        }
    }

    /* CARRYING KEY: Dijkstra to the exit, avoiding occupied enemies. */
    if (carrying) {
        int bestMove = -1;
        int dangerWeight = finished >= 1 ? 1 : 3;
        for (y = 0; y < s.size; ++y) {
            for (x = 0; x < s.size; ++x) {
                pathCost[y][x] = INF;
                firstMove[y][x] = -1;
                visited[y][x] = 0;
            }
        }
        if (me.x >= 0 && me.y >= 0 && me.x < s.size && me.y < s.size)
            pathCost[me.y][me.x] = 0;

        for (;;) {
            int bx = -1, by = -1, best = INF;
            for (y = 0; y < s.size; ++y)
                for (x = 0; x < s.size; ++x)
                    if (!visited[y][x] && pathCost[y][x] < best) {
                        best = pathCost[y][x]; bx = x; by = y;
                    }
            if (bx < 0) break;
            visited[by][bx] = 1;
            for (i = 0; i < 4; ++i) {
                int nd = (i + s.self + s.turn) % 4;
                int nx = bx + dx[nd], ny = by + dy[nd];
                int nextCost;
                if (nx < 0 || ny < 0 || nx >= s.size || ny >= s.size) continue;
                if (s.terrain[ny][nx] == '#' || occupied[ny][nx] || visited[ny][nx]) continue;
                if (attackZone[ny][nx] && s.players[s.self].hp <= 1) continue;
                nextCost = best + 10 + 8 * dangerWeight * danger[ny][nx];
                if (attackZone[ny][nx]) nextCost += 90;
                if (nextCost < pathCost[ny][nx]) {
                    pathCost[ny][nx] = nextCost;
                    firstMove[ny][nx] = (bx == me.x && by == me.y) ? nd : firstMove[by][bx];
                }
            }
        }
        if (s.exit.y >= 0 && s.exit.y < s.size && s.exit.x >= 0 && s.exit.x < s.size &&
            pathCost[s.exit.y][s.exit.x] < INF && firstMove[s.exit.y][s.exit.x] >= 0)
            return (Command)firstMove[s.exit.y][s.exit.x];

        /* If the route is blocked, choose the safest legal adjacent move. */
        {
            int bestScore = INF;
            for (d = 0; d < 4; ++d) {
                int nx = me.x + dx[d], ny = me.y + dy[d];
                int score;
                if (nx < 0 || ny < 0 || nx >= s.size || ny >= s.size) continue;
                if (s.terrain[ny][nx] == '#' || occupied[ny][nx]) continue;
                if (attackZone[ny][nx] && s.players[s.self].hp <= 1) continue;
                score = (abs(nx - s.exit.x) + abs(ny - s.exit.y)) * 10 +
                        danger[ny][nx] * 8 + (attackZone[ny][nx] ? 90 : 0);
                if (score < bestScore) { bestScore = score; bestMove = d; }
            }
            if (bestMove >= 0) return (Command)bestMove;
        }
        return COMMAND_UP;
    }

    /* FREE KEY: survival-first approach, contested-key avoidance and ambush staging. */
    if (s.key_owner == -1) {
        int myHp = s.players[s.self].hp;
        int contested = 0;
        int adjacentKeyDirection = -1;
        int bestMove = -1;
        int bestGoalCost = INF;
        int bestGoalFirst = -1;
        int bestStageScore = INF;
        int bestStageFirst = -1;

        /* Detect a real pickup contest: another active player is already in a
           cardinal pickup position. A simultaneous pickup collision hurts both. */
        for (i = 0; i < PLAYER_COUNT; ++i) {
            int md;
            if (!active[i]) continue;
            md = abs(s.players[i].pos.x - s.key.x) +
                 abs(s.players[i].pos.y - s.key.y);
            if (md <= 1) contested = 1;
        }

        /* If a weak competitor is beside the key and inside our attack fan,
           attack instead of entering a simultaneous pickup collision. Only take
           this controlled fight at full HP and when its respawn disadvantage is
           at least as large as ours. */
        if (myHp == MAX_HP) {
            int target = -1;
            int targetDir = -1;
            int targetScore = -INF;
            for (i = 0; i < PLAYER_COUNT; ++i) {
                int ex, ey, md, dir = -1, score;
                if (!active[i]) continue;
                ex = s.players[i].pos.x;
                ey = s.players[i].pos.y;
                md = abs(ex - s.key.x) + abs(ey - s.key.y);
                if (md != 1 || s.players[i].hp != 1 ||
                    s.players[i].deaths < s.players[s.self].deaths) continue;
                if (ey == me.y - 1 && abs(ex - me.x) <= 1) dir = 0;
                else if (ey == me.y + 1 && abs(ex - me.x) <= 1) dir = 1;
                else if (ex == me.x - 1 && abs(ey - me.y) <= 1) dir = 2;
                else if (ex == me.x + 1 && abs(ey - me.y) <= 1) dir = 3;
                if (dir < 0) continue;
                score = 100 + 5 * (int)(s.players[i].deaths - s.players[s.self].deaths);
                if (score > targetScore) {
                    targetScore = score;
                    target = i;
                    targetDir = dir;
                }
            }
            if (target >= 0 && targetDir >= 0)
                return (Command)(COMMAND_ATTACK_UP + targetDir);
        }

        /* A pickup is safe only when no active non-carrier can hit our current
           tile and no opponent can issue a competing pickup this turn. */
        for (d = 0; d < 4; ++d) {
            if (me.x + dx[d] == s.key.x && me.y + dy[d] == s.key.y) {
                adjacentKeyDirection = d;
                break;
            }
        }
        if (adjacentKeyDirection >= 0 &&
            me.x >= 0 && me.y >= 0 && me.x < s.size && me.y < s.size &&
            !attackZone[me.y][me.x] && !contested) {
            return (Command)(COMMAND_PICK_UP + adjacentKeyDirection);
        }

        /* Dijkstra from P4. Cells currently in an opponent's attack fan are
           hard exclusions, not merely a small path-cost penalty. This prevents
           the old behaviour of stepping next to an enemy just to touch the key. */
        for (y = 0; y < s.size; ++y) {
            for (x = 0; x < s.size; ++x) {
                pathCost[y][x] = INF;
                firstMove[y][x] = -1;
                visited[y][x] = 0;
            }
        }
        if (me.x >= 0 && me.y >= 0 && me.x < s.size && me.y < s.size)
            pathCost[me.y][me.x] = 0;

        for (;;) {
            int bx = -1, by = -1, best = INF;
            for (y = 0; y < s.size; ++y)
                for (x = 0; x < s.size; ++x)
                    if (!visited[y][x] && pathCost[y][x] < best) {
                        best = pathCost[y][x]; bx = x; by = y;
                    }
            if (bx < 0) break;
            visited[by][bx] = 1;
            for (i = 0; i < 4; ++i) {
                int nd = (i + s.self + s.turn) % 4;
                int nx = bx + dx[nd], ny = by + dy[nd];
                int nextCost, localDanger;
                if (nx < 0 || ny < 0 || nx >= s.size || ny >= s.size) continue;
                if (s.terrain[ny][nx] == '#' || s.terrain[ny][nx] == 'E' ||
                    occupied[ny][nx] || visited[ny][nx]) continue;
                if (attackZone[ny][nx]) continue;
                localDanger = (int)danger[ny][nx];
                nextCost = best + 10 + 12 * localDanger;
                /* A contested key is a reason to stay on its safe perimeter,
                   not to race into the four pickup squares. */
                if (contested) {
                    int keyDistance = abs(nx - s.key.x) + abs(ny - s.key.y);
                    if (keyDistance <= 2) nextCost += 35;
                }
                if (nextCost < pathCost[ny][nx]) {
                    pathCost[ny][nx] = nextCost;
                    firstMove[ny][nx] = (bx == me.x && by == me.y) ? nd : firstMove[by][bx];
                }
            }
        }

        /* First choice: a safe, unoccupied cardinal tile next to the key. */
        if (!contested) {
            for (d = 0; d < 4; ++d) {
                int gx = s.key.x + dx[d], gy = s.key.y + dy[d];
                int goalCost;
                if (gx < 0 || gy < 0 || gx >= s.size || gy >= s.size) continue;
                if (s.terrain[gy][gx] == '#' || s.terrain[gy][gx] == 'E' ||
                    occupied[gy][gx] || attackZone[gy][gx]) continue;
                if (pathCost[gy][gx] >= INF || firstMove[gy][gx] < 0) continue;
                goalCost = pathCost[gy][gx] + 12 * (int)danger[gy][gx];
                if (goalCost < bestGoalCost) {
                    bestGoalCost = goalCost;
                    bestGoalFirst = firstMove[gy][gx];
                }
            }
        }
        if (bestGoalFirst >= 0 && bestGoalFirst < 4)
            return (Command)bestGoalFirst;

        /* No safe pickup route, or the key is being contested. Hold a safe
           staging tile near the key-to-exit corridor. Stay outside all current
           attack fans; this gives P4 a chance to intercept the carrier later. */
        {
            int keyExitDistance = abs(s.key.x - s.exit.x) + abs(s.key.y - s.exit.y);
            for (y = 0; y < s.size; ++y) {
                for (x = 0; x < s.size; ++x) {
                    int keyDistance, exitDistance, minEnemy = 1000;
                    int lineGap, score, first;
                    if (pathCost[y][x] >= INF || occupied[y][x] ||
                        s.terrain[y][x] == '#' || s.terrain[y][x] == 'E' ||
                        attackZone[y][x]) continue;
                    keyDistance = abs(x - s.key.x) + abs(y - s.key.y);
                    if (keyDistance < 2 || keyDistance > 6) continue;
                    exitDistance = abs(x - s.exit.x) + abs(y - s.exit.y);
                    lineGap = abs((keyDistance + exitDistance) - keyExitDistance);
                    for (i = 0; i < PLAYER_COUNT; ++i) {
                        int md;
                        if (!active[i]) continue;
                        md = abs(x - s.players[i].pos.x) +
                             abs(y - s.players[i].pos.y);
                        if (md < minEnemy) minEnemy = md;
                    }
                    if (minEnemy <= 1) continue;
                    score = pathCost[y][x] + 8 * keyDistance +
                            5 * lineGap - (minEnemy < 8 ? 2 * minEnemy : 16);
                    /* Prefer the exit side of the key corridor when scores tie. */
                    if (exitDistance < keyDistance) score -= 6;
                    first = firstMove[y][x];
                    if (score < bestStageScore) {
                        bestStageScore = score;
                        bestStageFirst = first;
                    }
                }
            }
        }
        if (bestStageFirst >= 0 && bestStageFirst < 4)
            return (Command)bestStageFirst;

        /* Last resort: step to the safest reachable adjacent tile, maximizing
           distance from opponents before considering distance to the key. */
        {
            int bestSafety = -INF;
            for (d = 0; d < 4; ++d) {
                int nx = me.x + dx[d], ny = me.y + dy[d];
                int minEnemy = 1000, score;
                if (nx < 0 || ny < 0 || nx >= s.size || ny >= s.size) continue;
                if (s.terrain[ny][nx] == '#' || s.terrain[ny][nx] == 'E' ||
                    occupied[ny][nx] || attackZone[ny][nx]) continue;
                for (i = 0; i < PLAYER_COUNT; ++i) {
                    int md;
                    if (!active[i]) continue;
                    md = abs(nx - s.players[i].pos.x) +
                         abs(ny - s.players[i].pos.y);
                    if (md < minEnemy) minEnemy = md;
                }
                score = 10 * minEnemy - 2 *
                        (abs(nx - s.key.x) + abs(ny - s.key.y));
                if (score > bestSafety) {
                    bestSafety = score;
                    bestMove = d;
                }
            }
        }
        if (bestMove >= 0) return (Command)bestMove;
        /* There is no WAIT command. An empty attack fan is the safe stationary
           action and allows the next snapshot to reveal whether the contest moved. */
        return (Command)(COMMAND_ATTACK_UP + safeDirection);
    }

    /* ENEMY CARRIES KEY: attack the carrier if in its fan, otherwise chase a
       reachable adjacent attack tile. Key possession blocks the carrier from
       attacking, so this is P4's normal combat priority. */
    if (s.key_owner >= 0 && s.key_owner < PLAYER_COUNT && active[s.key_owner]) {
        Position enemy = s.players[s.key_owner].pos;
        int myHp = s.players[s.self].hp;
        int attackDirection = -1;
        int bestAttackScore = -1;
        for (d = 0; d < 4; ++d) {
            int ax = enemy.x - me.x, ay = enemy.y - me.y;
            int hit = (d == 0 && ay == -1 && abs(ax) <= 1) ||
                      (d == 1 && ay == 1 && abs(ax) <= 1) ||
                      (d == 2 && ax == -1 && abs(ay) <= 1) ||
                      (d == 3 && ax == 1 && abs(ay) <= 1);
            if (hit) {
                int score = 100 + (s.players[s.key_owner].hp == 1 ? 10 : 0);
                if (score > bestAttackScore) {
                    bestAttackScore = score;
                    attackDirection = d;
                }
            }
        }
        /* At one HP, leave crossfire before attacking the carrier. The carrier
           itself is excluded from attackZone because it cannot attack while
           carrying the key. */
        if (myHp <= 1 && me.x >= 0 && me.y >= 0 &&
            me.x < s.size && me.y < s.size && attackZone[me.y][me.x]) {
            int escapeDir = -1, escapeScore = -INF;
            for (d = 0; d < 4; ++d) {
                int nx = me.x + dx[d], ny = me.y + dy[d];
                int minEnemy = 1000, score;
                if (nx < 0 || ny < 0 || nx >= s.size || ny >= s.size) continue;
                if (s.terrain[ny][nx] == '#' || s.terrain[ny][nx] == 'E' ||
                    occupied[ny][nx] || attackZone[ny][nx]) continue;
                for (i = 0; i < PLAYER_COUNT; ++i) {
                    int md;
                    if (!active[i] || i == s.key_owner) continue;
                    md = abs(nx - s.players[i].pos.x) +
                         abs(ny - s.players[i].pos.y);
                    if (md < minEnemy) minEnemy = md;
                }
                score = 10 * minEnemy -
                        (abs(nx - enemy.x) + abs(ny - enemy.y));
                if (score > escapeScore) {
                    escapeScore = score;
                    escapeDir = d;
                }
            }
            if (escapeDir >= 0) return (Command)escapeDir;
        }

        if (attackDirection >= 0 &&
            !(myHp <= 1 && me.x >= 0 && me.y >= 0 &&
              me.x < s.size && me.y < s.size && attackZone[me.y][me.x]))
            return (Command)(COMMAND_ATTACK_UP + attackDirection);

        for (y = 0; y < s.size; ++y) {
            for (x = 0; x < s.size; ++x) {
                pathCost[y][x] = INF;
                firstMove[y][x] = -1;
                visited[y][x] = 0;
            }
        }
        if (me.x >= 0 && me.y >= 0 && me.x < s.size && me.y < s.size)
            pathCost[me.y][me.x] = 0;
        for (;;) {
            int bx = -1, by = -1, best = INF;
            for (y = 0; y < s.size; ++y)
                for (x = 0; x < s.size; ++x)
                    if (!visited[y][x] && pathCost[y][x] < best) {
                        best = pathCost[y][x]; bx = x; by = y;
                    }
            if (bx < 0) break;
            visited[by][bx] = 1;
            for (i = 0; i < 4; ++i) {
                int nd = (i + s.self + s.turn) % 4;
                int nx = bx + dx[nd], ny = by + dy[nd];
                int nextCost;
                if (nx < 0 || ny < 0 || nx >= s.size || ny >= s.size) continue;
                if (s.terrain[ny][nx] == '#' || s.terrain[ny][nx] == 'E' ||
                    occupied[ny][nx] || visited[ny][nx]) continue;
                if (attackZone[ny][nx] && myHp <= 1) continue;
                nextCost = best + 10 + 10 * danger[ny][nx] +
                           (attackZone[ny][nx] ? 70 : 0);
                if (nextCost < pathCost[ny][nx]) {
                    pathCost[ny][nx] = nextCost;
                    firstMove[ny][nx] = (bx == me.x && by == me.y) ? nd : firstMove[by][bx];
                }
            }
        }
        {
            int bestCost = INF, bestFirst = -1;
            for (y = enemy.y - 1; y <= enemy.y + 1; ++y) {
                for (x = enemy.x - 1; x <= enemy.x + 1; ++x) {
                    if (x < 0 || y < 0 || x >= s.size || y >= s.size) continue;
                    if (x == enemy.x && y == enemy.y) continue;
                    if (s.terrain[y][x] == '#' || s.terrain[y][x] == 'E' || occupied[y][x]) continue;
                    if (attackZone[y][x] && myHp <= 1) continue;
                    if (pathCost[y][x] + (attackZone[y][x] ? 70 : 0) < bestCost) {
                        bestCost = pathCost[y][x] + (attackZone[y][x] ? 70 : 0);
                        bestFirst = firstMove[y][x];
                    }
                }
            }
            if (bestFirst >= 0 && bestFirst < 4) return (Command)bestFirst;
        }
    }

    return (Command)(COMMAND_ATTACK_UP + safeDirection);
}
