#include "player_api.h"

/* 제출 파일: 어부지리 + Kite 합성 (시뮬 측정 이름 "합성 모드1 턴30").
   - 열쇠를 누가 들었거나 턴 30 미만이면 "어부지리 출구 경비"(ours_choose)로 판단한다.
   - 턴 30 이후 열쇠가 바닥에 있을 때만 "Kite"(kite_choose)로 판단한다.
   두 전략은 같은 스냅샷을 받아 각자 명령을 고른다. 상태 파일은 맨 아래 player0_choose_command가 한 번만 읽는다.
   매 턴 별도 프로세스라 static 상태는 쓰지 않는다. */
#ifndef P0_FINISH
#define P0_FINISH 1 /* 1이면 1대1 결판 규칙(살아 있는 적이 HP 1 하나뿐이고 내가 HP 2일 때 열쇠를 내려놓고 쓰러뜨린 뒤 다시 줍기) */
#endif
#ifndef P0_CONTEST
#define P0_CONTEST 400 /* 열쇠 옆에 상대가 있을 때 줍기 시도 점수. 0이면 판당 취소가 31% 줄지만(순위는 같음) 교수님 --self-test(열쇠 옆에 상대가 있어도 줍는지 검사)를 통과하지 못해서 400 유지 */
#endif
#ifndef P0_KITE_END
#define P0_KITE_END 100 /* 이 턴부터는 Kite를 끈다: 오래 끄는 판에서 Kite가 도망만 다니다 지는 경우가 영상 재현에서 확인됨 */
#endif
#ifndef P0_KITE_TURN
#define P0_KITE_TURN 30 /* 이 턴부터 열쇠가 바닥에 있으면 Kite */
#endif

/* ===================== 어부지리 출구 경비 ===================== */

/* 우리 조 전략: "어부지리 출구 경비"
   - 통로를 막는 팀과 그 팀을 노리는 팀이 서로 싸우게 두고, 우리는 공격받을 수 없는 칸만 밟는다.
     (공격은 공격자의 턴 시작 위치 기준 8방향 인접 칸만 닿으므로 위험 칸을 정확히 계산할 수 있다)
   - 열쇠 경쟁에서 이길 수 있을 때만 열쇠로 가고, 아니면 출구 접근 칸을 공격 범위로 덮는 경비 자리를 잡는다.
   - 열쇠 소유자가 접근 칸에 들어오면 쳐서 HP를 깎고, 쓰러지면 바로 옆에 떨어진 열쇠를 줍는다.
   - 열쇠를 들고 출구 옆이면 들어간다(HP 2면 턴당 피해 최대 1이라 반드시 산다).
   v2: 남이 열쇠를 들면 뒤쫓지 않고, 소유자의 출구까지 최단 경로를 예측해 먼저 도착할 수 있는 칸에서 기다린다(요격).
       게임이 길어질수록 열쇠 경쟁에 적극적으로 나서 무승부를 줄인다.
   매 턴 별도 프로세스라 static 상태는 쓰지 않는다. 모든 판단은 한 함수 안에서 한다. */
/* 튜닝값: 열쇠 경쟁 여유 칸 수, 이 턴 수마다 여유를 1칸씩 늘림 */
#ifndef P0_SLACK
#define P0_SLACK 0 /* v3: 2 -> 0. 열쇠 경쟁에서 같거나 뒤지면 바로 출구 경비로 돌린다 (rusher 5명 상대 32% -> 100%) */
#endif
#ifndef P0_LATE
#define P0_LATE 25
#endif
/* 경로 계산에서 위험 칸 1개당 추가 비용 (0이면 위험 무시) */
#ifndef P0_DW
#define P0_DW 4
#endif
/* 위험 회피 점수(튜닝값): 맞을 수 있는 칸으로 가면 감점 / 이동할 칸 옆에 다른 사람이 있으면 감점. HP 1일 때와 2일 때를 따로 둔다 */
#ifndef P0_RISK1
#define P0_RISK1 600
#endif
#ifndef P0_RISK2
#define P0_RISK2 400  /* v3: 150 -> 400. HP 2일 때도 맞을 수 있는 칸을 더 강하게 피한다 (1000부터는 너무 소극적이라 오히려 진다) */
#endif
#ifndef P0_NEAR1
#define P0_NEAR1 400
#endif
#ifndef P0_NEAR2
#define P0_NEAR2 150
#endif
/* v4 청소 모드: 열쇠가 이 턴 이후에도 바닥에 있으면(모두 서로 피하며 교착) 열쇠를 줍기 전에 통로를 막은 적을 먼저 쓰러뜨린다.
   살아 있는 적이 P0_CLEANMAX명 이하가 되면 청소를 끝내고 평소처럼 열쇠를 줍는다. (보통 판은 80턴 전에 끝나서 영향이 없다) */
#ifndef P0_CLEAN
#define P0_CLEAN 80 /* 이 턴부터 열쇠가 바닥에 있으면 청소 모드 */
#endif
#ifndef P0_HUNT
#define P0_HUNT 120 /* 남이 열쇠를 든 채 이 턴이 넘도록 못 나가면(서로 막은 교착) 소유자를 직접 쫓아 친다 */
#endif
#ifndef P0_CLEANKEY
#define P0_CLEANKEY 3
#endif
#ifndef P0_CLEANMAX
#define P0_CLEANMAX 3
#endif
#ifndef P0_CLEANATK
#define P0_CLEANATK 1
#endif
#ifndef P0_KEYMUL
#define P0_KEYMUL 4 /* 열쇠 소유자를 치는 가치 배율 */
#endif
#ifndef P0_TWOHIT
#define P0_TWOHIT 1 /* 1이면 남이 열쇠를 들었을 때 접근 칸+출구를 함께 덮는 자리에 선다 */
#endif
#ifndef P0_KEYNORISK
#define P0_KEYNORISK 1 /* 1이면 열쇠 소유자를 칠 수 있는 공격은 피격 위험 감점을 면제 */
#endif
#ifndef P0_CLEANMOVE
#define P0_CLEANMOVE 3 /* 청소 중 적에게 다가가는 이동 점수 배수 */
#endif
static Command ours_choose(const Snapshot *sp)
{
    static const int dx[4] = {0, 0, -1, 1}, dy[4] = {-1, 1, 0, 0};
    Snapshot s = *sp;
    int dk[MAP_MAX][MAP_MAX], dme[MAP_MAX][MAP_MAX], dex[MAP_MAX][MAP_MAX], dt[MAP_MAX][MAP_MAX], danger[MAP_MAX][MAP_MAX];
    int qx[MAP_MAX * MAP_MAX], qy[MAP_MAX * MAP_MAX];
    int finish = 0, clean, alive = 0, pass, h, t, x, y, d, i, j, k, me, carrier, sx = 0, sy = 0, tx = 0, ty = 0;
    int best = COMMAND_ATTACK_UP, bestScore = -1000000000;
    Position p;

    /* 1) 상태는 호출 쪽이 한 번 읽어 둔 스냅샷을 쓴다 */
    me = s.self;
    p = s.players[me].pos;
    carrier = s.key_owner == me;

    /* 2) 위험 지도: 살아 있는 적의 8방향 인접 칸 = 이번 턴에 맞을 수 있는 칸 */
    for (y = 0; y < MAP_MAX; ++y) for (x = 0; x < MAP_MAX; ++x) danger[y][x] = 0;
    for (i = 0; i < PLAYER_COUNT; ++i) {
        if (i == me || s.players[i].hp <= 0) continue;
        for (y = -1; y <= 1; ++y) for (x = -1; x <= 1; ++x) {
            int cx = s.players[i].pos.x + x, cy = s.players[i].pos.y + y;
            if ((x || y) && cx >= 0 && cy >= 0 && cx < s.size && cy < s.size && s.terrain[cy][cx] != '#') ++danger[cy][cx];
        }
    }

    /* 청소 모드 판정: 열쇠가 바닥에 있고 오래 교착이면, 살아 있는 적이 많은 동안 적을 먼저 친다 */
    for (i = 0; i < PLAYER_COUNT; ++i) if (i != me && s.players[i].hp > 0) ++alive;
    clean = !carrier && ((s.key_owner < 0 && s.turn >= P0_CLEAN && alive > P0_CLEANMAX) || (s.key_owner >= 0 && s.turn >= P0_HUNT));
    /* 청소 중에는 HP와 상관없이 위험 지도를 끈다. HP 1일 때 켜 두면 출구를 막고 선 상대에게 끝내 다가가지 못해 무승부가 난다(Debug 대전에서 확인) */
    if (clean) for (y = 0; y < MAP_MAX; ++y) for (x = 0; x < MAP_MAX; ++x) danger[y][x] = 0;

    /* 1대1 결판(P0_FINISH): 살아 있는 적이 HP 1 하나뿐이고 나는 HP 2일 때.
       - 열쇠를 든 채 출구 접근 칸이 모두 그 적의 공격 범위라 막혀 있으면, 열쇠를 든 동안은 공격이 안 되므로 열쇠를 옆 칸에 내려놓는다.
       - 열쇠가 바닥에 있으면 줍기 전에 그 적부터 쓰러뜨린다(청소 모드). 쓰러뜨리면 평소대로 줍고 출구로 간다. */
    if (P0_FINISH && alive == 1 && s.players[me].hp == MAX_HP) {
        int lone = -1;
        for (i = 0; i < PLAYER_COUNT; ++i) if (i != me && s.players[i].hp > 0) lone = i;
        if (lone >= 0 && s.players[lone].hp == 1) {
            if (!carrier && s.key_owner < 0) {
                clean = 1; finish = 1; /* 쓰러뜨리기 전에는 열쇠를 줍지 않는다 */
                for (y = 0; y < MAP_MAX; ++y) for (x = 0; x < MAP_MAX; ++x) danger[y][x] = 0;
            } else if (carrier && abs(s.players[lone].pos.x - s.exit.x) + abs(s.players[lone].pos.y - s.exit.y) <= 4) {
                int open = 0, bestd = -1, bestdir = -1;
                for (d = 0; d < 4; ++d) {
                    int nx = s.exit.x + dx[d], ny = s.exit.y + dy[d];
                    if (nx < 0 || ny < 0 || nx >= s.size || ny >= s.size || s.terrain[ny][nx] != ' ') continue;
                    if (s.players[lone].pos.x == nx && s.players[lone].pos.y == ny) continue; /* 그 적이 서 있는 칸은 못 쓴다 */
                    if (danger[ny][nx] == 0) open = 1;
                }
                if (!open) {
                    for (d = 0; d < 4; ++d) {
                        int nx = p.x + dx[d], ny = p.y + dy[d], dd;
                        if (nx < 0 || ny < 0 || nx >= s.size || ny >= s.size || s.terrain[ny][nx] != ' ') continue;
                        if (s.players[lone].pos.x == nx && s.players[lone].pos.y == ny) continue;
                        dd = abs(s.players[lone].pos.x - nx) + abs(s.players[lone].pos.y - ny);
                        if (dd > bestd) { bestd = dd; bestdir = d; }
                    }
                    if (bestdir >= 0) return (Command)(COMMAND_DROP_UP + bestdir); /* 적에게서 가장 먼 인접 칸에 내려놓는다 */
                }
            }
        }
    }

    /* 3) BFS 네 번: pass0 = 열쇠(또는 소유자/출구)까지, pass1 = 나로부터, pass2 = 출구로부터,
          pass3 = 이번 턴 목표까지 거리. 목표는 pass3 직전에 앞의 세 거리표로 정한다. */
    for (pass = 0; pass < 4; ++pass) {
        int (*dist)[MAP_MAX] = pass == 0 ? dk : pass == 1 ? dme : pass == 2 ? dex : dt;
        if (pass == 0) {
            if (carrier) { sx = s.exit.x; sy = s.exit.y; }
            else if (s.key_owner < 0) { sx = s.key.x; sy = s.key.y; }
            else { sx = s.players[s.key_owner].pos.x; sy = s.players[s.key_owner].pos.y; }
        } else if (pass == 1) { sx = p.x; sy = p.y; }
        else if (pass == 2) { sx = s.exit.x; sy = s.exit.y; }
        else {
            /* 목표 정하기 */
            int myd = dk[p.y][p.x], enemyMin = 1000000, gx = -1, gy = -1, bs = 0;
            for (i = 0; i < PLAYER_COUNT; ++i)
                if (i != me && s.players[i].hp > 0 && dk[s.players[i].pos.y][s.players[i].pos.x] < enemyMin)
                    enemyMin = dk[s.players[i].pos.y][s.players[i].pos.x];
            /* 경비 자리: 출구 접근 칸(출구의 상하좌우 바닥)을 가장 많이 공격 범위로 덮는 칸 */
            for (y = 0; y < s.size; ++y) for (x = 0; x < s.size; ++x) {
                int occupied = 0, block, sc, ax, ay;
                if (s.terrain[y][x] != ' ' || dme[y][x] >= 1000000) continue;
                for (j = 0; j < PLAYER_COUNT; ++j)
                    if (j != me && s.players[j].hp > 0 && s.players[j].pos.x == x && s.players[j].pos.y == y) occupied = 1;
                if (occupied) continue;
                /* 내가 접근 칸 위에 서면 그 길은 막힌다 */
                block = (abs(x - s.exit.x) + abs(y - s.exit.y)) == 1;
                for (d = 0; d < 4; ++d) {
                    sc = block * 2;
                    for (k = -1; k <= 1; ++k) {
                        ax = x + dx[d] + (d < 2 ? k : 0);
                        ay = y + dy[d] + (d < 2 ? 0 : k);
                        if (ax < 0 || ay < 0 || ax >= s.size || ay >= s.size || s.terrain[ay][ax] == '#') continue;
                        if (ax == s.exit.x && ay == s.exit.y) sc += 1;
                        else if (abs(ax - s.exit.x) + abs(ay - s.exit.y) == 1) sc += 2;
                    }
                    if (sc > bs) { bs = sc; gx = x; gy = y; }
                }
            }
            /* 기본 목표: 열쇠를 들었으면 출구, 바닥에 있으면 열쇠 */
            if (carrier) { tx = s.exit.x; ty = s.exit.y; }
            else if (s.key_owner < 0) { tx = s.key.x; ty = s.key.y; }
            else { tx = p.x; ty = p.y; }
            if (!carrier && s.key_owner < 0) {
                /* 열쇠 경쟁에서 확실히 지면 경비. 게임이 길어질수록 여유를 늘려 적극적으로 줍는다. */
                if (myd > enemyMin + P0_SLACK + s.turn / P0_LATE && gx >= 0) { tx = gx; ty = gy; }
            } else if (!carrier) {
                /* 남이 열쇠를 들었다: 경비 자리에 제때 도착하면 경비, 아니면 예측 경로 위에서 요격 */
                Position c = s.players[s.key_owner].pos;
                int tc = dex[c.y][c.x], step, found = 0;
                if (P0_TWOHIT) {
                    /* 소유자가 출구로 들어오는 접근 칸 a를 찾고, a와 출구를 한 번의 공격 부채꼴로 함께 덮는 칸(a·출구 옆 4곳) 중
                       소유자가 a에 닿기 전에 도착할 수 있는 가장 가까운 칸에 선다 (HP2 소유자를 두 번 쳐서 죽이기 위함) */
                    int a0x = -1, a0y = -1, ad = 1000000, ux, uy, q, pcx[4], pcy[4], bpd = 1000000;
                    for (d = 0; d < 4; ++d) {
                        int nx = s.exit.x + dx[d], ny = s.exit.y + dy[d];
                        if (nx < 0 || ny < 0 || nx >= s.size || ny >= s.size || s.terrain[ny][nx] != ' ') continue;
                        if (dk[ny][nx] < ad) { ad = dk[ny][nx]; a0x = nx; a0y = ny; }
                    }
                    if (a0x >= 0 && ad < 1000000) {
                        ux = s.exit.x - a0x; uy = s.exit.y - a0y;
                        pcx[0] = a0x + uy; pcy[0] = a0y + ux; pcx[1] = a0x - uy; pcy[1] = a0y - ux;
                        pcx[2] = s.exit.x + uy; pcy[2] = s.exit.y + ux; pcx[3] = s.exit.x - uy; pcy[3] = s.exit.y - ux;
                        for (q = 0; q < 4; ++q) {
                            int occ = 0;
                            if (pcx[q] < 0 || pcy[q] < 0 || pcx[q] >= s.size || pcy[q] >= s.size || s.terrain[pcy[q]][pcx[q]] != ' ') continue;
                            if (dme[pcy[q]][pcx[q]] > ad - 1 || dme[pcy[q]][pcx[q]] >= bpd) continue;
                            for (j = 0; j < PLAYER_COUNT; ++j)
                                if (j != me && s.players[j].hp > 0 && s.players[j].pos.x == pcx[q] && s.players[j].pos.y == pcy[q]) occ = 1;
                            if (occ) continue;
                            bpd = dme[pcy[q]][pcx[q]]; tx = pcx[q]; ty = pcy[q]; found = 1;
                        }
                    }
                }
                if (!found && gx >= 0 && dme[gy][gx] <= tc - 2) { tx = gx; ty = gy; found = 1; }
                for (step = 1; !found && step <= tc && step < MAP_MAX * MAP_MAX; ++step) {
                    /* 소유자는 출구 거리가 1씩 줄어드는 칸으로 간다고 가정하고 한 칸 전진 */
                    int moved = 0, bestA = 1000000, ax, ay;
                    for (d = 0; d < 4 && !moved; ++d) {
                        int nx = c.x + dx[d], ny = c.y + dy[d];
                        if (nx < 0 || ny < 0 || nx >= s.size || ny >= s.size || dex[ny][nx] != dex[c.y][c.x] - 1) continue;
                        c.x = nx; c.y = ny; moved = 1;
                    }
                    if (!moved) break;
                    /* 소유자가 step번째에 설 칸을 칠 수 있는(8방향 인접) 칸에 step-1턴 안에 도착할 수 있나 */
                    for (ay = c.y - 1; ay <= c.y + 1; ++ay) for (ax = c.x - 1; ax <= c.x + 1; ++ax) {
                        if (ax < 0 || ay < 0 || ax >= s.size || ay >= s.size || (ax == c.x && ay == c.y)) continue;
                        if (s.terrain[ay][ax] != ' ' || dme[ay][ax] > step - 1 || dme[ay][ax] >= bestA) continue;
                        bestA = dme[ay][ax]; tx = ax; ty = ay; found = 1;
                    }
                }
                if (!found) {
                    /* 요격 불가: 경비 자리, 그것도 없으면 소유자를 직접 쫓는다 */
                    if (gx >= 0) { tx = gx; ty = gy; }
                    else { tx = s.players[s.key_owner].pos.x; ty = s.players[s.key_owner].pos.y; }
                }
            }
            if (clean) {
                /* 청소: 가장 가까운 적(HP 1이면 2칸 가까운 것으로 침)을 찾아간다 */
                int bd = 1000000;
                if (s.key_owner >= 0) { tx = s.players[s.key_owner].pos.x; ty = s.players[s.key_owner].pos.y; bd = -1; }
                for (i = 0; bd >= 0 && i < PLAYER_COUNT; ++i) {
                    int v;
                    if (i == me || s.players[i].hp <= 0 || dme[s.players[i].pos.y][s.players[i].pos.x] >= 1000000) continue;
                    v = dme[s.players[i].pos.y][s.players[i].pos.x] - (s.players[i].hp == 1 ? 2 : 0);
                    if (v < bd) { bd = v; tx = s.players[i].pos.x; ty = s.players[i].pos.y; }
                }
            }
            sx = tx; sy = ty;
        }
        if (pass < 3 || P0_DW <= 0) { /* pass 3은 아래 위험 가중 계산이 거리표를 처음부터 다시 채우므로 일반 BFS가 필요 없다 */
            for (y = 0; y < MAP_MAX; ++y) for (x = 0; x < MAP_MAX; ++x) dist[y][x] = 1000000;
            dist[sy][sx] = 0; h = t = 0; qx[t] = sx; qy[t++] = sy;
            while (h < t) {
                x = qx[h]; y = qy[h++];
                for (d = 0; d < 4; ++d) {
                    int nx = x + dx[d], ny = y + dy[d];
                    if (nx < 0 || ny < 0 || nx >= s.size || ny >= s.size || s.terrain[ny][nx] == '#') continue;
                    if (s.terrain[ny][nx] == 'E' && !carrier) continue;
                    if (dist[ny][nx] != 1000000) continue;
                    dist[ny][nx] = dist[y][x] + 1; qx[t] = nx; qy[t++] = ny;
                }
            }
        }
        if (pass == 3 && P0_DW > 0) {
            /* v2: 목표 경로는 위험 칸을 돌아가게 한다. 칸에 들어가는 비용 = 1 + P0_DW x 위험도.
               (통로를 막고 선 적이 있으면 다른 통로로 돌아간다) 바뀌는 값이 없을 때까지 반복 완화 */
            int changed = 1;
            for (y = 0; y < MAP_MAX; ++y) for (x = 0; x < MAP_MAX; ++x) dist[y][x] = 1000000;
            dist[sy][sx] = 0;
            while (changed) {
                changed = 0;
                for (y = 0; y < s.size; ++y) for (x = 0; x < s.size; ++x) {
                    int cost;
                    if (dist[y][x] >= 1000000) continue;
                    cost = dist[y][x] + 1 + P0_DW * danger[y][x];
                    for (d = 0; d < 4; ++d) {
                        int nx = x + dx[d], ny = y + dy[d];
                        if (nx < 0 || ny < 0 || nx >= s.size || ny >= s.size || s.terrain[ny][nx] == '#') continue;
                        if (s.terrain[ny][nx] == 'E' && !carrier) continue;
                        if (cost < dist[ny][nx]) { dist[ny][nx] = cost; changed = 1; }
                    }
                }
            }
        }
    }

    /* 4) 이동 4개, 공격 4개, 집기 4개를 점수로 비교 */
    for (k = 0; k < 12; ++k) {
        int kind = k / 4, dir = k % 4, nx = p.x, ny = p.y, score = 0, risk, near = 0, keyhit = 0;
        if (kind == 1 && carrier) continue; /* 교수님 패치: 열쇠를 든 동안은 공격이 안 된다. 이동만 후보로 둔다 */
        if (kind == 0) {
            nx += dx[dir]; ny += dy[dir];
            if (nx < 0 || ny < 0 || nx >= s.size || ny >= s.size || s.terrain[ny][nx] == '#') continue;
            if (s.terrain[ny][nx] == 'E' && !carrier) continue;
            for (j = 0; j < PLAYER_COUNT; ++j) {
                if (j == me || s.players[j].hp <= 0) continue;
                if (s.players[j].pos.x == nx && s.players[j].pos.y == ny) near = 100; /* 남이 서 있는 칸 */
                else if (abs(s.players[j].pos.x - nx) + abs(s.players[j].pos.y - ny) == 1) ++near; /* 같은 칸 충돌 위험 */
            }
            if (near >= 100) continue;
            /* 열쇠 들고 출구 진입 (아무도 출구로 못 들어오므로 충돌도 없다).
               HP 1이고 출구 칸이 공격 범위면 맞아 죽을 수 있지만(사망 판정이 승리보다 먼저), 위험 때 멈추면
               시뮬 승률이 더 낮았다(73.1% -> 72.0%). 그래서 HP와 상관없이 바로 들어간다. */
            if (s.terrain[ny][nx] == 'E') return (Command)k;
            if (!clean) score -= near * (s.players[me].hp == 1 ? P0_NEAR1 : P0_NEAR2);
            if (dt[ny][nx] < 1000000) score += (dt[p.y][p.x] - dt[ny][nx]) * (clean ? 40 * P0_CLEANMOVE : 40);
        } else if (kind == 2) {
            int contest = 0;
            if (finish) continue;
            if (clean) { /* 청소 중에는 열쇠 주변에 적이 가까우면(3칸 이내) 줍지 않는다 */
                int nearEnemy = 0;
                for (j = 0; j < PLAYER_COUNT; ++j)
                    if (j != me && s.players[j].hp > 0 && abs(s.players[j].pos.x - s.key.x) + abs(s.players[j].pos.y - s.key.y) <= P0_CLEANKEY) nearEnemy = 1;
                if (nearEnemy) continue;
            }
            if (s.key_owner != -1 || p.x + dx[dir] != s.key.x || p.y + dy[dir] != s.key.y) continue;
            for (j = 0; j < PLAYER_COUNT; ++j)
                if (j != me && s.players[j].hp > 0 && abs(s.players[j].pos.x - s.key.x) + abs(s.players[j].pos.y - s.key.y) == 1) ++contest;
            score += contest ? P0_CONTEST : 2000; /* 경쟁 줍기도 시도: 실패해도 피해 1, 성공하면 열쇠 (HP 1에서 막으면 승률이 63%로 폭락해 그대로 둔다) */
        } else {
            /* 공격: 각 적이 갈 수 있는 위치(제자리+이동 4곳) 중 부채꼴에 들어가는 비율만큼 가치 */
            for (j = 0; j < PLAYER_COUNT; ++j) {
                int px[5], py[5], n = 0, cov = 0, wsum = 0, m, val;
                if (j == me || s.players[j].hp <= 0) continue;
                if (abs(s.players[j].pos.x - p.x) > 3 || abs(s.players[j].pos.y - p.y) > 3) continue;
                px[n] = s.players[j].pos.x; py[n++] = s.players[j].pos.y;
                for (d = 0; d < 4; ++d) {
                    int ex = s.players[j].pos.x + dx[d], ey = s.players[j].pos.y + dy[d];
                    if (ex < 0 || ey < 0 || ex >= s.size || ey >= s.size || s.terrain[ey][ex] == '#') continue;
                    if (s.terrain[ey][ex] == 'E' && s.key_owner != j) continue;
                    if (ex == p.x && ey == p.y) continue;
                    px[n] = ex; py[n++] = ey;
                }
                for (m = 0; m < n; ++m) {
                    /* 열쇠 소유자는 출구로 갈 확률이 높다고 보고 가중치 3 */
                    int w = (j == s.key_owner && px[m] == s.exit.x && py[m] == s.exit.y) ? 3 : 1;
                    int fx = px[m] - (p.x + dx[dir]), fy = py[m] - (p.y + dy[dir]);
                    wsum += w;
                    if (dir < 2 ? (fy == 0 && fx >= -1 && fx <= 1) : (fx == 0 && fy >= -1 && fy <= 1)) cov += w;
                }
                val = s.players[j].hp == 1 ? 250 : 80;
                if (j == s.key_owner) { val *= P0_KEYMUL; if (cov) keyhit = 1; }
                score += val * cov / wsum;
            }
            if (clean) score *= P0_CLEANATK;
        }
        /* 도착(또는 제자리) 칸의 피격 위험 */
        risk = danger[ny][nx];
        if (risk && !(P0_KEYNORISK && keyhit)) score -= (s.players[me].hp == 1 ? P0_RISK1 : P0_RISK2) * (carrier ? 2 : 1) + 50 * (risk - 1);
        if (score > bestScore) { bestScore = score; best = k; }
    }
    return (Command)best;
}

/* ===================== Kite ===================== */

/* "Kite" 전략: 거리 두기 + 경로 예측. 이 파일에서는 턴 30 이후 열쇠가 바닥에 있을 때만 불린다(진입점 참고).
   그래서 아래 모드 중 RUN(열쇠를 든 경우)과 GUARD(남이 열쇠를 든 경우)는 이 파일에서 실행되지 않고, 단독 Kite일 때만 쓰인다.
   규칙에서 뽑은 사실 두 가지가 전부의 바탕이다.
     (1) 공격은 공격자의 "턴 시작 위치" 주변 8칸(부채꼴 3칸)만 닿는다. 한 칸 이상 떨어져 있으면 절대 못 맞는다.
     (2) 피해는 "이동 후 위치"로 판정한다. 인접한 추격자에게서 한 칸 물러나면 안 맞고, 거리 2에서 다가오는 상대는
         내 공격 부채꼴 안으로 스스로 들어온다.
   모드:
     RUN    열쇠를 들었다. 적마다 "몇 턴 뒤에 어디를 칠 수 있나"를 BFS로 계산해 위협 시간표를 만들고,
            (칸, 시각) 단위로 위협을 피하는 최단 경로를 찾는다 -> 추격자 경로를 피해 맵을 우회해서 출구로 간다.
     FETCH  열쇠 경쟁에서 앞서거나 경쟁자가 없다. 같은 경로 탐색으로 열쇠 옆 칸까지 가서 줍는다.
     DUEL   열쇠 근처 경쟁자가 정확히 1명(1대1). 상대 칸 바로 옆(거리1)에는 안 서고, 거리 2를 유지하며 상대가 다가올 칸을
            예측해 그 칸을 공격 부채꼴로 덮는다. 붙으면 먼저 물러난다. 갈 수 있는 칸이 적은 구석은 피한다.
     LOITER 경쟁자가 2명 이상. 열쇠 주변 3~6칸 고리를 안전한 칸만 골라 돌아다니며 기다린다(다가오는 적은 공격).
     GUARD  남이 열쇠를 들었다. 접근 칸+출구를 함께 덮는 자리 / 예측 경로 위의 요격 칸으로 가서 소유자를 친다.
   한 칸으로 가는 후보마다 "맞을 확률(상대가 같은 칸으로 들어와 충돌하는 경우 포함)"을 계산해 감점하므로,
   내가 갈 칸으로 상대가 들어오는 경우도 피한다. 매 턴 별도 프로세스라 static 상태는 쓰지 않는다. */
#ifndef KK_K2
#define KK_K2 250      /* 위협받는 (칸,시각) 한 번당 경로 비용, HP2 */
#endif
#ifndef KK_K1
#define KK_K1 900      /* HP1 */
#endif
#ifndef KK_PA
#define KK_PA 70       /* 상대 공격 범위 안에 서면 맞을 확률(%) */
#endif
#ifndef KK_PC
#define KK_PC 35       /* 상대가 내가 갈 칸으로 들어와 충돌할 확률(%) */
#endif
#ifndef KK_HURT2
#define KK_HURT2 300
#endif
#ifndef KK_HURT1
#define KK_HURT1 700
#endif
#ifndef KK_KC
#define KK_KC 5        /* 열쇠까지 이 걸음 이하인 적 = 경쟁자 (7 -> 5: 시나리오 19개 평균 +0.4%p, 시드 두 묶음에서 모두 상승) */
#endif
#ifndef KK_KE
#define KK_KE 4        /* 나까지 이 걸음 이하인 적 = 경쟁자 */
#endif
#ifndef KK_FORCE
#define KK_FORCE 100   /* 이 턴부터는 교착을 끝내려고 위험을 감수하고 열쇠로 간다 */
#endif
#ifndef KK_HUNT
#define KK_HUNT 120    /* 남이 열쇠를 든 채 이 턴이 넘으면 소유자를 직접 쫓는다 */
#endif
#ifndef KK_EW
#define KK_EW 60      /* 기다리는 동안 출구까지 거리 1칸당 감점 */
#endif
#ifndef KK_GH
#define KK_GH 300      /* 경비 중 피격 감점을 이 퍼센트로 나눈다(100=그대로, 300=1/3) */
#endif
#ifndef KK_KEYMUL
#define KK_KEYMUL 4
#endif
#ifndef KK_PW
#define KK_PW 10       /* 경로 비용 1칸당 점수 */
#endif
#ifndef KK_DELAY
#define KK_DELAY 1     /* 1이면 HP1로 열쇠가 가까울 때 "언제, 어디서 줍는지"까지 경로로 비교해 줍기를 미룰 수 있다 */
#endif
#ifndef KK_DLYD
#define KK_DLYD 2      /* 열쇠까지 이 걸음 이하일 때만 줍기 시점을 비교한다 */
#endif
#ifndef KK_DCOMP
#define KK_DCOMP 300   /* 적이 열쇠를 집을 수 있는 시각 이후에 줍는 계획에 붙이는 비용 (0~300은 시뮬 승률 동일, 600부터는 우회를 안 한다) */
#endif
#define KK_INF 1000000
#define KK_TMAX 100 /* 경로 탐색이 내다보는 최대 시각. 큰 맵에서 열쇠까지 40걸음을 넘어도 경로를 찾도록 넉넉히 둔다 */
#define KK_M MAP_MAX

enum { KK_RUN, KK_FETCH, KK_DUEL, KK_LOITER, KK_GUARD };
static const int KDX[4] = {0, 0, -1, 1}, KDY[4] = {-1, 1, 0, 0};

static int kk_ok(const Snapshot *s, int x, int y) { return x >= 0 && y >= 0 && x < s->size && y < s->size && s->terrain[y][x] != '#'; }
static int kk_cheb(int ax, int ay, int bx, int by) { int a = abs(ax - bx), b = abs(ay - by); return a > b ? a : b; }
static int kk_infan(int px, int py, int dir, int qx, int qy)
{
    int fx = qx - (px + KDX[dir]), fy = qy - (py + KDY[dir]);
    return dir < 2 ? (fy == 0 && abs(fx) <= 1) : (fx == 0 && abs(fy) <= 1);
}
static int kk_occ(const Snapshot *s, int me, int x, int y)
{
    int i;
    for (i = 0; i < PLAYER_COUNT; ++i) if (i != me && s->players[i].hp > 0 && s->players[i].pos.x == x && s->players[i].pos.y == y) return 1;
    return 0;
}

/* 한 점에서 모든 칸까지 걸음 수 (플레이어는 무시, 출구는 allowE일 때만 통과) */
static void kk_bfs(const Snapshot *s, int sx, int sy, int allowE, int dist[KK_M][KK_M])
{
    int qx[KK_M * KK_M], qy[KK_M * KK_M], h = 0, t = 0, x, y, d;
    for (y = 0; y < KK_M; ++y) for (x = 0; x < KK_M; ++x) dist[y][x] = KK_INF;
    dist[sy][sx] = 0; qx[t] = sx; qy[t++] = sy;
    while (h < t) {
        x = qx[h]; y = qy[h++];
        for (d = 0; d < 4; ++d) {
            int nx = x + KDX[d], ny = y + KDY[d];
            if (!kk_ok(s, nx, ny) || (s->terrain[ny][nx] == 'E' && !allowE) || dist[ny][nx] != KK_INF) continue;
            dist[ny][nx] = dist[y][x] + 1; qx[t] = nx; qy[t++] = ny;
        }
    }
}

/* (칸, 시각)별 위협을 피하는 경로의 최소 비용 = 걸음 수 + K x (위협받는 도착 횟수).
   시각 tau에 칸 n에 도착해 맞으려면 어떤 적이 tau-1번 움직여 n의 8방향 이웃에 설 수 있어야 한다: R[n] <= tau-1. */
static int kk_route(const Snapshot *s, int R[KK_M][KK_M], int sx, int sy, int tau0, unsigned char goal[KK_M][KK_M], int carrier, int K)
{
    int cur[KK_M][KK_M], nxt[KK_M][KK_M], x, y, d, tau, best = KK_INF;
    for (y = 0; y < KK_M; ++y) for (x = 0; x < KK_M; ++x) cur[y][x] = KK_INF;
    cur[sy][sx] = 0;
    if (goal[sy][sx]) return 0;
    for (tau = tau0 + 1; tau <= tau0 + KK_TMAX; ++tau) {
        for (y = 0; y < s->size; ++y) for (x = 0; x < s->size; ++x) nxt[y][x] = KK_INF;
        for (y = 0; y < s->size; ++y) for (x = 0; x < s->size; ++x) {
            if (cur[y][x] >= KK_INF) continue;
            for (d = -1; d < 4; ++d) {
                int nx = x + (d < 0 ? 0 : KDX[d]), ny = y + (d < 0 ? 0 : KDY[d]), c;
                if (!kk_ok(s, nx, ny) || (s->terrain[ny][nx] == 'E' && !carrier)) continue;
                c = cur[y][x] + 1 + (R[ny][nx] <= tau - 1 ? K : 0);
                if (c < nxt[ny][nx]) nxt[ny][nx] = c;
            }
        }
        for (y = 0; y < s->size; ++y) for (x = 0; x < s->size; ++x) {
            cur[y][x] = nxt[y][x];
            if (goal[y][x] && cur[y][x] < best) best = cur[y][x];
        }
        if (best < KK_INF && best <= tau - tau0) break;
    }
    return best;
}

/* kk_route에 "열쇠 줍기"를 끼운 버전. layer 0 = 열쇠 없음(아직 못 주운 상태), 1 = 열쇠를 든 상태.
   열쇠 옆 칸에서 제자리 1턴을 써서 줍고, 그 뒤 출구까지 간다. 줍는 걸음이 tcomp 이상이면(적이 먼저/동시에 집을 수 있음) dcomp를 더한다. */
static int kk_route2(const Snapshot *s, int R[KK_M][KK_M], int sx, int sy, int tau0, int layer, int K, int tcomp, int dcomp)
{
    int c0[KK_M][KK_M], c1[KK_M][KK_M], n0[KK_M][KK_M], n1[KK_M][KK_M], x, y, d, tau, best = KK_INF;
    for (y = 0; y < KK_M; ++y) for (x = 0; x < KK_M; ++x) { c0[y][x] = KK_INF; c1[y][x] = KK_INF; }
    if (layer) { if (sx == s->exit.x && sy == s->exit.y) return 0; c1[sy][sx] = 0; } else c0[sy][sx] = 0;
    for (tau = tau0 + 1; tau <= tau0 + KK_TMAX; ++tau) {
        for (y = 0; y < s->size; ++y) for (x = 0; x < s->size; ++x) { n0[y][x] = KK_INF; n1[y][x] = KK_INF; }
        for (y = 0; y < s->size; ++y) for (x = 0; x < s->size; ++x) {
            if (c0[y][x] < KK_INF) {
                for (d = -1; d < 4; ++d) {
                    int nx = x + (d < 0 ? 0 : KDX[d]), ny = y + (d < 0 ? 0 : KDY[d]), c;
                    if (!kk_ok(s, nx, ny) || s->terrain[ny][nx] == 'E') continue;
                    c = c0[y][x] + 1 + (R[ny][nx] <= tau - 1 ? K : 0);
                    if (c < n0[ny][nx]) n0[ny][nx] = c;
                }
                if (abs(x - s->key.x) + abs(y - s->key.y) == 1) {
                    int c = c0[y][x] + 1 + (R[y][x] <= tau - 1 ? K : 0) + (tau >= tcomp ? dcomp : 0);
                    if (c < n1[y][x]) n1[y][x] = c;
                }
            }
            if (c1[y][x] < KK_INF) {
                for (d = -1; d < 4; ++d) {
                    int nx = x + (d < 0 ? 0 : KDX[d]), ny = y + (d < 0 ? 0 : KDY[d]), c;
                    if (!kk_ok(s, nx, ny)) continue;
                    c = c1[y][x] + 1 + (R[ny][nx] <= tau - 1 ? K : 0);
                    if (c < n1[ny][nx]) n1[ny][nx] = c;
                }
            }
        }
        for (y = 0; y < s->size; ++y) for (x = 0; x < s->size; ++x) { c0[y][x] = n0[y][x]; c1[y][x] = n1[y][x]; }
        if (c1[s->exit.y][s->exit.x] < best) best = c1[s->exit.y][s->exit.x];
        if (best < KK_INF && best <= tau - tau0) break;
    }
    return best;
}

/* 적 e가 이번 턴 끝에 있을 만한 위치 5곳(제자리+이동 4)과 가중치. 목표 쪽으로 한 걸음 가는 칸에 가중치를 더 준다. */
static int kk_epos(const Snapshot *s, int e, int gd[KK_M][KK_M], int qx[5], int qy[5], int w[5])
{
    int n = 0, d, bestd = KK_INF, bi = -1;
    qx[0] = s->players[e].pos.x; qy[0] = s->players[e].pos.y; w[0] = 2; n = 1;
    for (d = 0; d < 4; ++d) {
        int nx = qx[0] + KDX[d], ny = qy[0] + KDY[d];
        if (!kk_ok(s, nx, ny) || (s->terrain[ny][nx] == 'E' && s->key_owner != e) || kk_occ(s, e, nx, ny)) continue;
        qx[n] = nx; qy[n] = ny; w[n] = 1;
        if (gd[ny][nx] < bestd) { bestd = gd[ny][nx]; bi = n; }
        ++n;
    }
    if (bi >= 0) w[bi] = 4;
    return n;
}

/* 내가 (px,py)에서 dir로 공격할 때 기대 가치 */
static int kk_hit(const Snapshot *s, int me, int px, int py, int dir, int dme[KK_M][KK_M], int dex[KK_M][KK_M], int only)
{
    int e, total = 0;
    for (e = 0; e < PLAYER_COUNT; ++e) {
        int qx[5], qy[5], w[5], n, m, wsum = 0, cov = 0, val;
        int (*gd)[KK_M];
        if (e == me || s->players[e].hp <= 0 || (only >= 0 && e != only)) continue;
        if (kk_cheb(s->players[e].pos.x, s->players[e].pos.y, px, py) > 3) continue;
        gd = e == s->key_owner ? dex : dme;
        n = kk_epos(s, e, gd, qx, qy, w);
        for (m = 0; m < n; ++m) {
            if (m > 0 && qx[m] == px && qy[m] == py) continue; /* 내가 서 있는 칸으로는 못 들어온다 */
            wsum += w[m];
            if (kk_infan(px, py, dir, qx[m], qy[m])) cov += w[m];
        }
        if (!wsum) continue;
        val = s->players[e].hp == 1 ? 250 : 100;
        if (e == s->key_owner) val *= KK_KEYMUL;
        total += val * cov / wsum;
    }
    return total;
}

/* (mx,my)에 서 있을 때 이번 턴 피해를 받을 확률(%). moved면 상대가 같은 칸으로 들어와 충돌하는 경우도 센다 */
static int kk_hurtpct(const Snapshot *s, int me, int mx, int my, int moved)
{
    int i, keep = 100;
    for (i = 0; i < PLAYER_COUNT; ++i) {
        int ex, ey;
        if (i == me || s->players[i].hp <= 0) continue;
        ex = s->players[i].pos.x; ey = s->players[i].pos.y;
        if (kk_cheb(ex, ey, mx, my) <= 1) keep = keep * (100 - KK_PA) / 100;
        if (moved && abs(ex - mx) + abs(ey - my) == 1) keep = keep * (100 - KK_PC) / 100;
    }
    return 100 - keep;
}

/* (mx,my) 주변에서 안전하게 갈 수 있는 칸 수 (구석 방지) */
static int kk_mob(const Snapshot *s, int me, int mx, int my)
{
    int d, c = 0, i;
    for (d = 0; d < 4; ++d) {
        int nx = mx + KDX[d], ny = my + KDY[d], safe = 1;
        if (!kk_ok(s, nx, ny) || s->terrain[ny][nx] == 'E' || kk_occ(s, me, nx, ny)) continue;
        for (i = 0; i < PLAYER_COUNT; ++i)
            if (i != me && s->players[i].hp > 0 && kk_cheb(s->players[i].pos.x, s->players[i].pos.y, nx, ny) <= 1) safe = 0;
        c += safe;
    }
    return c;
}

static Command kite_choose(const Snapshot *sp)
{
    Snapshot s = *sp;
    int dme[KK_M][KK_M], dkey[KK_M][KK_M], dex[KK_M][KK_M], dcar[KK_M][KK_M], de[KK_M][KK_M], R[KK_M][KK_M];
    unsigned char goal[KK_M][KK_M];
    int x, y, d, i, k, u, me, carrier, mode, nC = 0, rival = -1, K, hurtUnit, minE = KK_INF, fetchCost = KK_INF;
    int best = COMMAND_ATTACK_UP, bestScore = -2000000000;
    int stayPos = 0, stayInit = 0, delay = 0, tcomp = KK_INF;
    Position p;

    /* 1) 상태는 호출 쪽이 한 번 읽어 둔 스냅샷을 쓴다 */
    me = s.self; p = s.players[me].pos; carrier = s.key_owner == me;
    K = s.players[me].hp == 1 ? KK_K1 : KK_K2;
    hurtUnit = s.players[me].hp == 1 ? KK_HURT1 : KK_HURT2;
    if (s.turn >= KK_FORCE) { K /= 3; hurtUnit /= 3; }

    /* 2) 거리표: 나, 열쇠(바닥일 때), 출구. 위협 시간표 R[n] = 어떤 적이 n의 8방향 이웃에 서는 데 걸리는 최소 걸음 */
    kk_bfs(&s, p.x, p.y, carrier, dme);
    kk_bfs(&s, s.key.x, s.key.y, 0, dkey);
    kk_bfs(&s, s.exit.x, s.exit.y, 0, dex);
    for (y = 0; y < KK_M; ++y) for (x = 0; x < KK_M; ++x) R[y][x] = KK_INF;
    for (i = 0; i < PLAYER_COUNT; ++i) {
        if (i == me || s.players[i].hp <= 0) continue;
        kk_bfs(&s, s.players[i].pos.x, s.players[i].pos.y, s.key_owner == i, de);
        for (y = 0; y < s.size; ++y) for (x = 0; x < s.size; ++x) {
            if (!kk_ok(&s, x, y)) continue;
            for (d = 0; d < 9; ++d) {
                int ux = x + d % 3 - 1, uy = y + d / 3 - 1;
                if (d == 4 || !kk_ok(&s, ux, uy)) continue;
                if (de[uy][ux] < R[y][x]) R[y][x] = de[uy][ux];
            }
        }
    }
    memset(goal, 0, sizeof goal);

    /* 3) 모드 결정 */
    if (carrier) {
        mode = KK_RUN; goal[s.exit.y][s.exit.x] = 1;
        /* 출구 바로 옆이고 HP 2면 어떤 공격도 한 번에 죽이지 못하니 그대로 들어간다 */
        if (s.players[me].hp == 2)
            for (d = 0; d < 4; ++d) if (p.x + KDX[d] == s.exit.x && p.y + KDY[d] == s.exit.y) return (Command)d;
    } else if (s.key_owner >= 0) {
        mode = KK_GUARD;
    } else {
        int dk_me = dkey[p.y][p.x];
        for (i = 0; i < PLAYER_COUNT; ++i) {
            int dke;
            if (i == me || s.players[i].hp <= 0) continue;
            dke = dkey[s.players[i].pos.y][s.players[i].pos.x];
            if (dke < minE) minE = dke;
            if (dke <= KK_KC || dme[s.players[i].pos.y][s.players[i].pos.x] <= KK_KE) { ++nC; rival = i; }
        }
        for (d = 0; d < 4; ++d) {
            int kx = s.key.x + KDX[d], ky = s.key.y + KDY[d];
            if (kk_ok(&s, kx, ky) && s.terrain[ky][kx] != 'E') goal[ky][kx] = 1;
        }
        fetchCost = kk_route(&s, R, p.x, p.y, 0, goal, 0, K);
        if (nC == 0 || dk_me < minE || s.turn >= KK_FORCE) mode = KK_FETCH;
        else mode = nC == 1 ? KK_DUEL : KK_LOITER;
        /* 경쟁에서 앞서도 가는 길이 위협을 지나야 하면 기다리거나 상대를 상대한다 */
        if (mode == KK_FETCH && nC > 0 && s.turn < KK_FORCE && fetchCost >= K) mode = nC == 1 ? KK_DUEL : KK_LOITER;
    }
    /* 열쇠를 줍는 시점까지 경로에 넣어 비교한다: tcomp = 적이 가장 빨리 열쇠를 집는 걸음 수(이 이후에 줍는 계획은 비용을 더 낸다) */
    if (KK_DELAY && mode == KK_FETCH && s.players[me].hp == 1 && dkey[p.y][p.x] <= KK_DLYD) {
        delay = 1;
        for (i = 0; i < PLAYER_COUNT; ++i)
            if (i != me && s.players[i].hp > 0 && dkey[s.players[i].pos.y][s.players[i].pos.x] < tcomp) tcomp = dkey[s.players[i].pos.y][s.players[i].pos.x];
    }
    if (mode == KK_GUARD) {
        Position c = s.players[s.key_owner].pos;
        int ax = -1, ay = -1, ad = KK_INF, tx = c.x, ty = c.y, found = 0;
        kk_bfs(&s, c.x, c.y, 0, dcar);
        for (d = 0; d < 4; ++d) {
            int nx = s.exit.x + KDX[d], ny = s.exit.y + KDY[d];
            if (!kk_ok(&s, nx, ny) || s.terrain[ny][nx] == 'E') continue;
            if (dcar[ny][nx] < ad) { ad = dcar[ny][nx]; ax = nx; ay = ny; }
        }
        if (s.turn < KK_HUNT && ax >= 0 && ad < KK_INF) {
            /* 접근 칸 a와 출구를 한 번의 부채꼴로 함께 덮는 4곳 중 소유자가 a에 닿기 전에 도착할 수 있는 가장 가까운 자리 */
            int ux = s.exit.x - ax, uy = s.exit.y - ay, q, pcx[4], pcy[4], bd = KK_INF;
            pcx[0] = ax + uy; pcy[0] = ay + ux; pcx[1] = ax - uy; pcy[1] = ay - ux;
            pcx[2] = s.exit.x + uy; pcy[2] = s.exit.y + ux; pcx[3] = s.exit.x - uy; pcy[3] = s.exit.y - ux;
            for (q = 0; q < 4; ++q) {
                if (!kk_ok(&s, pcx[q], pcy[q]) || s.terrain[pcy[q]][pcx[q]] == 'E' || kk_occ(&s, me, pcx[q], pcy[q])) continue;
                if (dme[pcy[q]][pcx[q]] > ad - 1 || dme[pcy[q]][pcx[q]] >= bd) continue;
                bd = dme[pcy[q]][pcx[q]]; tx = pcx[q]; ty = pcy[q]; found = 1;
            }
            if (!found) {
                /* 소유자가 출구 거리가 줄어드는 쪽으로 간다고 보고 따라가며, 내가 먼저 닿는 8방향 칸을 요격 자리로 고른다 */
                Position cc = c;
                int step, tc = dex[c.y][c.x], bestA = KK_INF;
                for (step = 1; !found && step <= tc && step < KK_M * KK_M; ++step) {
                    int moved = 0;
                    for (d = 0; d < 4 && !moved; ++d) {
                        int nx = cc.x + KDX[d], ny = cc.y + KDY[d];
                        if (!kk_ok(&s, nx, ny) || dex[ny][nx] != dex[cc.y][cc.x] - 1) continue;
                        cc.x = nx; cc.y = ny; moved = 1;
                    }
                    if (!moved) break;
                    for (u = 0; u < 9; ++u) {
                        int bx = cc.x + u % 3 - 1, by = cc.y + u / 3 - 1;
                        if (u == 4 || !kk_ok(&s, bx, by) || s.terrain[by][bx] == 'E') continue;
                        if (dme[by][bx] > step - 1 || dme[by][bx] >= bestA) continue;
                        bestA = dme[by][bx]; tx = bx; ty = by; found = 1;
                    }
                }
            }
        }
        goal[ty][tx] = 1;
    }

    /* 4) 후보 12개를 점수로 비교 */
    for (k = 0; k < 12; ++k) {
        int kind = k / 4, dir = k % 4, mx = p.x, my = p.y, score = 0, moved = 0, pos, hp;
        if (kind == 0) {
            mx += KDX[dir]; my += KDY[dir]; moved = 1;
            if (!kk_ok(&s, mx, my) || (s.terrain[my][mx] == 'E' && !carrier) || kk_occ(&s, me, mx, my)) continue;
        } else if (kind == 2) {
            if (s.key_owner != -1 || p.x + KDX[dir] != s.key.x || p.y + KDY[dir] != s.key.y) continue;
        }
        /* 위치 가치 */
        if (mode == KK_RUN || mode == KK_FETCH || mode == KK_GUARD) {
            if (delay && kind == 2) { /* 지금 줍고 출구까지 */
                int c = kk_route2(&s, R, p.x, p.y, 1, 1, K, tcomp, KK_DCOMP);
                pos = c >= KK_INF ? -100000 : -KK_PW * c;
            } else if (!moved && stayInit) pos = stayPos;
            else {
                int c = delay ? kk_route2(&s, R, mx, my, 1, 0, K, tcomp, KK_DCOMP) : kk_route(&s, R, mx, my, 1, goal, carrier, K);
                pos = c >= KK_INF ? -100000 : -KK_PW * c;
                if (!moved) { stayPos = pos; stayInit = 1; }
            }
            score += pos;
        } else if (mode == KK_DUEL) {
            int rx = s.players[rival].pos.x, ry = s.players[rival].pos.y, dc = kk_cheb(mx, my, rx, ry);
            pos = 0;
            if (dc == 2) pos += 120; else if (dc == 3) pos += 60; else if (dc >= 4) pos += 20 - dc; else pos -= 50;
            pos += 25 * kk_mob(&s, me, mx, my) - 4 * dkey[my][mx];
            pos -= KK_EW * (dex[my][mx] < 2 ? 6 : dex[my][mx]); /* 열쇠를 못 주울 때는 출구 쪽 요격 위치를 우선 */
            if (s.turn >= KK_FORCE) pos -= 30 * dc;
            score += pos;
        } else { /* LOITER */
            int nd = KK_INF, dk2 = dkey[my][mx];
            for (i = 0; i < PLAYER_COUNT; ++i)
                if (i != me && s.players[i].hp > 0) { int c = kk_cheb(s.players[i].pos.x, s.players[i].pos.y, mx, my); if (c < nd) nd = c; }
            pos = 30 * (nd > 4 ? 4 : nd) + 25 * kk_mob(&s, me, mx, my);
            pos += (dk2 >= 3 && dk2 <= 6) ? 60 : -20 * (dk2 < 3 ? 3 - dk2 : dk2 - 6);
            pos -= KK_EW * (dex[my][mx] < 2 ? 6 : dex[my][mx]);
            if (moved) pos += 8;
            pos += ((s.turn * 7 + me * 13 + mx * 3 + my * 5) & 7);
            score += pos;
        }
        /* 행동 가치 */
        if (kind == 1) score += kk_hit(&s, me, p.x, p.y, dir, dme, dex, -1);
        else if (kind == 2) {
            int adj = 0;
            for (i = 0; i < PLAYER_COUNT; ++i)
                if (i != me && s.players[i].hp > 0 && abs(s.players[i].pos.x - s.key.x) + abs(s.players[i].pos.y - s.key.y) <= 1) adj = 1;
            score += adj ? 250 : 2500; /* 열쇠 옆에 적이 있으면 동시에 집어 둘 다 취소될 수 있다 */
        }
        /* 맞을 확률 감점 */
        hp = kk_hurtpct(&s, me, mx, my, moved);
        if (mode == KK_GUARD) {
            /* 경비 중에는 소유자를 막는 게 우선: 소유자를 칠 수 있는 공격이면 감점 면제, 아니어도 감점을 줄인다 */
            if (kind == 1 && kk_hit(&s, me, p.x, p.y, dir, dme, dex, s.key_owner) > 0) hp = 0;
            else hp = hp * 100 / KK_GH;
        }
        score -= hp * (delay ? K * KK_PW : hurtUnit) / 100 * (carrier ? 2 : 1); /* 줍기 시점 비교 중에는 이번 턴 피격도 경로 위협과 같은 단위(K x KK_PW)로 센다 */
        if (score > bestScore) { bestScore = score; best = k; }
    }
    return (Command)best;
}

/* ===================== 진입점 ===================== */
Command player0_choose_command(const char *state_path)
{
    Snapshot s;
    FILE *file = fopen(state_path, "r");
    int ok;

    if (!file) return COMMAND_ATTACK_UP;
    ok = snapshot_read(file, &s);
    fclose(file);
    if (!ok) return COMMAND_ATTACK_UP;
    {   /* 1대1 결판 중에는 Kite 대신 ours_choose가 판단한다(열쇠를 내려놓은 뒤에도 같은 계획으로 이어가기 위해) */
        int i, alive = 0, lone = -1;
        for (i = 0; i < PLAYER_COUNT; ++i) if (i != s.self && s.players[i].hp > 0) { ++alive; lone = i; }
        if (P0_FINISH && alive == 1 && s.players[s.self].hp == MAX_HP && s.players[lone].hp == 1) return ours_choose(&s);
    }
    return (s.turn >= P0_KITE_TURN && s.turn < P0_KITE_END && s.key_owner < 0) ? kite_choose(&s) : ours_choose(&s);
}
