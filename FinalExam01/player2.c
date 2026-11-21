#include "player_api.h"

/* 표시할 이름을 이 문자열에 입력하세요 (UTF-8, 최대 63바이트). */
char player2_name[PLAYER_NAME_BYTES] = "Player2";

/* ======================================================================
   플레이어2 자동 AI - 상황별 입구/관문 매복 & 최단 경로 하이브리드 알고리즘
   ----------------------------------------------------------------------
   [핵심 설계: 열쇠를 먹든 안 먹든 상황을 판단하여 입구 근처에 매복]
   1) 지형 기반 입구(Entrance) 자동 식별:
      - [탈출구 입구 (Exit Entrance)]: 탈출구(s.exit) 상하좌우 인접 칸(distExit == 1).
        특히 s.exit을 향해 3칸 부채꼴 공격을 날렸을 때 양쪽 진입로와 탈출구를 동시에
        타격할 수 있는 중앙 매복 거점(예: (6,8))과 좌우 길목 봉쇄 거점(예: (5,7), (7,7)).
      - [관문/통로 입구 (Chokepoint Gate)]: 맵 상단(열쇠 구역)과 하단(탈출구 구역)을 잇는
        양옆이 벽('#')인 좁은 1칸 병목 입구(예: (3,6), (9,6)).

   2) 상황 A - 열쇠를 안 먹었을 때 (바닥 열쇠: owner == -1):
      - [최단 경로 선점 vs 입구 매복 판단]:
        내가 적들보다 열쇠에 먼저(또는 비슷하게) 도착할 수 있으면 최단 경로로 열쇠를 줍는다.
        하지만 적이 나보다 훨씬 먼저 열쇠에 도착할 것이 확실하고(minEnemyKeyDist + 3 <= myKeyDist)
        내가 탈출구/관문 입구 쪽에 더 가깝다면, 무리하게 열쇠로 달려가 허탕 치지 않고
        즉시 적이 내려와야 하는 '관문 입구' 또는 '탈출구 입구' 근처로 이동해 매복한다!
      - [열쇠 입구 매복]:
        열쇠 바로 앞에서 경쟁자와 마주쳤을 때는 동시 집기 취소(-1 HP)를 피하기 위해
        먼저 3칸 부채꼴 공격으로 경쟁자를 타격한 뒤 단독으로 열쇠를 획득한다.

   3) 상황 B - 다른 플레이어가 열쇠를 먹었을 때 (소지자 존재: owner != me):
      - [탈출구/관문 입구 선점 매복]:
        내가 소지자보다 탈출구 입구에 먼저 도착할 수 있고(myDistToExit <= carrierDistToExit + 1)
        소지자가 탈출구 쪽으로 이동 중일 때는, 소지자가 진입하는 방향의 '탈출구 입구' 또는
        '관문 입구' 길목을 선점하여 매복한다.
      - [전진 요격 & 적 접근 시 부채꼴 일격 (허공 공격 버그 완벽 제거)]:
        입구 매복 거점에 도착했을 때 아직 소지자가 멀리 있다면 제자리에서 허공 공격(**)을
        반복하지 않고 소지자가 오는 관문 입구 쪽으로 전진 매복하며 거리를 좁힌다.
        소지자가 입구 반경 3칸 이내로 들어오는 순간 3칸 부채꼴 공격으로 타격하여 열쇠를 강탈한다!

   4) 상황 C - 내가 열쇠를 먹었을 때 (열쇠 보유: owner == me):
      - [입구 매복 회피 & 안전 관문 선택]:
        열쇠 보유 중에는 공격이 금지되므로, 적들이 매복 중인 위험한 입구(관문)를 피하고
        안전한 반대편 입구를 통해 최단 경로로 탈출한다.
      - [전술적 열쇠 미끼 드롭 매복 (Bait & Ambush)]:
        입구가 적에게 완전히 봉쇄되었고 적이 바로 옆에 붙어 추격 중일 때는
        인접한 안전 바닥에 열쇠를 내려놓아(DROP) 미끼로 삼고, 다음 턴 열쇠를 주우러
        입구로 다가오는 적을 부채꼴 공격으로 역습한 뒤 다시 열쇠를 주워 탈출한다.
   ====================================================================== */

static int is_active_enemy(const Snapshot *s, int idx)
{
    return idx != s->self && s->players[idx].hp > 0 && s->players[idx].rank == 0;
}

Command player2_choose_command(const char *state_path)
{
    const int dx[4] = {0, 0, -1, 1}, dy[4] = {-1, 1, 0, 0}; /* 상, 하, 좌, 우 */
    const int INF = 100000000;
    Snapshot s;
    FILE *file;

    int occ[MAP_MAX][MAP_MAX];             /* 살아있는(레이싱 중인) 플레이어 번호, 없으면 -1 */
    int spawnBlock[MAP_MAX][MAP_MAX];      /* 곧 부활할 플레이어의 스폰 칸 */
    int degree[MAP_MAX][MAP_MAX];          /* 각 칸에서 이동 가능한 인접 바닥 칸 수 */
    int isGate[MAP_MAX][MAP_MAX];          /* 양옆/상하가 벽인 좁은 병목 입구(관문) 여부 */
    int goal[MAP_MAX][MAP_MAX];            /* 이번 턴 목표 타일 여부 */

    int distExit[MAP_MAX][MAP_MAX];        /* 출구까지의 순수 최단 거리 (벽만 장애물) */
    int distExitAvoid[MAP_MAX][MAP_MAX];   /* 출구까지의 우회 최단 거리 (현재 적 위치를 벽으로 간주) */
    int distKeyAdj[MAP_MAX][MAP_MAX];      /* 열쇠 인접 칸까지의 순수 최단 거리 */
    int distPure[MAP_MAX][MAP_MAX];        /* 현재 목표까지의 순수 최단 거리 */
    int distSafe[MAP_MAX][MAP_MAX];        /* 위험도 가중치를 반영한 다익스트라 최소 비용 */

    int attackZone[MAP_MAX][MAP_MAX];      /* 이번 턴 적들의 예상 공격 범위 위험도 */
    int moveClaim[MAP_MAX][MAP_MAX];       /* 이번 턴 적들이 이동할 것으로 예상되는 타일 충돌 위험도 */
    int enemyProximity[MAP_MAX][MAP_MAX];  /* 적과의 인접/근접 위험도 */

    int predX[PLAYER_COUNT], predY[PLAYER_COUNT]; /* 적들의 이번 턴 예상 도착 위치 */
    double atkScore[4] = {0.0, 0.0, 0.0, 0.0};    /* 내가 4방향으로 공격할 때의 기대 점수 */
    int safeWaitDir = 0;                          /* 아무도 안 맞고 취소되지 않는 안전한 허공 공격 방향 */

    Position queue[MAP_MAX * MAP_MAX];
    int used[MAP_MAX][MAP_MAX];

    int n, me, mx, my, myHp, owner;
    int i, j, d, k, x, y, head, tail, bestAtk;
    int minEnemyKeyDist = INF, closestKeyEnemy = -1, enemiesNearKey = 0;
    int ambushMode = 0; /* 1이면 현재 입구/출구 근처 매복 전략 수행 중 */

    /* ---------- 0. 상태 파일 읽기 ---------- */
    file = fopen(state_path, "r");
    if (!file) return COMMAND_ATTACK_UP;
    i = snapshot_read(file, &s);
    fclose(file);
    if (!i) return COMMAND_ATTACK_UP;

    srand((unsigned)(s.turn * 1315423911u + s.self * 2654435761u + 101u));

    n = s.size;
    me = s.self;
    mx = s.players[me].pos.x;
    my = s.players[me].pos.y;
    myHp = s.players[me].hp;
    owner = s.key_owner; /* -1: 바닥, 0~5: 소유자 번호 */

    /* ---------- 1. 맵 초기화, 활동 플레이어 식별, 병목 입구(isGate) 계산 ---------- */
    for (y = 0; y < MAP_MAX; ++y) {
        for (x = 0; x < MAP_MAX; ++x) {
            occ[y][x] = -1;
            spawnBlock[y][x] = 0;
            degree[y][x] = 0;
            isGate[y][x] = 0;
            goal[y][x] = 0;
            distExit[y][x] = distExitAvoid[y][x] = distKeyAdj[y][x] = distPure[y][x] = distSafe[y][x] = INF;
            attackZone[y][x] = 0;
            moveClaim[y][x] = 0;
            enemyProximity[y][x] = 0;
        }
    }

    for (i = 0; i < PLAYER_COUNT; ++i) {
        if (s.players[i].rank != 0) continue; /* 이미 탈출한 플레이어는 완전 제외 */
        if (s.players[i].hp > 0) {
            occ[s.players[i].pos.y][s.players[i].pos.x] = i;
        } else if (s.players[i].rest <= 1) {
            spawnBlock[s.players[i].spawn.y][s.players[i].spawn.x] = 1;
        }
    }

    for (y = 0; y < n; ++y) {
        for (x = 0; x < n; ++x) {
            int wallLR = 0, wallUD = 0;
            if (s.terrain[y][x] == '#') continue;
            for (d = 0; d < 4; ++d) {
                int nx = x + dx[d], ny = y + dy[d];
                if (nx >= 0 && ny >= 0 && nx < n && ny < n && s.terrain[ny][nx] != '#')
                    ++degree[y][x];
            }
            /* 좌우가 벽이고 상하가 통로이거나, 상하가 벽이고 좌우가 통로인 좁은 관문 입구 탐지 */
            if (x > 0 && x + 1 < n && y > 0 && y + 1 < n) {
                wallLR = (s.terrain[y][x - 1] == '#' && s.terrain[y][x + 1] == '#' &&
                          s.terrain[y - 1][x] != '#' && s.terrain[y + 1][x] != '#');
                wallUD = (s.terrain[y - 1][x] == '#' && s.terrain[y + 1][x] == '#' &&
                          s.terrain[y][x - 1] != '#' && s.terrain[y][x + 1] != '#');
                if (wallLR || wallUD) isGate[y][x] = 1;
            }
            for (i = 0; i < PLAYER_COUNT; ++i) {
                if (is_active_enemy(&s, i)) {
                    int ax = abs(x - s.players[i].pos.x), ay = abs(y - s.players[i].pos.y);
                    if (ax <= 1 && ay <= 1) enemyProximity[y][x] += 40;
                    else if (ax + ay <= 3) enemyProximity[y][x] += 14;
                }
            }
        }
    }

    /* ---------- 2. 기본 거리 BFS (출구 순수 거리, 출구 우회 거리, 열쇠 인접 거리) ---------- */
    /* (2-1) distExit: 벽만 고려한 출구까지의 최단 거리 */
    head = tail = 0;
    distExit[s.exit.y][s.exit.x] = 0;
    queue[tail++] = s.exit;
    while (head < tail) {
        Position p = queue[head++];
        for (d = 0; d < 4; ++d) {
            x = p.x + dx[d]; y = p.y + dy[d];
            if (x < 0 || y < 0 || x >= n || y >= n) continue;
            if (s.terrain[y][x] == '#' || distExit[y][x] != INF) continue;
            distExit[y][x] = distExit[p.y][p.x] + 1;
            queue[tail].x = x; queue[tail].y = y; ++tail;
        }
    }

    /* (2-2) distExitAvoid: 현재 살아있는 적을 장애물로 간주한 출구 최단 거리 */
    head = tail = 0;
    if (occ[s.exit.y][s.exit.x] == -1 || occ[s.exit.y][s.exit.x] == me) {
        distExitAvoid[s.exit.y][s.exit.x] = 0;
        queue[tail++] = s.exit;
    }
    while (head < tail) {
        Position p = queue[head++];
        for (d = 0; d < 4; ++d) {
            x = p.x + dx[d]; y = p.y + dy[d];
            if (x < 0 || y < 0 || x >= n || y >= n) continue;
            if (s.terrain[y][x] == '#' || distExitAvoid[y][x] != INF) continue;
            if (occ[y][x] != -1 && occ[y][x] != me) continue;
            distExitAvoid[y][x] = distExitAvoid[p.y][p.x] + 1;
            queue[tail].x = x; queue[tail].y = y; ++tail;
        }
    }

    /* (2-3) distKeyAdj: 열쇠(또는 곧 리스폰될 열쇠) 인접 칸까지의 최단 거리 */
    {
        Position targetKey = (owner == -1) ? s.key : s.key_start;
        head = tail = 0;
        for (d = 0; d < 4; ++d) {
            x = targetKey.x + dx[d]; y = targetKey.y + dy[d];
            if (x < 0 || y < 0 || x >= n || y >= n) continue;
            if (s.terrain[y][x] == '#' || s.terrain[y][x] == 'E') continue;
            distKeyAdj[y][x] = 0;
            queue[tail].x = x; queue[tail].y = y; ++tail;
        }
        while (head < tail) {
            Position p = queue[head++];
            for (d = 0; d < 4; ++d) {
                x = p.x + dx[d]; y = p.y + dy[d];
                if (x < 0 || y < 0 || x >= n || y >= n) continue;
                if (s.terrain[y][x] == '#' || s.terrain[y][x] == 'E' || distKeyAdj[y][x] != INF) continue;
                distKeyAdj[y][x] = distKeyAdj[p.y][p.x] + 1;
                queue[tail].x = x; queue[tail].y = y; ++tail;
            }
        }
    }

    /* 적들의 열쇠 접근 거리 및 열쇠 주변 밀집도 파악 */
    for (i = 0; i < PLAYER_COUNT; ++i) {
        int dKey;
        if (!is_active_enemy(&s, i)) continue;
        dKey = distKeyAdj[s.players[i].pos.y][s.players[i].pos.x];
        if (dKey <= 2) ++enemiesNearKey;
        if (dKey < minEnemyKeyDist) {
            minEnemyKeyDist = dKey;
            closestKeyEnemy = i;
        }
    }

    /* ---------- 3. 적들의 다음 행동(이동/공격) 정밀 시뮬레이션 ---------- */
    for (i = 0; i < PLAYER_COUNT; ++i) {
        int ex, ey, bestStepD = -1, bestStepScore = INF;
        int enemyHasAdjacentTarget = 0;

        predX[i] = s.players[i].pos.x;
        predY[i] = s.players[i].pos.y;
        if (!is_active_enemy(&s, i)) continue;

        ex = s.players[i].pos.x;
        ey = s.players[i].pos.y;

        /* 적 i의 8방향 인접 칸에 다른 살아있는 플레이어가 있는지 확인 */
        if (i != owner && !(owner == -1 && abs(ex - s.key.x) + abs(ey - s.key.y) == 1)) {
            for (j = 0; j < PLAYER_COUNT; ++j) {
                if (j == i || s.players[j].hp <= 0 || s.players[j].rank != 0) continue;
                if (abs(ex - s.players[j].pos.x) <= 1 && abs(ey - s.players[j].pos.y) <= 1) {
                    enemyHasAdjacentTarget = 1;
                    break;
                }
            }
        }

        /* 적의 이동 예상 타일 계산 */
        if (!(owner == -1 && abs(ex - s.key.x) + abs(ey - s.key.y) == 1)) {
            for (d = 0; d < 4; ++d) {
                int nx = ex + dx[d], ny = ey + dy[d], stepScore = INF;
                if (nx < 0 || ny < 0 || nx >= n || ny >= n || s.terrain[ny][nx] == '#') continue;
                if (s.terrain[ny][nx] == 'E' && owner != i) continue;
                if (occ[ny][nx] != -1) continue;

                if (i == owner) {
                    stepScore = distExitAvoid[ny][nx] < INF ? distExitAvoid[ny][nx] * 10 : distExit[ny][nx] * 10;
                } else if (owner == -1) {
                    stepScore = distKeyAdj[ny][nx] * 10;
                } else {
                    stepScore = (abs(nx - s.players[owner].pos.x) + abs(ny - s.players[owner].pos.y)) * 10;
                }

                moveClaim[ny][nx] += 15;
                if (stepScore < bestStepScore) {
                    bestStepScore = stepScore;
                    bestStepD = d;
                }
            }
            if (bestStepD >= 0) {
                predX[i] = ex + dx[bestStepD];
                predY[i] = ey + dy[bestStepD];
                moveClaim[predY[i]][predX[i]] += (enemyHasAdjacentTarget ? 25 : 65);
            }
        }

        /* 적이 실제로 공격할 가능성이 있는 방향에 대해 3칸 부채꼴 공격 위험도(attackZone) 기록 */
        if (enemyHasAdjacentTarget) {
            for (d = 0; d < 4; ++d) {
                int fx = ex + dx[d], fy = ey + dy[d];
                int dirThreat = 0;

                for (k = -1; k <= 1; ++k) {
                    int hx = (d < 2) ? (fx + k) : fx;
                    int hy = (d < 2) ? fy : (fy + k);
                    if (hx < 0 || hy < 0 || hx >= n || hy >= n || s.terrain[hy][hx] == '#') continue;

                    for (j = 0; j < PLAYER_COUNT; ++j) {
                        if (j == i || s.players[j].hp <= 0 || s.players[j].rank != 0) continue;
                        if (s.players[j].pos.x == hx && s.players[j].pos.y == hy) {
                            dirThreat += 35;
                            if (j == owner) dirThreat += 65;
                            if (s.players[j].hp == 1) dirThreat += 25;
                        }
                    }
                }

                if (dirThreat > 0) {
                    for (k = -1; k <= 1; ++k) {
                        int hx = (d < 2) ? (fx + k) : fx;
                        int hy = (d < 2) ? fy : (fy + k);
                        if (hx < 0 || hy < 0 || hx >= n || hy >= n || s.terrain[hy][hx] == '#') continue;
                        attackZone[hy][hx] += dirThreat;
                    }
                }
            }
        }
    }

    /* ---------- 4. 내가 공격할 때의 4방향 기대 점수 (비보유 시에만 사용) ---------- */
    for (d = 0; d < 4; ++d) {
        int fx = mx + dx[d], fy = my + dy[d];
        int curTargets = 0;
        for (j = 0; j < PLAYER_COUNT; ++j) {
            int ex, ey, hitNow, hitPred;
            double w = 1.0;
            if (!is_active_enemy(&s, j)) continue;
            ex = s.players[j].pos.x; ey = s.players[j].pos.y;

            if (j == owner) w = 8.0;
            else if (owner == -1 && abs(ex - s.key.x) + abs(ey - s.key.y) <= 1) w = 4.0;
            if (s.players[j].hp == 1) w *= 2.5;

            hitNow = (d < 2) ? (ey == fy && abs(ex - fx) <= 1)
                             : (ex == fx && abs(ey - fy) <= 1);
            hitPred = (d < 2) ? (predY[j] == fy && abs(predX[j] - fx) <= 1)
                              : (predX[j] == fx && abs(predY[j] - fy) <= 1);

            if (hitNow) {
                ++curTargets;
                if (owner == -1 && abs(ex - s.key.x) + abs(ey - s.key.y) == 1)
                    atkScore[d] += w * 0.85;
                else
                    atkScore[d] += w * 0.45;
            }
            if (hitPred) {
                atkScore[d] += w * 0.65;
            }
        }
        if (curTargets == 0) safeWaitDir = d;
    }
    bestAtk = 0;
    for (d = 1; d < 4; ++d) if (atkScore[d] > atkScore[bestAtk]) bestAtk = d;

    /* ======================================================================
       5. 상황별 전략 결정:
          [Case A] 내가 열쇠 소지자 (owner == me) -> 입구 매복 회피 탈출 & 전술적 드롭 매복
          [Case B] 열쇠가 바닥에 있음 (owner == -1) -> 상황 봐서 최단 습득 vs 입구 매복
          [Case C] 다른 플레이어가 열쇠 소지 (owner != me) -> 입구/관문 매복 및 강탈
       ====================================================================== */

    if (owner == me) {
        /* ------------------------------------------------------------------
           [Case A] 내가 열쇠를 가지고 있음 (절대 공격 명령 금지!)
           ------------------------------------------------------------------ */
        /* A-1) 출구가 바로 인접해 있으면 즉시 골인!
                단, 내 체력이 1이고 출구 입구에 매복 중인 적이 이번 턴 출구를 타격할 확률이 높다면
                출구 옆 안전 칸으로 1턴 회피/대기하여 적의 공격을 뺀 뒤 골인한다! */
        for (d = 0; d < 4; ++d) {
            if (mx + dx[d] == s.exit.x && my + dy[d] == s.exit.y) {
                if (myHp == 1 && attackZone[s.exit.y][s.exit.x] >= 70) {
                    int dodgeDir = -1, minRisk = attackZone[s.exit.y][s.exit.x];
                    for (k = 0; k < 4; ++k) {
                        int nx = mx + dx[k], ny = my + dy[k];
                        if (nx < 0 || ny < 0 || nx >= n || ny >= n || s.terrain[ny][nx] == '#') continue;
                        if (occ[ny][nx] != -1) continue;
                        if (attackZone[ny][nx] + moveClaim[ny][nx] < minRisk) {
                            minRisk = attackZone[ny][nx] + moveClaim[ny][nx];
                            dodgeDir = k;
                        }
                    }
                    if (dodgeDir >= 0) return (Command)(COMMAND_UP + dodgeDir);
                }
                return (Command)(COMMAND_UP + d);
            }
        }

        /* A-2) 내 위치(me)에서 출발하는 정방향 다익스트라 탐색:
                적 주변의 위험도(enemyProximity)를 강하게 반영하여, 적들이 매복 중인
                위험한 입구/골목을 피해 안전하게 출구까지 이어지는 최단 루트의 첫 걸음(firstStep)을 구한다! */
        {
            int cost[MAP_MAX][MAP_MAX], firstStep[MAP_MAX][MAP_MAX];
            for (y = 0; y < n; ++y) {
                for (x = 0; x < n; ++x) {
                    cost[y][x] = INF;
                    firstStep[y][x] = -1;
                    used[y][x] = 0;
                }
            }
            cost[my][mx] = 0;

            for (;;) {
                int bx = -1, by = -1, minC = INF;
                for (y = 0; y < n; ++y) {
                    for (x = 0; x < n; ++x) {
                        if (!used[y][x] && cost[y][x] < minC) {
                            minC = cost[y][x];
                            bx = x; by = y;
                        }
                    }
                }
                if (bx < 0) break;
                if (bx == s.exit.x && by == s.exit.y) break;
                used[by][bx] = 1;

                for (i = 0; i < 4; ++i) {
                    int stepCost = 100;
                    int isFirst = (bx == mx && by == my);
                    d = i;
                    x = bx + dx[d]; y = by + dy[d];
                    if (x < 0 || y < 0 || x >= n || y >= n || s.terrain[y][x] == '#') continue;
                    if (occ[y][x] != -1 || used[y][x]) continue; /* 적이 막고 있는 통로는 우회! */

                    /* 적 주변 8칸(공격 가능 범위) 및 이번 턴 예상 공격/이동 충돌 회피 */
                    stepCost += enemyProximity[y][x] * (myHp == 1 ? 5 : 3);
                    if (isFirst) {
                        stepCost += attackZone[y][x] * (myHp == 1 ? 7 : 3);
                        stepCost += moveClaim[y][x] * 5;
                    } else if (abs(x - mx) + abs(y - my) == 2) {
                        stepCost += attackZone[y][x] * 2 + moveClaim[y][x] * 2;
                    }

                    if (minC + stepCost < cost[y][x]) {
                        cost[y][x] = minC + stepCost;
                        firstStep[y][x] = isFirst ? d : firstStep[by][bx];
                    }
                }
            }

            /* 적에게 막히지 않고 출구까지 이어지는 안전 최단 경로가 존재하면 즉시 그 첫 걸음으로 이동! */
            if (firstStep[s.exit.y][s.exit.x] >= 0) {
                int chosenDir = firstStep[s.exit.y][s.exit.x];
                int tx = mx + dx[chosenDir], ty = my + dy[chosenDir];
                if (myHp == 1 && (attackZone[ty][tx] >= 70 || moveClaim[ty][tx] >= 70)) {
                    int safeDir = -1, bestSafeRisk = attackZone[ty][tx] + moveClaim[ty][tx];
                    for (d = 0; d < 4; ++d) {
                        int nx = mx + dx[d], ny = my + dy[d];
                        if (nx < 0 || ny < 0 || nx >= n || ny >= n || s.terrain[ny][nx] == '#') continue;
                        if (occ[ny][nx] != -1) continue;
                        if (attackZone[ny][nx] + moveClaim[ny][nx] < bestSafeRisk) {
                            bestSafeRisk = attackZone[ny][nx] + moveClaim[ny][nx];
                            safeDir = d;
                        }
                    }
                    if (safeDir >= 0) return (Command)(COMMAND_UP + safeDir);
                }
                return (Command)(COMMAND_UP + chosenDir);
            }
        }

        /* A-3) 출구로 가는 모든 입구/통로가 적들에 의해 완전히 봉쇄되었을 때(firstStep == -1):
                내 옆에 적이 붙어 있다면 안전한 인접 바닥에 열쇠를 내려놓아(DROP 미끼 매복)
                다음 턴에 길을 막은 적을 직접 공격해 뚫는다! */
        if (enemyProximity[my][mx] >= 40) {
            for (d = 0; d < 4; ++d) {
                x = mx + dx[d]; y = my + dy[d];
                if (x >= 0 && y >= 0 && x < n && y < n && occ[y][x] == -1 && s.terrain[y][x] == ' ' && moveClaim[y][x] < 30)
                    return (Command)(COMMAND_DROP_UP + d);
            }
        }

        {
            int bestMove = -1, bestScore = INF;
            for (d = 0; d < 4; ++d) {
                int score;
                x = mx + dx[d]; y = my + dy[d];
                if (x < 0 || y < 0 || x >= n || y >= n || s.terrain[y][x] == '#') continue;
                if (occ[y][x] != -1) continue;
                score = distExit[y][x] * 100
                      + attackZone[y][x] * (myHp == 1 ? 6 : 2)
                      + moveClaim[y][x] * 4
                      + enemyProximity[y][x] * 3;
                if (degree[y][x] <= 1) score += 500;
                if (score < bestScore) {
                    bestScore = score;
                    bestMove = d;
                }
            }
            if (bestMove >= 0) return (Command)(COMMAND_UP + bestMove);

            for (d = 0; d < 4; ++d) {
                x = mx + dx[d]; y = my + dy[d];
                if (x >= 0 && y >= 0 && x < n && y < n && occ[y][x] == -1 && s.terrain[y][x] == ' ')
                    return (Command)(COMMAND_DROP_UP + d);
            }
            for (d = 0; d < 4; ++d) {
                x = mx + dx[d]; y = my + dy[d];
                if (x >= 0 && y >= 0 && x < n && y < n && s.terrain[y][x] != '#')
                    return (Command)(COMMAND_UP + d);
            }
            return COMMAND_UP;
        }
    }

    /* ------------------------------------------------------------------
       [Case B] 열쇠가 바닥에 있음 (owner == -1)
       -> 상황을 봐서 (1) 열쇠 직접 습득 vs (2) 열쇠를 안 먹고 입구 근처 매복!
       ------------------------------------------------------------------ */
    if (owner == -1) {
        int myKeyDist = distKeyAdj[my][mx];
        int myExitDist = distExitAvoid[my][mx] < INF ? distExitAvoid[my][mx] : distExit[my][mx];

        /* B-1) 내가 열쇠 칸 바로 위에 서 있는 경우 (발밑 집기 불가 -> 인접 칸으로 이동) */
        if (mx == s.key.x && my == s.key.y) {
            int bestStep = -1, bestVal = INF;
            for (d = 0; d < 4; ++d) {
                int val;
                x = mx + dx[d]; y = my + dy[d];
                if (x < 0 || y < 0 || x >= n || y >= n) continue;
                if (s.terrain[y][x] != ' ' || occ[y][x] != -1) continue;
                val = (distExitAvoid[y][x] < INF ? distExitAvoid[y][x] * 30 : distExit[y][x] * 30)
                    + attackZone[y][x] * 2 + moveClaim[y][x] * 3 + enemyProximity[y][x] * 2;
                if (val < bestVal) { bestVal = val; bestStep = d; }
            }
            if (bestStep >= 0) return (Command)(COMMAND_UP + bestStep);
        }

        /* B-2) 내가 이미 열쇠의 인접 칸(상하좌우)에 도착해 있는 경우 */
        for (d = 0; d < 4; ++d) {
            if (mx + dx[d] != s.key.x || my + dy[d] != s.key.y) continue;

            for (j = 0; j < PLAYER_COUNT; ++j) {
                int ex, ey;
                if (!is_active_enemy(&s, j)) continue;
                ex = s.players[j].pos.x; ey = s.players[j].pos.y;
                if (abs(ex - s.key.x) + abs(ey - s.key.y) != 1) continue;

                /* 경쟁자가 내 3칸 부채꼴 공격 범위 안에 있다면 먼저 공격! */
                for (k = 0; k < 4; ++k) {
                    int fx = mx + dx[k], fy = my + dy[k];
                    if (k < 2 ? (ey == fy && abs(ex - fx) <= 1) : (ex == fx && abs(ey - fy) <= 1)) {
                        return (Command)(COMMAND_ATTACK_UP + k);
                    }
                }
                if (myHp == 1 || (s.turn % 3 != 0)) {
                    return (Command)(COMMAND_ATTACK_UP + d);
                }
            }
            return (Command)(COMMAND_PICK_UP + d);
        }

        /* B-3) [열쇠를 안 먹고 입구 근처에 매복하는 상황 판단]:
                적이 이미 열쇠 바로 앞(1칸 이내)에 도달했고 나는 열쇠에서 5칸 이상 멀리 떨어져 있으며,
                내가 탈출구 입구 쪽에 더 가까운 위치(myExitDist < distExit[key])에 있다면:
                뒤늦게 열쇠로 달려가 허탕 치는 대신, 적이 열쇠를 들고 내려올 관문/탈출구 입구에 매복한다! */
        if (minEnemyKeyDist <= 1 && myKeyDist >= 5 && closestKeyEnemy >= 0 &&
            myExitDist + 1 < distExit[s.key.y][s.key.x]) {
            int refX = s.players[closestKeyEnemy].pos.x;
            ambushMode = 1;

            /* 적이 통과해야 하는 관문 입구(isGate) 및 탈출구 입구(distExit == 1)를 매복 목표로 설정 */
            for (y = 0; y < n; ++y) {
                for (x = 0; x < n; ++x) {
                    if (isGate[y][x] && ((refX <= s.exit.x && x <= s.exit.x) || (refX >= s.exit.x && x >= s.exit.x))) {
                        if (s.terrain[y][x] == ' ' && !(x == mx && y == my)) goal[y][x] = 1;
                    }
                }
            }
            for (d = 0; d < 4; ++d) {
                x = s.exit.x + dx[d]; y = s.exit.y + dy[d];
                if (x >= 0 && y >= 0 && x < n && y < n && s.terrain[y][x] == ' ' && !(x == mx && y == my))
                    goal[y][x] = 1;
            }
        }
        /* B-4) 그 외에는 최단 경로로 열쇠 인접 칸을 목표로 설정 */
        else {
            for (d = 0; d < 4; ++d) {
                x = s.key.x + dx[d]; y = s.key.y + dy[d];
                if (x >= 0 && y >= 0 && x < n && y < n && s.terrain[y][x] == ' ')
                    goal[y][x] = 1;
            }
        }
    }
    /* ------------------------------------------------------------------
       [Case C] 다른 플레이어가 열쇠를 가지고 있음 (owner != me)
       -> 상황에 따라 입구(관문 입구 / 탈출구 입구) 매복 & 추격 강탈!
       ------------------------------------------------------------------ */
    else {
        int cx = s.players[owner].pos.x, cy = s.players[owner].pos.y;
        int carrierDistToExit = distExitAvoid[cy][cx] < INF ? distExitAvoid[cy][cx] : distExit[cy][cx];
        int myDistToExit = distExitAvoid[my][mx] < INF ? distExitAvoid[my][mx] : distExit[my][mx];

        /* C-1) 열쇠 소지자가 내 3칸 부채꼴 공격 범위(현재 또는 예측 위치) 안에 있으면 즉시 공격! */
        for (d = 0; d < 4; ++d) {
            int fx = mx + dx[d], fy = my + dy[d];
            int hitNow = (d < 2) ? (cy == fy && abs(cx - fx) <= 1) : (cx == fx && abs(cy - fy) <= 1);
            int hitPred = (d < 2) ? (predY[owner] == fy && abs(predX[owner] - fx) <= 1)
                                  : (predX[owner] == fx && abs(predY[owner] - fy) <= 1);
            if (hitNow || hitPred) {
                return (Command)(COMMAND_ATTACK_UP + d);
            }
        }

        /* C-2) 열쇠 소지자가 이미 출구 바로 옆(1칸)이고 내가 당장 타격할 수 없는 거리라면,
                즉시 '최초 열쇠 위치(key_start)' 인접 칸으로 선발 이동하여 다음 열쇠를 선점! */
        if (carrierDistToExit <= 1 && myDistToExit > 2) {
            for (d = 0; d < 4; ++d) {
                x = s.key_start.x + dx[d]; y = s.key_start.y + dy[d];
                if (x >= 0 && y >= 0 && x < n && y < n && s.terrain[y][x] == ' ' && !(x == mx && y == my))
                    goal[y][x] = 1;
            }
        }
        /* C-3) [상황별 입구 매복]:
                열쇠 소지자가 탈출구에서 4칸 이내(관문 통과 직후 또는 탈출구 접근 중)이고
                내가 탈출구 입구에 더 가깝거나 같다면(myDistToExit <= carrierDistToExit),
                탈출구 입구(distExit == 1)에서 매복하여 기습 타격한다! */
        else if (carrierDistToExit <= 4 && myDistToExit <= carrierDistToExit) {
            ambushMode = 1;
            for (d = 0; d < 4; ++d) {
                x = s.exit.x + dx[d]; y = s.exit.y + dy[d];
                if (x >= 0 && y >= 0 && x < n && y < n && s.terrain[y][x] == ' ' && !(x == mx && y == my))
                    goal[y][x] = 1;
            }
        }
        /* C-4) 소지자가 아직 관문 입구 위쪽에 있거나 멀리 있을 때는,
                소지자가 통과하려는 관문 입구 및 소지자 예상 위치로 전진 매복/추격하여 길목을 끊는다! */
        else {
            for (y = predY[owner] - 1; y <= predY[owner] + 1; ++y) {
                for (x = predX[owner] - 1; x <= predX[owner] + 1; ++x) {
                    if (x >= 0 && y >= 0 && x < n && y < n && s.terrain[y][x] == ' ' && !(x == mx && y == my))
                        goal[y][x] = 1;
                }
            }
        }
    }

    /* 현재 서 있는 칸은 목표에서 제외하여, 적이 없는데 제자리 허공 공격을 반복하지 않도록 보장 */
    goal[my][mx] = 0;

    /* 입구 매복 모드(ambushMode == 1)에서 출구 입구(distExit <= 2)에 위치해 있고
       공격 범위 안에 타격 가능한 적이 들어왔다면 즉시 기습 공격! */
    if (ambushMode && distExit[my][mx] <= 2 && atkScore[bestAtk] >= 1.5) {
        return (Command)(COMMAND_ATTACK_UP + bestAtk);
    }

    /* ---------- 6. 비보유 시(Case B, C) 목표까지의 다익스트라 최단 경로 계산 ---------- */
    head = tail = 0;
    for (y = 0; y < n; ++y) {
        for (x = 0; x < n; ++x) {
            used[y][x] = 0;
            if (goal[y][x]) {
                distPure[y][x] = 0;
                queue[tail].x = x; queue[tail].y = y; ++tail;
                distSafe[y][x] = enemyProximity[y][x] + ((occ[y][x] != -1 && occ[y][x] != me) ? 450 : 0);
            }
        }
    }
    while (head < tail) {
        Position p = queue[head++];
        for (d = 0; d < 4; ++d) {
            x = p.x + dx[d]; y = p.y + dy[d];
            if (x < 0 || y < 0 || x >= n || y >= n) continue;
            if (s.terrain[y][x] == '#' || s.terrain[y][x] == 'E' || distPure[y][x] != INF) continue;
            distPure[y][x] = distPure[p.y][p.x] + 1;
            queue[tail].x = x; queue[tail].y = y; ++tail;
        }
    }

    for (;;) {
        int bx = -1, by = -1, bestD = INF;
        for (y = 0; y < n; ++y) {
            for (x = 0; x < n; ++x) {
                if (!used[y][x] && distSafe[y][x] < bestD) {
                    bestD = distSafe[y][x];
                    bx = x; by = y;
                }
            }
        }
        if (bx < 0) break;
        used[by][bx] = 1;

        for (d = 0; d < 4; ++d) {
            int stepCost = 100;
            int distFromMe;
            x = bx + dx[d]; y = by + dy[d];
            if (x < 0 || y < 0 || x >= n || y >= n) continue;
            if (s.terrain[y][x] == '#' || s.terrain[y][x] == 'E') continue;

            distFromMe = abs(x - mx) + abs(y - my);

            if (occ[y][x] != -1 && occ[y][x] != me) {
                stepCost += (distFromMe <= 1) ? 700 : 200;
            }
            if (distFromMe == 1) {
                stepCost += attackZone[y][x] * (myHp == 1 ? 5 : 2) + moveClaim[y][x] * 3;
            }
            if (spawnBlock[y][x]) stepCost += 80;

            if (distSafe[by][bx] + stepCost < distSafe[y][x]) {
                distSafe[y][x] = distSafe[by][bx] + stepCost;
            }
        }
    }

    /* ---------- 7. 비보유 시 다음 1칸 이동 또는 입구 매복/길뚫기 공격 선택 ---------- */
    {
        int bestMove = -1, bestTotalCost = INF, bestRival = 0;
        for (d = 0; d < 4; ++d) {
            int rival = 0, totalCost;
            x = mx + dx[d]; y = my + dy[d];
            if (x < 0 || y < 0 || x >= n || y >= n) continue;
            if (s.terrain[y][x] == '#' || s.terrain[y][x] == 'E') continue;
            if (occ[y][x] != -1) continue;
            if (distSafe[y][x] >= INF) continue;

            for (j = 0; j < PLAYER_COUNT; ++j) {
                if (!is_active_enemy(&s, j)) continue;
                if (predX[j] == x && predY[j] == y) ++rival;
            }

            totalCost = distSafe[y][x]
                      + (distPure[y][x] < INF ? distPure[y][x] * 40 : 500)
                      + attackZone[y][x] * (myHp == 1 ? 6 : 2)
                      + moveClaim[y][x] * 4
                      + rival * 160;

            if (distSafe[y][x] < distSafe[my][mx] ||
                distPure[y][x] < distPure[my][mx] ||
                attackZone[my][mx] > attackZone[y][x] + 25) {
                if (totalCost < bestTotalCost) {
                    bestTotalCost = totalCost;
                    bestMove = d;
                    bestRival = rival;
                }
            }
        }

        if (bestMove >= 0) {
            if (bestRival > 0 && (myHp == 1 || (s.turn % 2 == 0))) {
                return (Command)(COMMAND_ATTACK_UP + bestMove);
            }
            return (Command)(COMMAND_UP + bestMove);
        }

        /* 최단 경로의 바로 앞 칸을 다른 플레이어가 막고 서 있다면 즉시 공격해서 길을 뚫는다 */
        for (d = 0; d < 4; ++d) {
            x = mx + dx[d]; y = my + dy[d];
            if (x < 0 || y < 0 || x >= n || y >= n) continue;
            if (occ[y][x] != -1 && occ[y][x] != me && distPure[y][x] < distPure[my][mx]) {
                return (Command)(COMMAND_ATTACK_UP + d);
            }
        }

        /* 공격 범위 안에 적이 전혀 없다면 허공에 공격하지 않고 가장 유리한 인접 빈 칸으로 이동(패트롤)한다 */
        if (atkScore[bestAtk] <= 0.0) {
            for (d = 0; d < 4; ++d) {
                int totalCost;
                x = mx + dx[d]; y = my + dy[d];
                if (x < 0 || y < 0 || x >= n || y >= n) continue;
                if (s.terrain[y][x] == '#' || s.terrain[y][x] == 'E' || occ[y][x] != -1) continue;
                totalCost = (distPure[y][x] < INF ? distPure[y][x] * 100 : 5000)
                          + attackZone[y][x] * 3 + moveClaim[y][x] * 4;
                if (totalCost < bestTotalCost) {
                    bestTotalCost = totalCost;
                    bestMove = d;
                }
            }
            if (bestMove >= 0) return (Command)(COMMAND_UP + bestMove);
        }
    }

    /* ---------- 8. 근접 적 견제 공격 또는 불가피한 대기 (열쇠 비보유 시에만 도달) ---------- */
    if (atkScore[bestAtk] > 0.0) return (Command)(COMMAND_ATTACK_UP + bestAtk);
    return (Command)(COMMAND_ATTACK_UP + safeWaitDir);
}
