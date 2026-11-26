#include "player_api.h"

/* 화면에 표시되는 이름 */
char player4_name[PLAYER_NAME_BYTES] = "P4";

/* ===== P4 v4 : 명령 16개를 위험/이득 점수로 평가하는 AI =====
   핵심
   1) 열쇠 경쟁은 "내 도착 턴 vs 상대 도착 턴(부활 대기 포함)"으로 정한다.
   2) 남이 열쇠를 들면: 소유자의 출구 최단 경로를 계산해 내가 먼저 도착할 수 있는 칸을 고른다.
      - 그 칸에 서서 막기(차단): 우회 거리가 늘어나는 길목일수록 가치가 높다. 움직이지 않는 사람 칸으로
        이동하면 이동이 취소되고 이동한 쪽만 피해 1이라는 규칙을 이용한다.
      - 또는 경로 칸 옆에 서서 그 칸을 부채꼴로 덮는다(공격은 이동 후 위치를 친다).
   3) 내가 열쇠를 들면: 막은 적을 벽으로 보고 우회로를 찾고, 공격하지 않는다.
      HP 2로 출구 바로 앞이면 위험을 무시하고 들어간다(그 턴 피해는 최대 1).
   4) 위협은 현실적으로: 적 공격은 8칸 중 3칸만 맞히므로 확률로 계산하고, 같은 칸으로 들어가는 충돌도 확률로 계산한다.
   매 턴 별도 프로세스이므로 이전 턴 상태는 저장하지 않는다. */

#ifndef P4_FN
#define P4_FN player4_choose_command
#endif
#ifndef P4_GO_BIAS
#define P4_GO_BIAS 1       /* 열쇠 경쟁에서 이 값만큼 불리해도 간다 */
#endif
#ifndef P4_IDLE_DIV
#define P4_IDLE_DIV 3      /* 열쇠가 이 턴 방치될 때마다 허용 폭 +1 */
#endif
#ifndef P4_PATIENCE
#define P4_PATIENCE 40     /* 이 턴마다 허용 폭 +1 */
#endif
#ifndef P4_STAGE_R
#define P4_STAGE_R 2       /* 열쇠 대기 거리 */
#endif
#ifndef P4_ATK_P
#define P4_ATK_P 50        /* 적이 사정권의 나를 공격할 확률(%) */
#endif
#ifndef P4_W
#define P4_W 3             /* 거리 1칸당 점수 */
#endif
#ifndef P4_HIT_COST2
#define P4_HIT_COST2 14    /* HP 2일 때 피격 비용 */
#endif
#ifndef P4_HIT_COST1
#define P4_HIT_COST1 60    /* HP 1일 때 피격 비용 */
#endif
#ifndef P4_ATK_VAL
#define P4_ATK_VAL 14      /* 공격 성공 기본 가치 */
#endif
#ifndef P4_CARRIER_MUL
#define P4_CARRIER_MUL 3.5 /* 열쇠 소유자 공격 가치 배율 */
#endif
#ifndef P4_BUMP_VAL
#define P4_BUMP_VAL 45     /* 소유자가 내 칸으로 들어오다 취소될 때의 가치 */
#endif
#ifndef P4_BLOCK_W
#define P4_BLOCK_W 6       /* 우회 거리 1칸당 차단 가치 */
#endif
#ifndef P4_AMBUSH_V
#define P4_AMBUSH_V 24     /* 경로 칸 옆 매복 기본 가치 */
#endif
#ifndef P4_CHASE_TURN
#define P4_CHASE_TURN 150  /* 이 턴 이후 소유자가 안 나가면 직접 쫓는다 */
#endif

#ifndef P4_URG_START
#define P4_URG_START 120   /* 이 턴부터 위험을 덜 따진다 */
#endif
#ifndef P4_URG_SPAN
#define P4_URG_SPAN 300.0  /* 이 턴 동안 선형으로 줄인다 */
#endif
#ifndef P4_URG_PROG
#define P4_URG_PROG 60.0   /* 열쇠를 든 채 이 턴이 지날 때마다 전진 가중치 +1 */
#endif
#ifndef P4_URG_WMAX
#define P4_URG_WMAX 8.0
#endif
#ifndef P4_URG_MIN
#define P4_URG_MIN 0.15    /* 위험 비용의 최소 배율 */
#endif

#ifndef P4_PATH_RISK
#define P4_PATH_RISK 2.0   /* 출구 경로 비용: 상대 공격 범위 칸(피격 확률 1.0 기준)에 더하는 거리 */
#endif

#ifndef P4_CROWD
#define P4_CROWD 3         /* 열쇠를 든 채 출구 3칸 이내 상대가 이 수 이상이면 입구 앞에서 대기 */
#endif
#ifndef P4_CROWD1
#define P4_CROWD1 P4_CROWD /* 내 체력이 1일 때 입구 앞에서 기다리는 기준 상대 수(체력 2는 P4_CROWD 그대로) */
#endif
#ifndef P4_PICK_Q
#define P4_PICK_Q 70       /* 열쇠 옆의 상대가 같은 턴에 줍기를 시도할 확률(%): 동시에 줍으면 둘 다 취소 + 1피해 */
#endif
#ifndef P4_PICK_V
#define P4_PICK_V 500      /* 열쇠를 줍는 가치 */
#endif
#ifndef P4_PICKER_ATK
#define P4_PICKER_ATK 0    /* 열쇠를 주우려 가만히 서 있는 상대를 공격하는 추가 가치(0이면 쓰지 않는다) */
#endif
#ifndef P4_CLAIM
#define P4_CLAIM 1         /* 1이면 열쇠를 안 가는 자리는 열쇠 옆이 아니라 출구 근처의 덜 붐비는 칸을 미리 차지한다 */
#endif
#ifndef P4_CLAIM_MASK
#define P4_CLAIM_MASK 40   /* 자리(0 TL,1 TM,2 TR,3 BL,4 BM,5 BR) 비트마스크: 적용할 자리. 40 = BL·BR만 */
#endif
#ifndef P4_CLAIM_R
#define P4_CLAIM_R 3       /* 출구까지의 걸음 수가 이 값 이하인 칸만 후보 */
#endif
#ifndef P4_CLAIM_CW
#define P4_CLAIM_CW 4      /* 주변(체비쇼프 2 이내) 상대 한 명당 감점(가까울수록 크게) */
#endif
#ifndef P4_CLAIM_ADJ
#define P4_CLAIM_ADJ 6     /* 출구 바로 옆 칸 가산점(열쇠가 떨어지면 바로 줍고 들어갈 수 있다) */
#endif
#ifndef P4_CLAIM_DW
#define P4_CLAIM_DW 0.8    /* 내 이동 거리 1칸당 감점 */
#endif
#ifndef P4_WAIT
#define P4_WAIT 1          /* 1이면 출구 근처를 지키는 상대가 많을 때 열쇠를 줍지 않고 지켜본다 */
#endif
#ifndef P4_WAIT_MASK
#define P4_WAIT_MASK 23    /* 적용할 자리 비트마스크(0 TL,1 TM,2 TR,3 BL,4 BM,5 BR). 23 = BL·BR 제외 */
#endif
#ifndef P4_WAIT_W1
#define P4_WAIT_W1 1.0     /* HP 1인 지킴이를 세는 비중(1.0이면 HP와 상관없이 1명, 낮을수록 약해진 지킴이를 무시) */
#endif
#ifndef P4_WAIT_NTAB
#define P4_WAIT_NTAB { 2, 2, 2, 2, 2, 2 }   /* 자리별 지킴이 기준(0 TL,1 TM,2 TR,3 BL,4 BM,5 BR) */
#endif
#ifndef P4_WAIT_R
#define P4_WAIT_R 3        /* 출구까지의 걸음 수가 이 값 이하인 상대를 지킴이로 센다 */
#endif
#ifndef P4_WAIT_UNTIL
#define P4_WAIT_UNTIL 150  /* 이 턴 이후에는 기다리지 않는다(아무도 안 줍는 교착 방지) */
#endif
#ifndef P4_DROP
#define P4_DROP 1          /* 1이면 열쇠를 든 채 한 명이 따라붙을 때 열쇠를 일부러 떨어뜨려 줍는 순간을 노린다 */
#endif
#ifndef P4_DROP_MASK
#define P4_DROP_MASK 56    /* 적용할 자리 비트마스크(0 TL,1 TM,2 TR,3 BL,4 BM,5 BR). 56 = BL·BM·BR (3위 내 비율이 오른 자리) */
#endif
#ifndef P4_DROP_V
#define P4_DROP_V 40.0     /* 떨어뜨리기 가치 */
#endif
#ifndef P4_DROP_MIND
#define P4_DROP_MIND 4     /* 출구까지의 걸음 수가 이 값 이상일 때만(출구 코앞이면 그냥 달린다) */
#endif
#ifndef P4_DROP_ANYHP
#define P4_DROP_ANYHP 0    /* 1이면 상대 HP가 2여도 떨어뜨린다(기본은 HP 1일 때만: 줍는 턴의 확정 타격으로 처치) */
#endif
#ifndef P4_DROP_ATK
#define P4_DROP_ATK 150.0  /* 떨어뜨린 뒤 줍는 상대를 때리는 가치 */
#endif
#ifndef P4_DROP_CD
#define P4_DROP_CD 24      /* 기억 없이 반복을 막는 방법: 매 이 턴 중 앞 P4_DROP_WIN 턴에만 떨어뜨릴 수 있다 */
#endif
#ifndef P4_DROP_WIN
#define P4_DROP_WIN 4
#endif
#ifndef P4_ASTAR
#define P4_ASTAR 0         /* 1이면 열쇠를 든 채 이동할 때 시간까지 따지는 A* 로 "맞지 않고 출구에 닿는" 경로 비용을 계산해 이동을 고른다 */
#endif
#ifndef P4_AS_T
#define P4_AS_T 20         /* A* 가 내다보는 최대 걸음 수 */
#endif
#ifndef P4_AS_REACH
#define P4_AS_REACH 0.25   /* 지금은 떨어져 있지만 t걸음 안에 붙을 수 있는 상대의 공격 위험 배율 */
#endif
#ifndef P4_AS_HIT
#define P4_AS_HIT 1.0      /* 피격 1회(확률 1)를 걸음 수로 환산하는 배율(1.0 = 이동 점수식의 피격 비용과 같은 값) */
#endif
#ifndef P4_HOLD_R
#define P4_HOLD_R 5        /* 대기 거리(출구까지의 걸음 수) */
#endif
#ifndef P4_HOLD_UNTIL
#define P4_HOLD_UNTIL 160  /* 이 턴 이후에는 기다리지 않는다(교착 방지) */
#endif

#ifndef P4_CARRY_HITMUL
#define P4_CARRY_HITMUL 3.0 /* 열쇠를 든 동안 피격 비용 배율(1.5→3.0: 좌상단 자리 다른 팀 코드 상대 평균 순위 3.52→3.35) */
#endif

/* ---------- 자리별 오프닝 층 ----------
   공통 코어(탈출/교전/교착 방지)는 모든 자리가 같고, 시작 자리(spawn)에 따라 초반 행동 몇 가지만 다르게 한다.
   자리 번호 = 위/아래(0/1) * 3 + 왼/가운데/오른쪽(0/1/2) : 0 좌상, 1 상중, 2 우상, 3 좌하, 4 하중, 5 우하.
   각 칸: { 열쇠 경쟁 허용폭 보정(+면 서둘러 간다, -면 기다린다), 대기 거리, 대기 기준(0 열쇠 / 1 출구), 위험 비용 배율(%) } */
#ifndef P4_SEAT_TABLE
#define P4_SEAT_TABLE { {0, 2, 0, 100}, {0, 2, 0, 100}, {0, 2, 0, 100}, {0, 2, 0, 100}, {0, 2, 0, 100}, {0, 2, 0, 100} }
#endif

#define P4_INF 9999
#define P4_N MAP_MAX

static const int p4dx[4] = {0, 0, -1, 1};
static const int p4dy[4] = {-1, 1, 0, 0};

static int p4_in(const Snapshot *s, int x, int y)
{
    return x >= 0 && y >= 0 && x < s->size && y < s->size;
}

static int p4_can(const Snapshot *s, int x, int y, int allow_exit)
{
    if (!p4_in(s, x, y)) return 0;
    if (s->terrain[y][x] == '#') return 0;
    if (s->terrain[y][x] == 'E' && !allow_exit) return 0;
    return 1;
}

/* BFS. blk 가 NULL 이 아니면 blk 가 1 인 칸은 벽 취급, (bx,by) 도 벽 취급(음수면 없음) */
static void p4_bfs(const Snapshot *s, Position src, int allow_exit, unsigned char blk[P4_N][P4_N],
                   int bx, int by, int dist[P4_N][P4_N])
{
    Position q[P4_N * P4_N];
    int head = 0, tail = 0, x, y, d;
    for (y = 0; y < P4_N; ++y)
        for (x = 0; x < P4_N; ++x) dist[y][x] = P4_INF;
    if (!p4_in(s, src.x, src.y)) return;
    dist[src.y][src.x] = 0;
    q[tail++] = src;
    while (head < tail) {
        Position c = q[head++];
        for (d = 0; d < 4; ++d) {
            int nx = c.x + p4dx[d], ny = c.y + p4dy[d];
            if (!p4_can(s, nx, ny, allow_exit) || dist[ny][nx] != P4_INF) continue;
            if (nx == bx && ny == by) continue;
            if (blk && blk[ny][nx]) continue;
            dist[ny][nx] = dist[c.y][c.x] + 1;
            q[tail].x = nx; q[tail].y = ny; ++tail;
        }
    }
}

static int p4_cheb(int ax, int ay, int bx, int by)
{
    int dx = abs(ax - bx), dy = abs(ay - by);
    return dx > dy ? dx : dy;
}

/* 공격 방향 d 의 부채꼴(정면+양쪽 대각선)에 (px,py)가 들어가는가. 기준점 (ox,oy) */
static int p4_in_fan(int ox, int oy, int d, int px, int py)
{
    int ax = px - ox, ay = py - oy;
    if (d == 0) return ay == -1 && abs(ax) <= 1;
    if (d == 1) return ay == 1 && abs(ax) <= 1;
    if (d == 2) return ax == -1 && abs(ay) <= 1;
    return ax == 1 && abs(ay) <= 1;
}

/* 출구에서 퍼져 나가는 가중 거리. 칸에 들어가는 비용 = 1 + P4_PATH_RISK * 피격확률. 적이 선 칸은 못 지나간다.
   피격 위험이 큰 입구 대신 지키는 적이 없는 입구로 돌아 들어가게 만든다. */
static void p4_wdist(const Snapshot *s, double hit[P4_N][P4_N], unsigned char occ[P4_N][P4_N], double out[P4_N][P4_N])
{
    unsigned char done[P4_N][P4_N];
    int x, y, d, i, n = s->size;
    for (y = 0; y < P4_N; ++y) for (x = 0; x < P4_N; ++x) { out[y][x] = 1e9; done[y][x] = 0; }
    out[s->exit.y][s->exit.x] = 0.0;
    for (i = 0; i < n * n; ++i) {
        int bx = -1, by = -1; double best = 1e9;
        for (y = 0; y < n; ++y) for (x = 0; x < n; ++x)
            if (!done[y][x] && out[y][x] < best) { best = out[y][x]; bx = x; by = y; }
        if (bx < 0) break;
        done[by][bx] = 1;
        for (d = 0; d < 4; ++d) {
            int nx = bx + p4dx[d], ny = by + p4dy[d];
            double c;
            if (!p4_can(s, nx, ny, 0) || done[ny][nx] || occ[ny][nx]) continue;
            c = best + 1.0 + P4_PATH_RISK * hit[by][bx];   /* ny,nx 에서 by,bx 로 들어가는 비용 */
            if (c < out[ny][nx]) out[ny][nx] = c;
        }
    }
}

/* ---------- 시간을 포함한 A* ----------
   상태 = (칸, 걸음 수). 걸음 수 t 에 그 칸으로 들어갈 때의 비용 = 1 + asHit * (그 칸이 그 시점에 공격받을 확률).
   공격받을 확률: 상대와 체비쇼프 거리 1 이내면 pa, 걸음 수 안에 붙을 수 있으면 pa * P4_AS_REACH.
   휴리스틱은 위험을 뺀 BFS 거리(dE)라서 허용 가능(과대평가하지 않음). 열쇠를 든 선수는 가만히 있을 수 없으므로 매 걸음 이동한다. */
typedef struct P4Node { float f, g; unsigned char x, y, t; } P4Node;

static double p4_as_risk(const int *ex, const int *ey, int ne, int x, int y, int t)
{
    double ok = 1.0, pa = P4_ATK_P / 100.0 * 0.6;
    int i;
    for (i = 0; i < ne; ++i) {
        int ch = p4_cheb(x, y, ex[i], ey[i]);
        if (ch <= 1) ok *= 1.0 - pa;
        else if (ch <= t) ok *= 1.0 - pa * P4_AS_REACH;
    }
    return 1.0 - ok;
}

static double p4_astar(const Snapshot *s, int sx, int sy, int t0, int dE[P4_N][P4_N],
                       unsigned char occ[P4_N][P4_N], const int *ex, const int *ey, int ne, double asHit, int exitFree)
{
    float gbest[P4_AS_T + 1][P4_N][P4_N];
    P4Node heap[6000];
    int n = 0, t, x, y;
    for (t = t0; t <= P4_AS_T; ++t)
        for (y = 0; y < s->size; ++y)
            for (x = 0; x < s->size; ++x) gbest[t][y][x] = 1e9f;
    gbest[t0][sy][sx] = 0.0f;
    heap[n].f = (float)dE[sy][sx]; heap[n].g = 0.0f; heap[n].x = (unsigned char)sx; heap[n].y = (unsigned char)sy; heap[n].t = (unsigned char)t0; ++n;
    while (n > 0) {
        P4Node cur = heap[0];
        int i = 0, d;
        heap[0] = heap[--n];
        for (;;) {   /* 힙 정리: 아래로 내리기 */
            int l = 2 * i + 1, r = l + 1, m = i;
            if (l < n && heap[l].f < heap[m].f) m = l;
            if (r < n && heap[r].f < heap[m].f) m = r;
            if (m == i) break;
            { P4Node tmp = heap[i]; heap[i] = heap[m]; heap[m] = tmp; }
            i = m;
        }
        if (cur.g > gbest[cur.t][cur.y][cur.x]) continue;
        if (s->terrain[cur.y][cur.x] == 'E') return cur.g;
        if (cur.t >= P4_AS_T) continue;
        for (d = 0; d < 4; ++d) {
            int nx = cur.x + p4dx[d], ny = cur.y + p4dy[d], nt = cur.t + 1;
            double c;
            float ng;
            if (!p4_can(s, nx, ny, 1) || dE[ny][nx] >= P4_INF) continue;
            if (occ[ny][nx]) continue;
            c = 1.0;
            if (!(s->terrain[ny][nx] == 'E' && exitFree)) c += asHit * p4_as_risk(ex, ey, ne, nx, ny, nt);
            ng = cur.g + (float)c;
            if (ng >= gbest[nt][ny][nx] || n >= 5999) continue;
            gbest[nt][ny][nx] = ng;
            i = n++;
            heap[i].f = ng + (float)dE[ny][nx]; heap[i].g = ng; heap[i].x = (unsigned char)nx; heap[i].y = (unsigned char)ny; heap[i].t = (unsigned char)nt;
            while (i > 0 && heap[(i - 1) / 2].f > heap[i].f) {   /* 힙 정리: 위로 올리기 */
                P4Node tmp = heap[i]; heap[i] = heap[(i - 1) / 2]; heap[(i - 1) / 2] = tmp;
                i = (i - 1) / 2;
            }
        }
    }
    return 1e9;
}

Command P4_FN(const char *state_path)
{
    static const int dummy = 0; /* (static 상태는 쓰지 않는다. 읽기 전용 상수) */
    Snapshot s;
    FILE *fp;
    int dK[P4_N][P4_N], dE[P4_N][P4_N], dEb[P4_N][P4_N], dMe[P4_N][P4_N], dT[P4_N][P4_N], dC[P4_N][P4_N];
    int dB[P4_N][P4_N];
    unsigned char occ[P4_N][P4_N];
    double pd[P4_N][P4_N];
    double hit[P4_N][P4_N];
    double dW[P4_N][P4_N];
    int useW = 0, hold = 0;
    double pe[PLAYER_COUNT][P4_N][P4_N];
    int alive[PLAYER_COUNT];
    Position me, carrierPos = {0, 0};
    int i, x, y, d, c, carrying, keyFloor, carrier, myhp, nalive = 0, finished = 0;
    int mode; /* 0 key go, 1 ambush near key, 2 intercept carrier, 3 carry */
    double hitCost, bestScore = -1e18, wmul = 1.0;
    Command best = COMMAND_ATTACK_UP;
    int lvl[P4_N * 2];
    static const int seatTab[6][4] = P4_SEAT_TABLE;
    int seat, seatGo, seatStg, seatStx, seatHm;
    int claimOn = 0;
    int waitPick = 0;
    int dropOK[4] = {0, 0, 0, 0}, bait = 0;
    double asCost[4] = {0.0, 0.0, 0.0, 0.0};
    int asOn = 0;
    (void)dummy;

    fp = fopen(state_path, "r");
    if (!fp) return COMMAND_ATTACK_UP;
    i = snapshot_read(fp, &s);
    fclose(fp);
    if (!i) return COMMAND_ATTACK_UP;
    if (s.size < 3 || s.size > MAP_MAX || s.self < 0 || s.self >= PLAYER_COUNT) return COMMAND_ATTACK_UP;

    {   /* 시작 자리 번호 (맵 크기에 비례해 3x2로 나눈다) */
        Position sp = s.players[s.self].spawn;
        int cx = sp.x * 3 / s.size, cy = sp.y * 2 / s.size;
        seat = (cy > 1 ? 1 : cy) * 3 + (cx > 2 ? 2 : cx);
        seatGo = seatTab[seat][0]; seatStg = seatTab[seat][1]; seatStx = seatTab[seat][2]; seatHm = seatTab[seat][3];
    }
    me = s.players[s.self].pos;
    myhp = s.players[s.self].hp;
    carrier = s.key_owner;
    carrying = carrier == s.self;
    keyFloor = carrier < 0;
    hitCost = myhp >= 2 ? P4_HIT_COST2 : P4_HIT_COST1;
    if (carrying) hitCost *= P4_CARRY_HITMUL;
    hitCost = hitCost * seatHm / 100.0;
    /* 교착 방지: 매 턴 새 프로세스라 기억이 없으므로 게임 턴으로 인내심을 잰다. P4_URG_START 턴부터 위험 비용을 서서히 줄여서
       출구를 지키는 적을 계속 피하다 600턴을 넘기는 일(탈출 못 하면 순위 없음)을 막는다. */
    if (s.turn > P4_URG_START) {
        double u = 1.0 - (double)(s.turn - P4_URG_START) / P4_URG_SPAN;
        hitCost *= u < P4_URG_MIN ? P4_URG_MIN : u;
    }

    memset(occ, 0, sizeof(occ));
    for (i = 0; i < PLAYER_COUNT; ++i) {
        if (s.players[i].rank > 0) ++finished;
        alive[i] = (i != s.self && s.players[i].hp > 0 && s.players[i].rank == 0);
        if (alive[i]) {
            ++nalive;
            occ[s.players[i].pos.y][s.players[i].pos.x] = 1;
        }
    }
    if (carrier >= 0) carrierPos = s.players[carrier].pos;

    p4_bfs(&s, s.key, 0, NULL, -1, -1, dK);
    p4_bfs(&s, s.exit, 1, NULL, -1, -1, dE);
    p4_bfs(&s, s.exit, 1, occ, -1, -1, dEb);   /* 적을 벽으로 본 출구 거리 */
    p4_bfs(&s, me, carrying, NULL, -1, -1, dMe);
    if (carrier >= 0 && carrier != s.self) p4_bfs(&s, carrierPos, 1, NULL, -1, -1, dC);
    else p4_bfs(&s, me, 0, NULL, -1, -1, dC);

    /* ---------- 적 행동 예측 ---------- */
    memset(pd, 0, sizeof(pd));
    memset(hit, 0, sizeof(hit));
    memset(pe, 0, sizeof(pe));
    for (i = 0; i < PLAYER_COUNT; ++i) {
        Position e;
        int k, bestd = P4_INF, nbest = 0, nvalid = 0, here;
        int valid[4], dist[4], ecarry;
        double w[5];
        if (!alive[i]) continue;
        e = s.players[i].pos;
        ecarry = (carrier == i);
        for (d = 0; d < 4; ++d) {
            int nx = e.x + p4dx[d], ny = e.y + p4dy[d];
            valid[d] = p4_can(&s, nx, ny, ecarry);
            dist[d] = P4_INF;
            if (!valid[d]) continue;
            ++nvalid;
            if (ecarry) dist[d] = dE[ny][nx];
            else if (keyFloor) dist[d] = dK[ny][nx];
            else dist[d] = dC[ny][nx] <= 3 ? dC[ny][nx] : dE[ny][nx];
            if (dist[d] < bestd) bestd = dist[d];
        }
        for (d = 0; d < 4; ++d) if (valid[d] && dist[d] == bestd) ++nbest;
        if (ecarry) here = dE[e.y][e.x];
        else if (keyFloor) here = dK[e.y][e.x];
        else here = dC[e.y][e.x] <= 3 ? dC[e.y][e.x] : dE[e.y][e.x];
        for (k = 0; k < 5; ++k) w[k] = 0.0;
        if (nvalid == 0) w[4] = 1.0;
        else if (ecarry) {                       /* 소유자는 출구로 간다 */
            w[4] = 0.05;
            for (d = 0; d < 4; ++d) {
                if (!valid[d]) continue;
                if (dist[d] == bestd && bestd < here) w[d] = 0.85 / (nbest ? nbest : 1);
                else w[d] = 0.10 / nvalid;
            }
        } else if (keyFloor && here == 1) {      /* 열쇠 줍는 자리 */
            w[4] = 0.85;
            for (d = 0; d < 4; ++d) if (valid[d]) w[d] = 0.15 / nvalid;
        } else {
            w[4] = 0.28;
            for (d = 0; d < 4; ++d) {
                if (!valid[d]) continue;
                if (dist[d] == bestd && bestd < here) w[d] = 0.57 / (nbest ? nbest : 1);
                else w[d] = 0.15 / nvalid;
            }
        }
        for (k = 0; k < 5; ++k) {
            int nx = e.x, ny = e.y;
            if (w[k] <= 0.0) continue;
            if (k < 4) { nx += p4dx[k]; ny += p4dy[k]; }
            pd[ny][nx] += w[k];
            pe[i][ny][nx] += w[k];
        }
        if (!ecarry) {   /* 열쇠 소유자는 공격할 수 없다. 나머지는 8칸 중 일부를 확률로 친다 */
            double pa = P4_ATK_P / 100.0;
            for (y = e.y - 1; y <= e.y + 1; ++y)
                for (x = e.x - 1; x <= e.x + 1; ++x) {
                    if (!p4_in(&s, x, y) || (x == e.x && y == e.y) || s.terrain[y][x] == '#') continue;
                    hit[y][x] = 1.0 - (1.0 - hit[y][x]) * (1.0 - pa * 0.6);
                }
        }
    }

    /* ---------- 모드와 목표 ---------- */
    if (carrying) {
        mode = 3;
        memcpy(dT, dEb[me.y][me.x] < P4_INF ? dEb : dE, sizeof(dT));
        p4_wdist(&s, hit, occ, dW);
        useW = P4_PATH_RISK > 0.0 && dW[me.y][me.x] < 1e8;
        if (s.turn < P4_HOLD_UNTIL && dE[me.y][me.x] < P4_INF) {   /* 출구 근처에 상대가 몰려 있으면 입구 앞에서 기다린다 */
            int crowd = 0;
            for (i = 0; i < PLAYER_COUNT; ++i)
                if (alive[i] && abs(s.players[i].pos.x - s.exit.x) + abs(s.players[i].pos.y - s.exit.y) <= 3) ++crowd;
            hold = crowd >= (myhp <= 1 ? P4_CROWD1 : P4_CROWD);
        }
    } else if (keyFloor) {
        int myEta = dK[me.y][me.x] < 1 ? 2 : dK[me.y][me.x], minEta = P4_INF, bonus, idle;
        for (i = 0; i < PLAYER_COUNT; ++i) {
            int eta;
            if (i == s.self || s.players[i].rank != 0) continue;
            if (s.players[i].hp > 0) {
                eta = dK[s.players[i].pos.y][s.players[i].pos.x];
                if (eta < 1) eta = 2;
            } else {
                int rest = (int)(s.players[i].rest > 50 ? 50 : s.players[i].rest);
                eta = rest + 1 + dK[s.players[i].spawn.y][s.players[i].spawn.x];
            }
            if (eta < minEta) minEta = eta;
        }
        idle = KEY_RESPAWN_TURNS - s.key_respawn_remaining;
        if (idle < 0) idle = 0;
        bonus = P4_GO_BIAS + seatGo + idle / P4_IDLE_DIV + s.turn / P4_PATIENCE + finished;
        mode = (minEta - myEta + bonus >= 0 || nalive == 0) ? 0 : 1;
        if (P4_WAIT && ((P4_WAIT_MASK >> seat) & 1) && s.turn < P4_WAIT_UNTIL) {
            static const int waitN[6] = P4_WAIT_NTAB;
            double guards = 0.0;
            for (i = 0; i < PLAYER_COUNT; ++i)
                if (alive[i] && dE[s.players[i].pos.y][s.players[i].pos.x] <= P4_WAIT_R)
                    guards += s.players[i].hp >= 2 ? 1.0 : P4_WAIT_W1;
            if (guards >= waitN[seat] - 1e-9) { waitPick = 1; mode = 1; }
        }
        memcpy(dT, dK, sizeof(dT));
        if (P4_CLAIM && mode == 1 && ((P4_CLAIM_MASK >> seat) & 1) && dE[me.y][me.x] < P4_INF) {
            /* 출구 근처에서 가장 덜 붐비는 칸을 고른다. 상대가 서 있는 칸은 못 들어간다 */
            double bv = -1e18; int cx = -1, cy = -1, px, py;
            for (py = 0; py < s.size; ++py)
                for (px = 0; px < s.size; ++px) {
                    double v = 0.0; int e;
                    if (dE[py][px] < 1 || dE[py][px] > P4_CLAIM_R || s.terrain[py][px] == 'E' || occ[py][px] || dMe[py][px] >= P4_INF) continue;
                    for (e = 0; e < PLAYER_COUNT; ++e) {
                        int ch;
                        if (!alive[e]) continue;
                        ch = p4_cheb(px, py, s.players[e].pos.x, s.players[e].pos.y);
                        if (ch <= 2) v -= P4_CLAIM_CW * (3 - ch) / 3.0;
                    }
                    if (dE[py][px] == 1) v += P4_CLAIM_ADJ;
                    v -= P4_CLAIM_DW * dMe[py][px];
                    if (v > bv) { bv = v; cx = px; cy = py; }
                }
            if (cx >= 0) {
                Position t; t.x = cx; t.y = cy;
                p4_bfs(&s, t, 0, NULL, -1, -1, dT);
                claimOn = 1;
            }
        }
    } else {
        /* 남이 열쇠를 들었다: 소유자의 출구 최단 경로 위에서 먼저 자리 잡기 */
        int dEc = dE[carrierPos.y][carrierPos.x];
        double bestV = -1e18;
        int sx = -1, sy = -1, px, py;
        mode = 2;
        memset(lvl, 0, sizeof(lvl));
        for (py = 0; py < s.size; ++py)
            for (px = 0; px < s.size; ++px)
                if (dC[py][px] < P4_INF && dE[py][px] < P4_INF && dC[py][px] + dE[py][px] == dEc && s.terrain[py][px] != 'E'
                    && !(px == carrierPos.x && py == carrierPos.y) && dC[py][px] < P4_N * 2)
                    lvl[dC[py][px]]++;
        if (s.turn < P4_CHASE_TURN && dEc < P4_INF) {
            for (py = 0; py < s.size; ++py)
                for (px = 0; px < s.size; ++px) {
                    int tc, gain, nb;
                    if (dC[py][px] >= P4_INF || dE[py][px] >= P4_INF || dC[py][px] + dE[py][px] != dEc) continue;
                    if (s.terrain[py][px] == 'E' || (px == carrierPos.x && py == carrierPos.y)) continue;
                    tc = dC[py][px];
                    /* (a) 그 칸 위에 서서 막기: 막았을 때 소유자의 우회 거리 증가 */
                    if (dMe[py][px] <= tc - 1) {
                        p4_bfs(&s, carrierPos, 1, NULL, px, py, dB);
                        gain = dB[s.exit.y][s.exit.x] >= P4_INF ? 10 : dB[s.exit.y][s.exit.x] - dEc;
                        if (gain > 0) {
                            double v = P4_BLOCK_W * (gain > 8 ? 8 : gain) - 1.0 * dMe[py][px] - 0.7 * (tc - 1 - dMe[py][px]);
                            if (v > bestV) { bestV = v; sx = px; sy = py; }
                        }
                    }
                    /* (b) 경로 칸 옆에 서서 그 칸을 부채꼴로 덮기 */
                    for (nb = 0; nb < 8; ++nb) {
                        int ox = px + (nb < 4 ? p4dx[nb] : (nb == 4 || nb == 6 ? -1 : 1));
                        int oy = py + (nb < 4 ? p4dy[nb] : (nb == 4 || nb == 5 ? -1 : 1));
                        double v;
                        if (!p4_can(&s, ox, oy, 0)) continue;
                        if (dMe[oy][ox] > tc - 1) continue;
                        v = P4_AMBUSH_V / (double)(lvl[tc] > 0 ? lvl[tc] : 1) - 1.0 * dMe[oy][ox] - 0.7 * (tc - 1 - dMe[oy][ox]);
                        if (v > bestV) { bestV = v; sx = ox; sy = oy; }
                    }
                }
        }
        if (sx >= 0) {
            Position t; t.x = sx; t.y = sy;
            p4_bfs(&s, t, 0, NULL, -1, -1, dT);
        } else {
            memcpy(dT, dC, sizeof(dT));   /* 선점 불가: 직접 쫓는다 */
        }
    }

    /* 열쇠를 든 채 오래 못 나가면 전진 이득을 키운다(위험을 감수하고서라도 출구로) */
    if (mode == 3 && s.turn > P4_URG_START) {
        wmul = 1.0 + (double)(s.turn - P4_URG_START) / P4_URG_PROG;
        if (wmul > P4_URG_WMAX) wmul = P4_URG_WMAX;
    }

    if (P4_ASTAR && mode == 3 && !hold) {
        int ex[PLAYER_COUNT], ey[PLAYER_COUNT], ne = 0;
        double asHit = hitCost / P4_W * P4_AS_HIT;
        for (i = 0; i < PLAYER_COUNT; ++i)
            if (alive[i]) { ex[ne] = s.players[i].pos.x; ey[ne] = s.players[i].pos.y; ++ne; }
        for (d = 0; d < 4; ++d) {
            int nx = me.x + p4dx[d], ny = me.y + p4dy[d];
            if (!p4_can(&s, nx, ny, 1)) { asCost[d] = 1e9; continue; }
            if (s.terrain[ny][nx] == 'E') { asCost[d] = 0.0; continue; }
            asCost[d] = occ[ny][nx] ? 1e9 : p4_astar(&s, nx, ny, 1, dE, occ, ex, ey, ne, asHit, myhp >= 2);
        }
        asOn = 1;
    }

    if (P4_DROP && ((P4_DROP_MASK >> seat) & 1) && carrying && dE[me.y][me.x] >= P4_DROP_MIND && (s.turn % P4_DROP_CD) < P4_DROP_WIN) {
        int nt = 0, ti = -1, farOk = 1;
        for (i = 0; i < PLAYER_COUNT; ++i) {
            int ch;
            if (!alive[i]) continue;
            ch = p4_cheb(me.x, me.y, s.players[i].pos.x, s.players[i].pos.y);
            if (ch <= 2) { ++nt; ti = i; } else if (ch <= 5) farOk = 0;
        }
        if (nt == 1 && farOk && (s.players[ti].hp == 1 || P4_DROP_ANYHP)) {
            for (d = 0; d < 4; ++d) {
                int kx = me.x + p4dx[d], ky = me.y + p4dy[d];
                if (p4_can(&s, kx, ky, 0) && !occ[ky][kx] && p4_cheb(kx, ky, s.players[ti].pos.x, s.players[ti].pos.y) == 1) dropOK[d] = 1;
            }
        }
    }
    if (P4_DROP && ((P4_DROP_MASK >> seat) & 1) && keyFloor && !carrying) {       /* 방금 떨어뜨린 열쇠 옆에 HP 1 상대가 있고 내 부채꼴 안이면 줍는 순간을 친다 */
        int nb = 0, bi = -1;
        for (i = 0; i < PLAYER_COUNT; ++i)
            if (alive[i] && abs(s.players[i].pos.x - s.key.x) + abs(s.players[i].pos.y - s.key.y) == 1) { ++nb; bi = i; }
        if (nb == 1 && s.players[bi].hp == 1 && p4_cheb(me.x, me.y, s.players[bi].pos.x, s.players[bi].pos.y) == 1) bait = 1;
    }

    /* ---------- 16개 명령 평가 ---------- */
    for (c = 0; c < COMMAND_COUNT; ++c) {
        int nx = me.x, ny = me.y, valid = 1, moving = 0;
        double score = 0.0, gain = 0.0, risk = 0.0;
        if (c < 4) {
            nx += p4dx[c]; ny += p4dy[c];
            moving = 1;
            valid = p4_can(&s, nx, ny, carrying);
        } else if (c < 8) {
            valid = !carrying;
        } else if (c < 12) {
            int px = me.x + p4dx[c - 8], py = me.y + p4dy[c - 8];
            valid = keyFloor && !carrying && px == s.key.x && py == s.key.y;
        } else {
            valid = dropOK[c - 12];   /* 열쇠 버리기: 평소에는 쓰지 않고 P4_DROP 조건을 만족할 때만 */
        }
        if (!valid) continue;

        {
            double dd = dT[ny][nx];
            if (dd >= P4_INF) dd = 200;
            if (mode == 1 && claimOn) dd = dT[ny][nx] >= P4_INF ? 200 : dT[ny][nx];
            else if (mode == 1) {
                if (seatStx && dE[ny][nx] < P4_INF) dd = abs(dE[ny][nx] - seatStg);   /* 출구 쪽에서 기다린다 */
                else dd = abs((int)dd - seatStg);
            }
            else if (mode == 0) dd = abs((int)dd - 1);
            else if (mode == 3 && hold) dd = dE[ny][nx] >= P4_INF ? 200 : abs(dE[ny][nx] - P4_HOLD_R);
            else if (mode == 3 && asOn && c < 4) dd = asCost[c] < 1e8 ? asCost[c] : 200.0;
            else if (mode == 3 && useW) dd = s.terrain[ny][nx] == 'E' ? 0.0 : (dW[ny][nx] < 1e8 ? dW[ny][nx] : 200.0);
            score -= P4_W * wmul * dd;
        }

        if (mode == 3 && moving && s.terrain[ny][nx] == 'E') {
            score += 100000.0;                  /* 출구 진입: HP 2면 그 턴 피해는 최대 1이라 생존 */
            if (myhp >= 2) risk = 0.0; else risk += hitCost * hit[ny][nx];
        } else {
            risk += hitCost * hit[ny][nx];
            if (moving) risk += hitCost * (pd[ny][nx] > 1.0 ? 1.0 : pd[ny][nx]) * 0.9;
        }

        if (c >= 12) gain += P4_DROP_V;
        if (c >= 8 && c < 12) {                 /* 열쇠 줍기 */
            double conf = 0.0;
            for (i = 0; i < PLAYER_COUNT; ++i) {
                if (!alive[i]) continue;
                if (abs(s.players[i].pos.x - s.key.x) + abs(s.players[i].pos.y - s.key.y) == 1)
                    conf = 1.0 - (1.0 - conf) * ((100 - P4_PICK_Q) / 100.0);
            }
            gain += (waitPick ? -50.0 : (bait ? -400.0 : P4_PICK_V * (1.0 - conf))) - hitCost * conf;
        }

        if (c >= 4 && c < 8) {                  /* 공격 + (제자리이므로) 막기 */
            for (i = 0; i < PLAYER_COUNT; ++i) {
                double pin = 0.0, v;
                if (!alive[i]) continue;
                for (y = me.y - 1; y <= me.y + 1; ++y)
                    for (x = me.x - 1; x <= me.x + 1; ++x)
                        if (p4_in(&s, x, y) && p4_in_fan(me.x, me.y, c - 4, x, y)) pin += pe[i][y][x];
                v = P4_ATK_VAL;
                if (s.players[i].hp == 1) v *= 2.5;
                if (carrier == i) v *= P4_CARRIER_MUL;
                gain += pin * v;
            }
            if ((P4_PICKER_ATK > 0 || bait) && keyFloor) {      /* 열쇠를 주우려는 상대는 제자리라 부채꼴에 들어 있으면 확실히 맞는다 */
                for (i = 0; i < PLAYER_COUNT; ++i) {
                    int ex, ey;
                    if (!alive[i]) continue;
                    ex = s.players[i].pos.x; ey = s.players[i].pos.y;
                    if (abs(ex - s.key.x) + abs(ey - s.key.y) == 1 && p4_in_fan(me.x, me.y, c - 4, ex, ey))
                        gain += (P4_PICK_Q / 100.0) * (bait ? P4_DROP_ATK : P4_PICKER_ATK);
                }
            }
            if (carrier >= 0 && carrier != s.self && alive[carrier])
                gain += pe[carrier][me.y][me.x] * P4_BUMP_VAL;   /* 내 칸으로 들어오려다 취소되는 소유자 */
        }
        score += gain - risk;
        if (score > bestScore) { bestScore = score; best = (Command)c; }
    }
    return best;
}
