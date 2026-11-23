#include "player_api.h"

/* 화면에 표시할 팀 이름 */
char player2_name[PLAYER_NAME_BYTES] = "세얼간이";

#define P2_INF 100000000

static const int P2_DX[4] = { 0, 0, -1, 1 };
static const int P2_DY[4] = { -1, 1, 0, 0 };

static int in_bounds(int n, int x, int y)
{
    return x >= 0 && y >= 0 && x < n && y < n;
}

static int is_active_enemy(const Snapshot* s, int idx)
{
    return idx != s->self && s->players[idx].hp > 0 && s->players[idx].rank == 0;
}

static int cheb_dist(int x1, int y1, int x2, int y2)
{
    int dx = abs(x1 - x2), dy = abs(y1 - y2);
    return (dx > dy) ? dx : dy;
}

/* (mx, my)에서 방향 dir로 공격했을 때 (tx, ty)가 3칸 부채꼴에 들어가는지 판정 */
static int in_fan(int mx, int my, int dir, int tx, int ty)
{
    int fx = mx + P2_DX[dir], fy = my + P2_DY[dir];
    return (dir < 2) ? (ty == fy && abs(tx - fx) <= 1)
        : (tx == fx && abs(ty - fy) <= 1);
}

/* 비소지 적(공격 가능) 기준 (x, y)의 근접 위험도 계산 */
static int enemy_proximity(const Snapshot* s, int x, int y)
{
    int i, score = 0;
    for (i = 0; i < PLAYER_COUNT; ++i) {
        if (is_active_enemy(s, i) && i != s->key_owner) {
            int ax = abs(x - s->players[i].pos.x);
            int ay = abs(y - s->players[i].pos.y);
            if (ax <= 1 && ay <= 1) score += 40;
            else if (ax + ay <= 3) score += 14;
        }
    }
    return score;
}

/* (x, y) 주변 이동 가능한 인접 바닥 수 계산 (구석·막다른 길 판별용) */
static int count_exits(const Snapshot* s, int x, int y)
{
    int d, cnt = 0;
    for (d = 0; d < 4; ++d) {
        int nx = x + P2_DX[d], ny = y + P2_DY[d];
        if (in_bounds(s->size, nx, ny) && s->terrain[ny][nx] != '#') ++cnt;
    }
    return cnt;
}

/* 피격 후(myHp < MAX_HP) 적 플레이어가 없는 열린 방향 선택 (구석 진입 방지 포함) */
static int choose_empty_dir(const Snapshot* s,
    const int occ[MAP_MAX][MAP_MAX],
    const int attackZone[MAP_MAX][MAP_MAX],
    const int moveClaim[MAP_MAX][MAP_MAX],
    const int distExit[MAP_MAX][MAP_MAX],
    int carrying)
{
    int n = s->size, me = s->self;
    int mx = s->players[me].pos.x, my = s->players[me].pos.y;
    int i, j, d, bestDir = -1, bestScore = -P2_INF;

    for (i = 0; i < 4; ++i) {
        int nx, ny, minDist = P2_INF, enemiesInDir = 0, exits, score;
        d = (i + s->turn + me) % 4;
        nx = mx + P2_DX[d];
        ny = my + P2_DY[d];
        if (!in_bounds(n, nx, ny) || s->terrain[ny][nx] == '#') continue;
        if (!carrying && s->terrain[ny][nx] == 'E') continue;
        if (occ[ny][nx] != -1) continue;

        for (j = 0; j < PLAYER_COUNT; ++j) {
            int ex, ey, dist;
            if (!is_active_enemy(s, j)) continue;
            ex = s->players[j].pos.x;
            ey = s->players[j].pos.y;
            dist = abs(nx - ex) + abs(ny - ey);
            if (dist < minDist) minDist = dist;

            /* 이동하려는 방향 쪽에 있는 적에게 감점 */
            if ((d == 0 && ey < my) || (d == 1 && ey > my) ||
                (d == 2 && ex < mx) || (d == 3 && ex > mx)) {
                enemiesInDir += (dist <= 4) ? 2 : 1;
            }
        }
        if (minDist == P2_INF) minDist = 10;
        if (minDist > 6) minDist = 6; /* 너무 멀리 구석으로만 파고들지 않게 상한 제한 */

        exits = count_exits(s, nx, ny);
        score = minDist * 120 - enemiesInDir * 110 - moveClaim[ny][nx] * 15;
        if (attackZone[ny][nx] > 0) score -= 20000 + attackZone[ny][nx] * 20;

        /* 구석(모서리/막다른 길)에 갇히지 않도록 열린 교차로 및 맵 중앙 쪽 선호 */
        if (exits <= 1) score -= 900;
        else if (exits == 2) score -= 180;
        else score += 220;

        score -= (abs(nx - n / 2) + abs(ny - n / 2)) * 25;
        if (carrying && distExit[ny][nx] < P2_INF) score -= distExit[ny][nx] * 60;

        if (score > bestScore) {
            bestScore = score;
            bestDir = d;
        }
    }
    return bestDir;
}

/* 다중 시작점 BFS */
static void run_bfs(const Snapshot* s, const int src[MAP_MAX][MAP_MAX],
    int allowExit, int dist[MAP_MAX][MAP_MAX])
{
    Position q[MAP_MAX * MAP_MAX];
    int n = s->size, head = 0, tail = 0, x, y, d;

    for (y = 0; y < MAP_MAX; ++y)
        for (x = 0; x < MAP_MAX; ++x)
            dist[y][x] = P2_INF;

    for (y = 0; y < n; ++y) {
        for (x = 0; x < n; ++x) {
            if (!src[y][x] || s->terrain[y][x] == '#') continue;
            if (!allowExit && s->terrain[y][x] == 'E') continue;
            dist[y][x] = 0;
            q[tail].x = x; q[tail].y = y; ++tail;
        }
    }

    while (head < tail) {
        Position p = q[head++];
        for (d = 0; d < 4; ++d) {
            x = p.x + P2_DX[d]; y = p.y + P2_DY[d];
            if (!in_bounds(n, x, y) || s->terrain[y][x] == '#') continue;
            if (!allowExit && s->terrain[y][x] == 'E') continue;
            if (dist[y][x] != P2_INF) continue;
            dist[y][x] = dist[p.y][p.x] + 1;
            q[tail].x = x; q[tail].y = y; ++tail;
        }
    }
}

/* 목표(goal)에서 역방향으로 위험 가중치를 누적하는 통합 다익스트라 */
static void run_dijkstra(const Snapshot* s, const int goal[MAP_MAX][MAP_MAX],
    const int occ[MAP_MAX][MAP_MAX],
    const int attackZone[MAP_MAX][MAP_MAX],
    const int moveClaim[MAP_MAX][MAP_MAX],
    int carrying, int lateGame, int dist[MAP_MAX][MAP_MAX])
{
    unsigned char used[MAP_MAX][MAP_MAX];
    int n = s->size, me = s->self;
    int mx = s->players[me].pos.x, my = s->players[me].pos.y;
    int myHp = s->players[me].hp;
    int x, y, d;

    for (y = 0; y < MAP_MAX; ++y) {
        for (x = 0; x < MAP_MAX; ++x) {
            used[y][x] = 0;
            dist[y][x] = P2_INF;
        }
    }

    for (y = 0; y < n; ++y) {
        for (x = 0; x < n; ++x) {
            if (!goal[y][x]) continue;
            if (carrying) {
                dist[y][x] = attackZone[y][x] * (myHp < MAX_HP ? 10 : 2);
            }
            else {
                int prox = enemy_proximity(s, x, y);
                int proxW = (myHp < MAX_HP) ? prox : (prox / (lateGame ? 4 : 2));
                dist[y][x] = proxW + ((occ[y][x] != -1 && occ[y][x] != me) ? 450 : 0);
            }
        }
    }

    for (;;) {
        int bx = -1, by = -1, bestD = P2_INF;
        for (y = 0; y < n; ++y)
            for (x = 0; x < n; ++x)
                if (!used[y][x] && dist[y][x] < bestD) {
                    bestD = dist[y][x]; bx = x; by = y;
                }
        if (bx < 0) break;
        used[by][bx] = 1;

        for (d = 0; d < 4; ++d) {
            int stepCost = 100, distFromMe;
            x = bx + P2_DX[d]; y = by + P2_DY[d];
            if (!in_bounds(n, x, y) || used[y][x] || s->terrain[y][x] == '#') continue;
            if (!carrying && s->terrain[y][x] == 'E') continue;

            distFromMe = abs(x - mx) + abs(y - my);

            if (carrying) {
                if (occ[y][x] != -1 && occ[y][x] != me) continue;
                if (distFromMe > 0) {
                    stepCost += enemy_proximity(s, x, y) * (myHp < MAX_HP ? 6 : 2);
                    stepCost += attackZone[y][x] * (myHp < MAX_HP ? 10 : 2);
                    if (distFromMe == 1) stepCost += moveClaim[y][x] * (myHp < MAX_HP ? 5 : 3);
                    else if (distFromMe == 2) stepCost += moveClaim[y][x] * 2;
                }
            }
            else {
                if (occ[y][x] != -1 && occ[y][x] != me)
                    stepCost += (distFromMe <= 1) ? 700 : 200;
                if (distFromMe == 1) {
                    if (myHp < MAX_HP) stepCost += attackZone[y][x] * 5 + moveClaim[y][x] * 3;
                    else {
                        int risk = attackZone[y][x] + moveClaim[y][x];
                        stepCost += lateGame ? (risk / 2) : risk;
                    }
                }
                else if (distFromMe == 2 && myHp < MAX_HP) {
                    stepCost += attackZone[y][x] * 2;
                }
            }

            if (bestD + stepCost < dist[y][x]) dist[y][x] = bestD + stepCost;
        }
    }
}

Command player2_choose_command(const char* state_path)
{
    Snapshot s;
    FILE* file;

    int occ[MAP_MAX][MAP_MAX];
    int goal[MAP_MAX][MAP_MAX];
    int distExit[MAP_MAX][MAP_MAX];
    int distKey[MAP_MAX][MAP_MAX];
    int distPure[MAP_MAX][MAP_MAX];
    int distSafe[MAP_MAX][MAP_MAX];
    int attackZone[MAP_MAX][MAP_MAX];
    int moveClaim[MAP_MAX][MAP_MAX];

    int predX[PLAYER_COUNT], predY[PLAYER_COUNT];
    double atkScore[4] = { 0.0, 0.0, 0.0, 0.0 };
    int atkHitNow[4] = { 0, 0, 0, 0 };
    int atkCanKill[4] = { 0, 0, 0, 0 };
    int safeWaitDir = 0;

    int n, me, mx, my, myHp, owner, lateGame, keyMoved;
    int aliveEnemies = 0, alivePlayers = 1, nearestThreatCheb = P2_INF;
    int minEnemyKeyDist = P2_INF, ambushMode = 0;
    int i, j, d, k, x, y, bestAtk;

    /* ---------- 0. 상태 파일 읽기 ---------- */
    file = fopen(state_path, "r");
    if (!file) return COMMAND_ATTACK_UP;
    i = snapshot_read(file, &s);
    fclose(file);
    if (!i) return COMMAND_ATTACK_UP;

    n = s.size;
    me = s.self;
    mx = s.players[me].pos.x;
    my = s.players[me].pos.y;
    myHp = s.players[me].hp;
    owner = s.key_owner;
    lateGame = (s.turn >= 70);
    keyMoved = (s.key.x != s.key_start.x || s.key.y != s.key_start.y);

    /* ---------- 1. 맵 초기화 및 활동 플레이어 파악 ---------- */
    for (y = 0; y < MAP_MAX; ++y) {
        for (x = 0; x < MAP_MAX; ++x) {
            occ[y][x] = -1;
            goal[y][x] = 0;
            attackZone[y][x] = 0;
            moveClaim[y][x] = 0;
        }
    }

    for (i = 0; i < PLAYER_COUNT; ++i) {
        if (s.players[i].rank != 0 || s.players[i].hp <= 0) continue;
        occ[s.players[i].pos.y][s.players[i].pos.x] = i;
        if (is_active_enemy(&s, i)) {
            ++aliveEnemies;
            if (i != owner) {
                int cd = cheb_dist(mx, my, s.players[i].pos.x, s.players[i].pos.y);
                if (cd < nearestThreatCheb) nearestThreatCheb = cd;
            }
        }
    }
    alivePlayers = aliveEnemies + 1; /* 나 포함 살아있는 플레이어 수 */

    /* ---------- 2. 기본 거리 BFS (출구 거리, 열쇠 인접 거리) ---------- */
    goal[s.exit.y][s.exit.x] = 1;
    run_bfs(&s, goal, 1, distExit);
    goal[s.exit.y][s.exit.x] = 0;

    {
        Position tk = (owner == -1) ? s.key : s.key_start;
        for (d = 0; d < 4; ++d) {
            x = tk.x + P2_DX[d]; y = tk.y + P2_DY[d];
            if (in_bounds(n, x, y) && s.terrain[y][x] == ' ') goal[y][x] = 1;
        }
        run_bfs(&s, goal, 0, distKey);
        for (y = 0; y < n; ++y) for (x = 0; x < n; ++x) goal[y][x] = 0;
    }

    for (i = 0; i < PLAYER_COUNT; ++i) {
        int dKey;
        if (!is_active_enemy(&s, i)) continue;
        dKey = distKey[s.players[i].pos.y][s.players[i].pos.x];
        if (dKey < minEnemyKeyDist) minEnemyKeyDist = dKey;
    }

    /* ---------- 3. 적의 다음 행동(이동/공격) 예측 및 위험 지도 구축 ---------- */
    for (i = 0; i < PLAYER_COUNT; ++i) {
        int ex, ey, bestStepD = -1, bestStepScore = P2_INF, hasTarget = 0;
        predX[i] = s.players[i].pos.x;
        predY[i] = s.players[i].pos.y;
        if (!is_active_enemy(&s, i)) continue;

        ex = predX[i]; ey = predY[i];

        if (i != owner && !(owner == -1 && abs(ex - s.key.x) + abs(ey - s.key.y) == 1)) {
            for (j = 0; j < PLAYER_COUNT; ++j) {
                if (j == i || s.players[j].hp <= 0 || s.players[j].rank != 0) continue;
                if (abs(ex - s.players[j].pos.x) <= 1 && abs(ey - s.players[j].pos.y) <= 1) {
                    hasTarget = 1; break;
                }
            }
        }

        if (!(owner == -1 && abs(ex - s.key.x) + abs(ey - s.key.y) == 1)) {
            for (d = 0; d < 4; ++d) {
                int nx = ex + P2_DX[d], ny = ey + P2_DY[d], sc;
                if (!in_bounds(n, nx, ny) || s.terrain[ny][nx] == '#') continue;
                if (s.terrain[ny][nx] == 'E' && owner != i) continue;
                if (occ[ny][nx] != -1) continue;

                if (i == owner) sc = distExit[ny][nx];
                else if (owner == -1) sc = distKey[ny][nx];
                else sc = abs(nx - s.players[owner].pos.x) + abs(ny - s.players[owner].pos.y);

                moveClaim[ny][nx] += 15;
                if (sc < bestStepScore) { bestStepScore = sc; bestStepD = d; }
            }
            if (bestStepD >= 0) {
                predX[i] = ex + P2_DX[bestStepD];
                predY[i] = ey + P2_DY[bestStepD];
                moveClaim[predY[i]][predX[i]] += hasTarget ? 25 : 65;
            }
        }

        if (i != owner) {
            for (y = ey - 1; y <= ey + 1; ++y) {
                for (x = ex - 1; x <= ex + 1; ++x) {
                    if ((x == ex && y == ey) || !in_bounds(n, x, y) || s.terrain[y][x] == '#') continue;
                    attackZone[y][x] += 40;
                }
            }
        }
        if (hasTarget) {
            for (d = 0; d < 4; ++d) {
                int dirThreat = 0;
                for (j = 0; j < PLAYER_COUNT; ++j) {
                    if (j == i || s.players[j].hp <= 0 || s.players[j].rank != 0) continue;
                    if (in_fan(ex, ey, d, s.players[j].pos.x, s.players[j].pos.y)) {
                        dirThreat += 35 + (j == owner ? 65 : 0) + (s.players[j].hp == 1 ? 25 : 0);
                    }
                }
                if (dirThreat > 0) {
                    for (k = -1; k <= 1; ++k) {
                        int hx = ex + P2_DX[d] + (d < 2 ? k : 0);
                        int hy = ey + P2_DY[d] + (d < 2 ? 0 : k);
                        if (in_bounds(n, hx, hy) && s.terrain[hy][hx] != '#')
                            attackZone[hy][hx] += dirThreat;
                    }
                }
            }
        }
    }

    /* ---------- 4. 4방향 공격 기대 점수 계산 (비보유 시 사용) ---------- */
    for (d = 0; d < 4; ++d) {
        int curTargets = 0;
        for (j = 0; j < PLAYER_COUNT; ++j) {
            int ex, ey, hitNow, hitPred;
            double w = 1.0;
            if (!is_active_enemy(&s, j)) continue;
            ex = s.players[j].pos.x; ey = s.players[j].pos.y;

            if (j == owner) w = 8.0;
            else if (owner == -1 && abs(ex - s.key.x) + abs(ey - s.key.y) <= 1) w = 4.0;
            if (s.players[j].hp == 1) w *= 2.5;

            hitNow = in_fan(mx, my, d, ex, ey);
            hitPred = in_fan(mx, my, d, predX[j], predY[j]);
            if (hitNow) {
                ++curTargets;
                atkScore[d] += w * ((owner == -1 && abs(ex - s.key.x) + abs(ey - s.key.y) == 1) ? 0.85 : 0.45);
            }
            if (hitPred) atkScore[d] += w * 0.65;
            if ((hitNow || hitPred) && s.players[j].hp == 1) atkCanKill[d] = 1;
        }
        atkHitNow[d] = curTargets;
        if (curTargets == 0) safeWaitDir = d;
    }
    bestAtk = 0;
    for (d = 1; d < 4; ++d) if (atkScore[d] > atkScore[bestAtk]) bestAtk = d;

    /* ======================================================================
       5. 상황별 전략 결정: [Case A] 열쇠 보유 / [Case B] 바닥 열쇠 / [Case C] 적 보유
       ====================================================================== */

       /* [Case A] 내가 열쇠를 가지고 있음 (절대 공격 금지, 출구 탈출 최우선) */
    if (owner == me) {
        int bestDir = -1, bestCost = P2_INF;
        int dropDir = -1, bestDropRisk = P2_INF;

        /* A-1) 출구가 바로 옆이면 즉시 골인!
                HP 2면 한 대 맞아도 반드시 탈출 성공, HP 1일 때만 출구 타격 위협 시 1턴 회피 */
        for (d = 0; d < 4; ++d) {
            if (mx + P2_DX[d] != s.exit.x || my + P2_DY[d] != s.exit.y) continue;
            if (myHp < MAX_HP && attackZone[s.exit.y][s.exit.x] > 0 && (s.turn % 3 != 0)) {
                int dodgeDir = choose_empty_dir(&s, occ, attackZone, moveClaim, distExit, 1);
                if (dodgeDir >= 0 && attackZone[my + P2_DY[dodgeDir]][mx + P2_DX[dodgeDir]] == 0)
                    return (Command)(COMMAND_UP + dodgeDir);
            }
            return (Command)(COMMAND_UP + d);
        }

        /* A-2) 출구에서 역방향 안전 다익스트라 실행 (HP 1이어도 구석으로 안 가고 출구 우회로 탐색!) */
        goal[s.exit.y][s.exit.x] = 1;
        run_dijkstra(&s, goal, occ, attackZone, moveClaim, 1, lateGame, distSafe);

        for (i = 0; i < 4; ++i) {
            d = (i + s.turn) % 4;
            x = mx + P2_DX[d]; y = my + P2_DY[d];
            if (!in_bounds(n, x, y) || s.terrain[y][x] == '#' || occ[y][x] != -1) continue;

            if (distSafe[y][x] < P2_INF && 100 + distSafe[y][x] < bestCost) {
                bestCost = 100 + distSafe[y][x];
                bestDir = d;
            }
            if (s.terrain[y][x] == ' ' && moveClaim[y][x] < 30) {
                int risk = attackZone[y][x] + moveClaim[y][x] * 2;
                if (risk < bestDropRisk) { bestDropRisk = risk; dropDir = d; }
            }
        }

        /* A-3) 열쇠 드롭(DROP)은 출구 길이 물리적으로 완전히 막혔을 때(bestDir < 0)만 최후의 수단으로 사용!
                (HP 2일 때 어설프게 열쇠를 버려 적에게 헌납하는 현상 원천 차단) */
        if (bestDir < 0 && myHp >= MAX_HP && attackZone[my][mx] > 0 && dropDir >= 0) {
            return (Command)(COMMAND_DROP_UP + dropDir);
        }

        /* HP 1일 때 다음 출구 방향 칸이 바로 적 공격 범위(attackZone > 0)라면,
           그 1턴만 플레이어가 없는 열린 안전 칸으로 비켜섬 (단, 구석 무한 갇힘 방지 위해 4턴마다 돌파) */
        if (myHp < MAX_HP && bestDir >= 0 &&
            attackZone[my + P2_DY[bestDir]][mx + P2_DX[bestDir]] > 0 && (s.turn % 4 != 0)) {
            int dodgeDir = choose_empty_dir(&s, occ, attackZone, moveClaim, distExit, 1);
            if (dodgeDir >= 0 && attackZone[my + P2_DY[dodgeDir]][mx + P2_DX[dodgeDir]] == 0)
                return (Command)(COMMAND_UP + dodgeDir);
        }

        if (bestDir >= 0) return (Command)(COMMAND_UP + bestDir);

        /* 최후 안전망: 플레이어 없는 칸 -> 우회 이동 -> 빈 칸 드롭 -> 벽('#') 회피 이동 */
        {
            int fbDir = choose_empty_dir(&s, occ, attackZone, moveClaim, distExit, 1);
            if (fbDir >= 0) return (Command)(COMMAND_UP + fbDir);
            for (i = 0; i < 4; ++i) {
                d = (i + s.turn) % 4;
                x = mx + P2_DX[d]; y = my + P2_DY[d];
                if (in_bounds(n, x, y) && s.terrain[y][x] == ' ' && occ[y][x] == -1)
                    return (Command)(COMMAND_DROP_UP + d);
            }
            for (i = 0; i < 4; ++i) {
                d = (i + s.turn) % 4;
                x = mx + P2_DX[d]; y = my + P2_DY[d];
                if (in_bounds(n, x, y) && s.terrain[y][x] != '#')
                    return (Command)(COMMAND_UP + d);
            }
            return COMMAND_UP;
        }
    }

    /* ======================================================================
       [비보유 피격 회피] 공격을 한 번이라도 당한 상태(myHp < MAX_HP)이고
       공격 가능한 적이 2칸 이내(체비쇼프 거리 <= 2) 또는 현재 칸이 공격 범위일 때만
       플레이어가 없는 열린 방향으로 즉시 이탈! (5칸 과잉 도주로 인한 구석 갇힘 해결)
       ====================================================================== */
    if (myHp < MAX_HP && (nearestThreatCheb <= 2 || attackZone[my][mx] > 0)) {
        int emptyDir = choose_empty_dir(&s, occ, attackZone, moveClaim, distExit, 0);
        if (emptyDir >= 0) return (Command)(COMMAND_UP + emptyDir);
    }

    /* [Case B] 열쇠가 바닥에 있음 (owner == -1) */
    if (owner == -1) {
        int myKeyDist = distKey[my][mx];
        int myExitDist = distExit[my][mx];
        int respawnSoon = (keyMoved && s.key_respawn_remaining > 0 && s.key_respawn_remaining <= 2);

        /* B-1) 열쇠 칸 바로 위에 서 있으면 출구와 가까운 안전한 인접 칸으로 비켜섬 */
        if (mx == s.key.x && my == s.key.y) {
            int bestStep = -1, bestVal = P2_INF;
            for (d = 0; d < 4; ++d) {
                int val;
                x = mx + P2_DX[d]; y = my + P2_DY[d];
                if (!in_bounds(n, x, y) || s.terrain[y][x] != ' ' || occ[y][x] != -1) continue;
                val = distExit[y][x] * 30 + attackZone[y][x] * 2 + moveClaim[y][x] * 3;
                if (val < bestVal) { bestVal = val; bestStep = d; }
            }
            if (bestStep >= 0) return (Command)(COMMAND_UP + bestStep);
        }

        /* B-2) 열쇠 인접 칸에 도착: 경쟁자 선제공격(3턴 주기 교착 방지), 아니면 즉시 집기 */
        for (d = 0; d < 4; ++d) {
            if (mx + P2_DX[d] != s.key.x || my + P2_DY[d] != s.key.y) continue;
            if (!respawnSoon) {
                for (j = 0; j < PLAYER_COUNT; ++j) {
                    int ex, ey;
                    if (!is_active_enemy(&s, j)) continue;
                    ex = s.players[j].pos.x; ey = s.players[j].pos.y;
                    if (abs(ex - s.key.x) + abs(ey - s.key.y) != 1) continue;
                    if ((s.turn + me) % 3 != 0) {
                        for (k = 0; k < 4; ++k)
                            if (in_fan(mx, my, k, ex, ey)) return (Command)(COMMAND_ATTACK_UP + k);
                        return (Command)(COMMAND_ATTACK_UP + d);
                    }
                }
            }
            return (Command)(COMMAND_PICK_UP + d);
        }

        /* B-3) 30턴 복귀 전 도달 불가 -> key_start 선발 이동
                출구 입구 매복은 생존 플레이어가 3명 이하일 때만 실행! */
        if (keyMoved && s.key_respawn_remaining > 0 && myKeyDist >= s.key_respawn_remaining) {
            for (d = 0; d < 4; ++d) {
                x = s.key_start.x + P2_DX[d]; y = s.key_start.y + P2_DY[d];
                if (in_bounds(n, x, y) && s.terrain[y][x] == ' ') goal[y][x] = 1;
            }
        }
        else if (alivePlayers <= 3 && minEnemyKeyDist < P2_INF &&
            (!keyMoved || s.key_respawn_remaining >= 20) &&
            ((minEnemyKeyDist <= 1 && myKeyDist >= 5 && myExitDist <= distExit[s.key.y][s.key.x]) ||
                (minEnemyKeyDist + 5 <= myKeyDist && myExitDist <= minEnemyKeyDist + distExit[s.key.y][s.key.x] + 1))) {
            ambushMode = 1;
            for (d = 0; d < 4; ++d) {
                x = s.exit.x + P2_DX[d]; y = s.exit.y + P2_DY[d];
                if (in_bounds(n, x, y) && s.terrain[y][x] == ' ') goal[y][x] = 1;
            }
        }
        else {
            for (d = 0; d < 4; ++d) {
                x = s.key.x + P2_DX[d]; y = s.key.y + P2_DY[d];
                if (in_bounds(n, x, y) && s.terrain[y][x] == ' ') goal[y][x] = 1;
            }
        }
    }
    /* [Case C] 다른 플레이어가 열쇠를 가지고 있음 (owner != me) */
    else {
        int cx = s.players[owner].pos.x, cy = s.players[owner].pos.y;
        int carrierDist = distExit[cy][cx], myDist = distExit[my][mx];

        /* C-1) 소지자가 3칸 부채꼴 공격 범위(현재/예측) 안에 있으면 즉시 타격 */
        for (d = 0; d < 4; ++d) {
            if (in_fan(mx, my, d, cx, cy) || in_fan(mx, my, d, predX[owner], predY[owner]))
                return (Command)(COMMAND_ATTACK_UP + d);
        }

        /* C-2) 플레이어가 3명 이하일 때만 출구 입구 매복 알고리즘 실행! */
        if (alivePlayers <= 3 && !(carrierDist <= 1 && myDist > 2)) {
            int maxRing = (carrierDist <= 7) ? 1 : 2;
            ambushMode = 1;
            for (y = 0; y < n; ++y)
                for (x = 0; x < n; ++x)
                    if (distExit[y][x] >= 1 && distExit[y][x] <= maxRing && s.terrain[y][x] == ' ')
                        goal[y][x] = 1;
        }
        /* C-3) 플레이어가 4명 이상이면 입구 대기 금지:
                소지자가 가까이(6칸 이내) 있으면 추격, 멀거나 탈출 직전이면 key_start로 이동 */
        else {
            int setGoal = 0;
            if (carrierDist > 1 && abs(mx - cx) + abs(my - cy) <= 6) {
                for (d = 0; d < 4; ++d) {
                    x = predX[owner] + P2_DX[d]; y = predY[owner] + P2_DY[d];
                    if (in_bounds(n, x, y) && s.terrain[y][x] == ' ') {
                        goal[y][x] = 1; setGoal = 1;
                    }
                }
            }
            if (!setGoal) {
                for (d = 0; d < 4; ++d) {
                    x = s.key_start.x + P2_DX[d]; y = s.key_start.y + P2_DY[d];
                    if (in_bounds(n, x, y) && s.terrain[y][x] == ' ') goal[y][x] = 1;
                }
            }
        }
    }

    goal[my][mx] = 0;

    if (ambushMode && distExit[my][mx] <= 2 && atkScore[bestAtk] >= 1.5)
        return (Command)(COMMAND_ATTACK_UP + bestAtk);

    /* ---------- 6. 비보유 시 목표까지의 BFS 및 위험 가중 다익스트라 ---------- */
    run_bfs(&s, goal, 0, distPure);
    run_dijkstra(&s, goal, occ, attackZone, moveClaim, 0, lateGame, distSafe);

    /* ---------- 7. 일반 적 선제 공격 / 다음 1칸 이동 / 길뚫기 선택 ---------- */
    {
        int bestMove = -1, bestTotalCost = P2_INF, bestRival = 0;
        for (i = 0; i < 4; ++i) {
            int rival = 0, totalCost;
            d = (i + s.turn + me) % 4;
            x = mx + P2_DX[d]; y = my + P2_DY[d];
            if (!in_bounds(n, x, y) || s.terrain[y][x] == '#' || s.terrain[y][x] == 'E') continue;
            if (occ[y][x] != -1 || distSafe[y][x] >= P2_INF) continue;
            if (myHp < MAX_HP && attackZone[y][x] > 0) continue;

            for (j = 0; j < PLAYER_COUNT; ++j)
                if (is_active_enemy(&s, j) && predX[j] == x && predY[j] == y) ++rival;

            totalCost = distSafe[y][x]
                + (distPure[y][x] < P2_INF ? distPure[y][x] * 70 : 500)
                + moveClaim[y][x] + rival * 120;

            if (distSafe[y][x] < distSafe[my][mx] ||
                distPure[y][x] < distPure[my][mx] ||
                attackZone[my][mx] > attackZone[y][x] + 25) {
                if (totalCost < bestTotalCost) {
                    bestTotalCost = totalCost; bestMove = d; bestRival = rival;
                }
            }
        }

        /* 이동 전 일반 적 선제 타격 (체력이 가득 찼을 때만 교전) */
        if (myHp >= MAX_HP && atkHitNow[bestAtk] > 0 &&
            (atkCanKill[bestAtk] || (atkScore[bestAtk] >= 1.1 && (s.turn + me) % 2 == 0))) {
            return (Command)(COMMAND_ATTACK_UP + bestAtk);
        }

        if (bestMove >= 0) {
            if (bestRival > 0 && myHp >= MAX_HP && s.turn % 2 == 0)
                return (Command)(COMMAND_ATTACK_UP + bestMove);
            return (Command)(COMMAND_UP + bestMove);
        }

        /* 최단 경로 바로 앞을 막은 적 길뚫기 공격 */
        for (i = 0; i < 4; ++i) {
            d = (i + s.turn) % 4;
            x = mx + P2_DX[d]; y = my + P2_DY[d];
            if (in_bounds(n, x, y) && occ[y][x] != -1 && occ[y][x] != me &&
                distPure[y][x] < distPure[my][mx]) {
                return (Command)(COMMAND_ATTACK_UP + d);
            }
        }

        /* 공격 대상이 없으면 제자리 허공 공격 대신 패트롤 이동 */
        if (atkScore[bestAtk] <= 0.0) {
            for (i = 0; i < 4; ++i) {
                int sc;
                d = (i + s.turn + me) % 4;
                x = mx + P2_DX[d]; y = my + P2_DY[d];
                if (!in_bounds(n, x, y) || s.terrain[y][x] == '#' || s.terrain[y][x] == 'E' || occ[y][x] != -1) continue;
                if (myHp < MAX_HP && attackZone[y][x] > 0) continue;
                sc = (distPure[y][x] < P2_INF ? distPure[y][x] * 100 : 5000)
                    + attackZone[y][x] * 3 + moveClaim[y][x] * 4;
                if (sc < bestTotalCost) { bestTotalCost = sc; bestMove = d; }
            }
            if (bestMove >= 0) return (Command)(COMMAND_UP + bestMove);
        }
    }

    /* ---------- 8. 근접 견제 공격 또는 안전한 제자리 대기 ---------- */
    if (atkScore[bestAtk] > 0.0) return (Command)(COMMAND_ATTACK_UP + bestAtk);
    return (Command)(COMMAND_ATTACK_UP + safeWaitDir);
}