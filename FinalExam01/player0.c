/* 제출본 v4 (팀 "3인분"): 이전 제출본(미니맥스 깊이 4, A* 위험 가중) + 반복 심화(깊이 4는 항상 끝까지, 더 깊은 깊이는 한 턴 380ms 안에서만) + 노트북 재튜닝 값. 열쇠 판단 실험 플래그(P0_KJ*)는 모두 꺼진 기본값(P0_KJPERT 0). 이전본: player0_before_v4.c */
#define P0_TIME_MS 380
#define P0_CONTEST 823
#define P0_MMSLACK 574
#define P0_LATE 77
#define P0_DW 2
#define P0_RISK1 706
#define P0_RISK2 433
#define P0_NEAR1 244
#define P0_NEAR2 173
#define P0_KEYMUL 4
#define P0_CLEAN 240
#define P0_HUNT 458
#define P0_CLEANMOVE 9
#include "player_api.h"
#include <windows.h>

/* 제출 파일 player0 (팀 "3인분")
   매 턴 별도 프로세스로 불리므로 static 상태는 쓰지 않는다. 진입점(맨 아래)이 상태 파일을 한 번 읽어 ours_choose에 넘긴다.

   ours_choose 한 턴의 흐름
   1) 위험 지도: 공격은 공격자의 턴 시작 위치 기준 8방향 인접 칸만 닿으므로, 살아 있는 적의 인접 칸 = 이번 턴에 맞을 수 있는 칸.
   2) 목표 정하기(출구 경비 점수표)
      - 열쇠를 들었으면 출구.
      - 열쇠가 바닥에 있으면, 열쇠 경쟁에서 이길 때만 열쇠로 가고 지면 경비 자리(출구 접근 칸을 공격 부채꼴로 가장 많이 덮는 칸).
        턴이 길어질수록(P0_LATE 턴마다 1칸) 경쟁 여유를 늘려 무승부를 줄인다.
      - 남이 열쇠를 들었으면 쫓지 않고, 접근 칸과 출구를 함께 덮는 칸 / 경비 자리 / 소유자 예측 경로 위의 요격 칸 중 먼저 닿는 곳에서 기다린다.
      - 청소 모드: 열쇠가 오래 바닥에 있거나(P0_CLEAN) 소유자가 오래 못 나가면(P0_HUNT) 가까운 적이나 소유자를 직접 친다.
   3) 1대1 결판: 남은 적이 HP 1 하나뿐이고 내가 HP 2이면, 열쇠가 바닥일 때는 그 적부터 쓰러뜨리고,
      열쇠를 든 채 출구 접근 칸이 모두 막혔으면 열쇠를 적에게서 먼 옆 칸에 내려놓는다(열쇠를 든 동안은 공격할 수 없으므로).
   4) 경로: 열쇠를 들었으면 먼저 생존 보장 경로(mm_safepath)를 찾아 있으면 그대로 간다.
      아니면 A*(맨해튼 휴리스틱)로 지금 칸과 이웃 4칸에서 목표까지의 위험 가중 비용을 구한다.
      칸 비용 = 1 + 위험, 위험은 "k턴째 그 칸에 들어갈 때 그 칸을 칠 수 있는 적 수"를 k가 클수록 줄여 센다.
      BFS는 모든 칸까지 거리표가 필요한 곳(열쇠 경쟁, 경비 자리, 요격 예측)에만 쓴다.
   5) 이동 4·공격 4·집기 4를 점수표로 매긴다(목표 쪽 이동, 적을 칠 확률, 열쇠 줍기, 피격 위험 감점).
   6) 깊이 4 미니맥스(알파-베타): 가까운 적 2명 + 열쇠 소유자가 나에게 가장 나쁜 응수를 한다고 가정하고 4턴 뒤를 평가한다.
      판정(이동 충돌, 공격 부채꼴, 피해, 사망, 열쇠 떨어짐, 탈출)은 Rule.txt대로 직접 구현했다.
      최선 미니맥스 값보다 P0_MMSLACK 넘게 나쁜 수(맞아 죽거나 열쇠를 뺏기는 수)를 거른 뒤 남은 수 중 점수표 1등을 낸다.
      노드 수 상한(P0_MMNODES)을 넘으면 그 자리에서 평가해 시간이 일정하다. */

/* ---- 설정값 (괄호 안은 시뮬 근거) ---- */
#ifndef P0_CONTEST
#define P0_CONTEST 648 /* 열쇠 옆에 상대가 있을 때 줍기 점수. 0이면 교수님 --self-test(경쟁 줍기 검사)를 통과하지 못한다 */
#endif
#ifndef P0_LATE
#define P0_LATE 58     /* 이 턴 수마다 열쇠 경쟁 여유를 1칸씩 늘린다 */
#endif
#ifndef P0_DW
#define P0_DW 2        /* A* 첫 걸음: 지금 맞을 수 있는 칸 1개당 추가 비용 */
#endif
/* 위험 회피 점수: 맞을 수 있는 칸으로 가면 감점 / 이동할 칸 옆에 다른 사람이 있으면 감점. 1 = 내 HP 1, 2 = HP 2 */
#ifndef P0_RISK1
#define P0_RISK1 684
#endif
#ifndef P0_RISK2
#define P0_RISK2 487   /* 1000부터는 너무 소극적이라 진다 */
#endif
#ifndef P0_NEAR1
#define P0_NEAR1 237
#endif
#ifndef P0_NEAR2
#define P0_NEAR2 191
#endif
/* 청소 모드: 교착을 끝내려고 적을 먼저 친다. 살아 있는 적이 P0_CLEANMAX명 이하가 되면 평소처럼 열쇠를 줍는다.
   추격 중에도 위험 지도는 유지하고 쫓는 대상의 영향만 빼며, 내 HP가 1이면 추격을 멈춘다
   (원본 풀 250판 사망 1.54 -> 1.42, 복제본 섞인 풀 평균 순위 3.83 -> 3.32). */
#ifndef P0_CLEAN
#define P0_CLEAN 220   /* 이 턴부터 열쇠가 바닥에 있으면 청소 모드 */
#endif
#ifndef P0_HUNT
#define P0_HUNT 523    /* 남이 열쇠를 든 채 이 턴이 넘으면 소유자를 직접 쫓는다 */
#endif
#ifndef P0_CLEANKEY
#define P0_CLEANKEY 3  /* 청소 중 열쇠 주변 이 거리 안에 적이 있으면 줍지 않는다 */
#endif
#ifndef P0_CLEANMAX
#define P0_CLEANMAX 3
#endif
#ifndef P0_CLEANMOVE
#define P0_CLEANMOVE 8 /* 청소 중 적에게 다가가는 이동 점수 배수 */
#endif
#ifndef P0_KEYMUL
#define P0_KEYMUL 4    /* 열쇠 소유자를 치는 가치 배율 */
#endif
#ifndef P0_MV
#define P0_MV 40       /* 목표에 한 칸 가까워질 때 이동 점수 */
#endif
#ifndef P0_AV2
#define P0_AV2 80      /* HP 2인 적을 칠 때 기본 가치 */
#endif
#ifndef P0_AV1
#define P0_AV1 250     /* HP 1인 적을 칠 때 기본 가치 */
#endif
#ifndef P0_PICK
#define P0_PICK 2000   /* 경쟁 없는 열쇠 줍기 점수 */
#endif
#ifndef P0_RSTEP
#define P0_RSTEP 50    /* 위험도가 1 늘 때마다 추가 감점 */
#endif
#ifndef P0_CARRY
#define P0_CARRY 2     /* 열쇠를 든 동안 위험 감점 배수 */
#endif
#ifndef P0_MMDEPTH
#define P0_MMDEPTH 4   /* 미니맥스 깊이(턴). 깊이 비교에서 4가 최선(원본 풀 1위 30.6%, P5 포함 300판으로 확인) */
#endif
#ifndef P0_MMK
#define P0_MMK 2       /* 미니맥스에서 응수를 따지는 가까운 적 수(열쇠 소유자는 따로 항상 포함) */
#endif
#ifndef P0_MMSLACK
#define P0_MMSLACK 400 /* 최선 미니맥스 값보다 이만큼까지 나쁜 수는 허용(그중 점수표로 고름) */
#endif
#ifndef P0_MMNODES
#define P0_MMNODES 300000
#endif
#ifndef P0_RW
#define P0_RW 2        /* A*: k턴 뒤 그 칸을 칠 수 있는 적 1명당 비용(턴이 지날수록 1/k) */
#endif
#ifndef P0_RH
#define P0_RH 4        /* A*: 위험을 내다보는 턴 수 */
#endif
#ifndef P0_SAFEHOR
#define P0_SAFEHOR 3   /* 생존 경로: 적이 이 턴 안에 닿을 수 있는 칸만 위험으로 센다 */
#endif

#ifndef P0_TIME_MS
#define P0_TIME_MS 250 /* 반복 심화 시간 예산(ms, 진입점에서 잰다). 깊이 P0_MMDEPTH 는 항상 끝까지(노드 상한 P0_MMNODES), 더 깊은 깊이는 시간만으로 끊는다 */
#endif
#ifndef P0_IDMAX
#define P0_IDMAX 8     /* 반복 심화 최대 깊이 */
#endif
#ifndef P0_IDGROW
#define P0_IDGROW 3    /* 다음 깊이는 직전 깊이 걸린 시간의 이 배수가 남아 있을 때만 시작 */
#endif

#ifndef P0_KJPERT
#define P0_KJPERT 0    /* 0이 아니면: 남이 열쇠를 든 채 이 턴이 넘으면 점수표 1등이 아니라 턴 번호로 정한 2~3등 행동(미니맥스 거르기를 통과한 것 중)을 섞어 내어 같은 주기의 맴돌기를 깬다 */
#endif
#ifndef P0_KJPERTMOD
#define P0_KJPERTMOD 3 /* 몇 턴에 한 번 1등을 그대로 낼지(나머지 턴은 2·3등) */
#endif

#ifndef P0_KJPERTW
#define P0_KJPERTW 1   /* 맴돌기 깨기를 쓸 상황: 1 = 남이 열쇠를 듦, 2 = 열쇠가 바닥, 4 = 내가 열쇠를 듦(합쳐서 지정) */
#endif

#ifndef P0_KJOWN
#define P0_KJOWN 0     /* 0이 아니면: 이 턴 이후 미니맥스가 남의 열쇠 소유자를 "출구 쪽으로만 가는 사람"이 아니라 갈 수 있는 모든 칸으로 가는 사람으로 본다(도망 다니는 소유자에게 속지 않게) */
#endif

#ifndef P0_KJHUNT
#define P0_KJHUNT 0    /* 0이 아니면: 이 턴 이후 남이 열쇠를 들고 있을 때 "소유자는 우리 부채꼴 안(8방향 이웃)을 피해 도망간다"고 보고 길목 몰기(소유자가 도망칠 칸이 모두 사라지게 하는 이동)와 퇴로가 없을 때 치기에 점수를 준다 */
#endif
#ifndef P0_KJHW
#define P0_KJHW 100    /* 몰기 점수 배율(%) */
#endif

#define MM_INF 1000000

/* ---- 소유자 몰기(KJHUNT) 도우미 ---- */
/* (x,y)가 살아 있는 소유자 아닌 사람 중 누군가의 8방향 이웃(또는 그 자리)인가. 우리(me)는 (mx,my)에 있다고 본다 */
static int kj_inbox(const Snapshot *s, int owner, int x, int y, int mx, int my)
{
    int j;
    for (j = 0; j < PLAYER_COUNT; ++j) {
        int px, py;
        if (j == owner || s->players[j].hp <= 0 || s->players[j].rank) continue;
        px = j == s->self ? mx : s->players[j].pos.x; py = j == s->self ? my : s->players[j].pos.y;
        if (abs(px - x) <= 1 && abs(py - y) <= 1) return 1;
    }
    return 0;
}
/* 소유자가 (x,y)로 한 걸음 갈 수 있나(벽·출구·다른 사람 칸 제외). 우리는 (mx,my)에 있다고 본다 */
static int kj_legal(const Snapshot *s, int owner, int x, int y, int mx, int my)
{
    int j;
    if (x < 0 || y < 0 || x >= s->size || y >= s->size || s->terrain[y][x] != ' ') return 0;
    for (j = 0; j < PLAYER_COUNT; ++j) {
        int px, py;
        if (j == owner || s->players[j].hp <= 0 || s->players[j].rank) continue;
        px = j == s->self ? mx : s->players[j].pos.x; py = j == s->self ? my : s->players[j].pos.y;
        if (px == x && py == y) return 0;
    }
    return 1;
}
/* 우리가 (mx,my)에 선 한 걸음 뒤, 소유자가 도망칠 칸이 없어지는 비율과 퇴로 수로 점수를 낸다 */
static int kj_trap(const Snapshot *s, int owner, const int sxs[4], const int sys[4], int n0, int mx, int my)
{
    static const int dx[4] = {0, 0, -1, 1}, dy[4] = {-1, 1, 0, 0};
    int i, e, forced = 0, sum = 0;
    for (i = 0; i < n0; ++i) {
        int free_cells = 0;
        for (e = 0; e < 4; ++e) {
            int nx = sxs[i] + dx[e], ny = sys[i] + dy[e];
            if (kj_legal(s, owner, nx, ny, mx, my) && !kj_inbox(s, owner, nx, ny, mx, my)) ++free_cells;
        }
        if (!free_cells) ++forced;
        sum += free_cells;
    }
    return 1000 * forced / n0 + 60 * (4 * n0 - sum) / n0;
}
/* 12개 행동마다 몰기 점수 hb[k]. 소유자의 다음 칸은 "우리 부채꼴을 피하는 칸 중 하나(없으면 갈 수 있는 아무 칸)"로 본다 */
static void kj_hunt(const Snapshot *s, int me, int hb[12])
{
    static const int dx[4] = {0, 0, -1, 1}, dy[4] = {-1, 1, 0, 0};
    int owner = s->key_owner, ox = s->players[owner].pos.x, oy = s->players[owner].pos.y, px = s->players[me].pos.x, py = s->players[me].pos.y;
    int sxs[4], sys[4], n0 = 0, nl = 0, lx[4], ly[4], d, k, i;
    for (k = 0; k < 12; ++k) hb[k] = 0;
    for (d = 0; d < 4; ++d) {
        int nx = ox + dx[d], ny = oy + dy[d];
        if (!kj_legal(s, owner, nx, ny, px, py)) continue;
        lx[nl] = nx; ly[nl++] = ny;
        if (!kj_inbox(s, owner, nx, ny, px, py)) { sxs[n0] = nx; sys[n0++] = ny; }
    }
    if (!nl) return;
    if (!n0) { memcpy(sxs, lx, sizeof(lx)); memcpy(sys, ly, sizeof(ly)); n0 = nl; }
    for (d = 0; d < 4; ++d) { /* 공격: 소유자의 다음 칸이 부채꼴 안일 확률 */
        int hit = 0;
        for (i = 0; i < n0; ++i) {
            int fx = sxs[i] - (px + dx[d]), fy = sys[i] - (py + dy[d]);
            if (d < 2 ? (fy == 0 && fx >= -1 && fx <= 1) : (fx == 0 && fy >= -1 && fy <= 1)) ++hit;
        }
        hb[4 + d] = (s->players[owner].hp == 1 ? 1500 : 700) * hit / n0 + kj_trap(s, owner, sxs, sys, n0, px, py);
    }
    for (d = 0; d < 4; ++d) { /* 이동: 한 걸음 뒤 소유자가 도망칠 칸이 없어지는 비율 */
        int mx = px + dx[d], my = py + dy[d];
        if (mx < 0 || my < 0 || mx >= s->size || my >= s->size || s->terrain[my][mx] == '#') continue;
        hb[d] = kj_trap(s, owner, sxs, sys, n0, mx, my);
    }
}

/* 시간 측정: 진입점이 시작 시각을 적는다(한 턴 = 한 프로세스, 시뮬은 호출 사이에 덮어씀) */
static LONGLONG p0_t0, p0_freq;
static double p0_elapsed_ms(void)
{
    LARGE_INTEGER n;
    QueryPerformanceCounter(&n);
    return (double)(n.QuadPart - p0_t0) * 1000.0 / (double)p0_freq;
}

/* 적마다 "그 칸을 칠 수 있는 위치(8방향 이웃)에 가장 빨리 서는 턴": reach[e][y][x]. 이동만 센다(벽, 잠긴 출구는 못 지남) */
static void mm_reach(const Snapshot *s, int me, int reach[PLAYER_COUNT][MAP_MAX][MAP_MAX])
{
    static const int dx[4] = {0, 0, -1, 1}, dy[4] = {-1, 1, 0, 0};
    int e, x, y, d, h, t, qx[MAP_MAX * MAP_MAX], qy[MAP_MAX * MAP_MAX], dist[MAP_MAX][MAP_MAX];
    for (e = 0; e < PLAYER_COUNT; ++e) {
        for (y = 0; y < MAP_MAX; ++y) for (x = 0; x < MAP_MAX; ++x) reach[e][y][x] = MM_INF;
        if (e == me || s->players[e].hp <= 0 || s->players[e].rank) continue;
        if (s->key_owner == e) continue; /* 소유자는 공격할 수 없다 */
        for (y = 0; y < MAP_MAX; ++y) for (x = 0; x < MAP_MAX; ++x) dist[y][x] = MM_INF;
        h = t = 0; qx[t] = s->players[e].pos.x; qy[t++] = s->players[e].pos.y; dist[qy[0]][qx[0]] = 0;
        while (h < t) {
            x = qx[h]; y = qy[h++];
            for (d = 0; d < 4; ++d) {
                int nx = x + dx[d], ny = y + dy[d];
                if (nx < 0 || ny < 0 || nx >= s->size || ny >= s->size || s->terrain[ny][nx] != ' ' || dist[ny][nx] != MM_INF) continue;
                dist[ny][nx] = dist[y][x] + 1; qx[t] = nx; qy[t++] = ny;
            }
        }
        for (y = 0; y < s->size; ++y) for (x = 0; x < s->size; ++x) {
            int ax, ay, best = MM_INF;
            if (s->terrain[y][x] == '#') continue;
            for (ay = y - 1; ay <= y + 1; ++ay) for (ax = x - 1; ax <= x + 1; ++ax)
                if ((ax != x || ay != y) && ax >= 0 && ay >= 0 && ax < s->size && ay < s->size && dist[ay][ax] < best) best = dist[ay][ax];
            reach[e][y][x] = best;
        }
    }
}

/* A*: start(k0턴째 서 있음)에서 goal까지 위험 가중 비용. 칸에 k턴째 들어갈 때 비용 = 1 + DW*지금 위험(k==1) + RW*(k-1턴 안에 칠 수 있는 적 수)/k */
static int mm_astar(const Snapshot *s, int sx, int sy, int k0, int gx, int gy, int carrier, int danger[MAP_MAX][MAP_MAX],
                    int reach[PLAYER_COUNT][MAP_MAX][MAP_MAX])
{
    static const int dx[4] = {0, 0, -1, 1}, dy[4] = {-1, 1, 0, 0};
    int g[MAP_MAX][MAP_MAX], step[MAP_MAX][MAP_MAX];
    unsigned char closed[MAP_MAX][MAP_MAX], open[MAP_MAX][MAP_MAX];
    int x, y, d, e, n = s->size;
    if (sx == gx && sy == gy) return 0;
    for (y = 0; y < n; ++y) for (x = 0; x < n; ++x) { g[y][x] = MM_INF; closed[y][x] = open[y][x] = 0; }
    g[sy][sx] = 0; step[sy][sx] = k0; open[sy][sx] = 1;
    for (;;) {
        int bx = -1, by = -1, bf = MM_INF;
        for (y = 0; y < n; ++y) for (x = 0; x < n; ++x) /* 열린 칸 중 f = g + 맨해튼 이 가장 작은 칸 (칸 289개라 단순 탐색이 힙보다 짧다) */
            if (open[y][x]) { int f = g[y][x] + abs(x - gx) + abs(y - gy); if (f < bf) { bf = f; bx = x; by = y; } }
        if (bx < 0) return MM_INF;
        if (bx == gx && by == gy) return g[by][bx];
        open[by][bx] = 0; closed[by][bx] = 1;
        for (d = 0; d < 4; ++d) {
            int nx = bx + dx[d], ny = by + dy[d], k = step[by][bx] + 1, c;
            if (nx < 0 || ny < 0 || nx >= n || ny >= n || s->terrain[ny][nx] == '#' || closed[ny][nx]) continue;
            if (s->terrain[ny][nx] == 'E' && !carrier) continue;
            c = 1;
            if (k == 1) c += P0_DW * danger[ny][nx];
            else if (k <= P0_RH) {
                int cnt = 0;
                for (e = 0; e < PLAYER_COUNT; ++e) if (reach[e][ny][nx] <= k - 1) ++cnt;
                c += P0_RW * cnt * 2 / k; /* 2/k: 2턴째 1배, 4턴째 절반 */
            }
            if (g[by][bx] + c < g[ny][nx]) { g[ny][nx] = g[by][bx] + c; step[ny][nx] = k; open[ny][nx] = 1; }
        }
    }
}

/* ---- 미니맥스용 작은 판정기 ---- */
typedef struct {
    signed char x[PLAYER_COUNT], y[PLAYER_COUNT], hp[PLAYER_COUNT], act[PLAYER_COUNT];
    signed char owner, kx, ky, esc; /* esc: 이번 수에서 탈출한 사람(-1 없음) */
    signed char lost; /* 우리가 든 열쇠를 떨어뜨렸나 */
} MmState;

typedef struct {
    const Snapshot *s;
    int me, nodes, cap, timed, stop;
    int dgoal[MAP_MAX][MAP_MAX], dexit[MAP_MAX][MAP_MAX]; /* 루트 목표까지, 출구까지 BFS 거리(위험 없음) */
} MmCtx;

static int mm_free(const Snapshot *s, int x, int y) { return x >= 0 && y >= 0 && x < s->size && y < s->size && s->terrain[y][x] != '#'; }

/* Rule.txt 3~6절 순서: 유효성 -> 같은 목적지 취소 -> 멈춘 사람 칸 취소(연쇄) -> 이동/집기 -> 공격 -> 피해/사망 -> 탈출 */
static void mm_resolve(const MmCtx *c, MmState *st, const int cmd[PLAYER_COUNT])
{
    static const int dx[4] = {0, 0, -1, 1}, dy[4] = {-1, 1, 0, 0};
    const Snapshot *s = c->s;
    int tx[PLAYER_COUNT], ty[PLAYER_COUNT], kind[PLAYER_COUNT], ok[PLAYER_COUNT], dmg[PLAYER_COUNT] = {0}, sx[PLAYER_COUNT], sy[PLAYER_COUNT];
    int i, j, k, ch, owner0 = st->owner;
    st->esc = -1; st->lost = 0;
    for (i = 0; i < PLAYER_COUNT; ++i) {
        sx[i] = tx[i] = st->x[i]; sy[i] = ty[i] = st->y[i]; ok[i] = st->act[i]; kind[i] = -1;
        if (!st->act[i]) continue;
        if (cmd[i] < 0 || cmd[i] >= 12) { ok[i] = 0; continue; }
        kind[i] = cmd[i] / 4; tx[i] += dx[cmd[i] % 4]; ty[i] += dy[cmd[i] % 4];
        if (kind[i] == 1) { if (owner0 == i) ok[i] = 0; continue; }
        if (!mm_free(s, tx[i], ty[i])) ok[i] = 0;
        else if (kind[i] == 0 && s->terrain[ty[i]][tx[i]] == 'E' && owner0 != i) ok[i] = 0;
        else if (kind[i] == 2 && (owner0 != -1 || tx[i] != st->kx || ty[i] != st->ky)) ok[i] = 0;
    }
    { int cancel[PLAYER_COUNT] = {0};
      for (i = 0; i < PLAYER_COUNT; ++i) for (j = i + 1; j < PLAYER_COUNT; ++j)
          if (ok[i] && ok[j] && tx[i] == tx[j] && ty[i] == ty[j] && kind[i] == kind[j] && (kind[i] == 0 || kind[i] == 2)) cancel[i] = cancel[j] = 1;
      for (i = 0; i < PLAYER_COUNT; ++i) if (cancel[i]) ok[i] = 0; }
    do {
        ch = 0;
        for (i = 0; i < PLAYER_COUNT; ++i) {
            if (!ok[i] || kind[i] != 0) continue;
            for (j = 0; j < PLAYER_COUNT; ++j)
                if (i != j && st->act[j] && tx[i] == sx[j] && ty[i] == sy[j] && !(ok[j] && kind[j] == 0)) { ok[i] = 0; ch = 1; break; }
        }
    } while (ch);
    for (i = 0; i < PLAYER_COUNT; ++i) {
        if (!st->act[i]) continue;
        if (!ok[i]) { dmg[i] = 1; continue; }
        if (kind[i] == 0) { st->x[i] = (signed char)tx[i]; st->y[i] = (signed char)ty[i]; }
        else if (kind[i] == 2) st->owner = (signed char)i;
    }
    for (i = 0; i < PLAYER_COUNT; ++i) {
        if (!st->act[i] || !ok[i] || kind[i] != 1) continue;
        for (k = -1; k <= 1; ++k) {
            int hx = tx[i] + (cmd[i] % 4 < 2 ? k : 0), hy = ty[i] + (cmd[i] % 4 < 2 ? 0 : k);
            if (!mm_free(s, hx, hy)) continue;
            for (j = 0; j < PLAYER_COUNT; ++j) if (j != i && st->act[j] && st->x[j] == hx && st->y[j] == hy) dmg[j] = 1;
        }
    }
    for (i = 0; i < PLAYER_COUNT; ++i) {
        if (!st->act[i] || !dmg[i]) continue;
        if (--st->hp[i] > 0) continue;
        st->act[i] = 0;
        if (st->owner == i) {
            st->owner = -1;
            if (s->terrain[st->y[i]][st->x[i]] == 'E') { st->kx = (signed char)s->key_start.x; st->ky = (signed char)s->key_start.y; }
            else { st->kx = st->x[i]; st->ky = st->y[i]; }
            if (i == c->me) st->lost = 1;
        }
    }
    if (st->owner >= 0 && st->act[st->owner] && s->terrain[st->y[st->owner]][st->x[st->owner]] == 'E') {
        st->esc = st->owner; st->act[st->owner] = 0; st->owner = -1;
        st->kx = (signed char)s->key_start.x; st->ky = (signed char)s->key_start.y;
    }
}

/* 상태 점수(우리 기준). 루트와 비교한 변화만 본다 */
static int mm_eval(const MmCtx *c, const MmState *st, const MmState *root, int left)
{
    const Snapshot *s = c->s;
    int me = c->me, v = 0, i;
    if (st->esc == me) return 1000000 + left * 1000;
    if (st->esc >= 0) v -= 200000;
    if (!st->act[me]) return v - 6000;
    v -= 900 * (root->hp[me] - st->hp[me]);
    if (st->owner == me) v += 3000 - 50 * c->dexit[st->y[me]][st->x[me]];
    else v -= 40 * (c->dgoal[st->y[me]][st->x[me]] >= MM_INF ? 60 : c->dgoal[st->y[me]][st->x[me]]);
    for (i = 0; i < PLAYER_COUNT; ++i) {
        int lost;
        if (i == me || !root->act[i]) continue;
        lost = root->hp[i] - (st->act[i] ? st->hp[i] : 0);
        if (root->owner == i) v += lost * 1500 + (st->owner != i && st->esc != i ? 2500 : 0);
        else v += lost * 120;
        if (st->act[i] && abs(st->x[i] - st->x[me]) <= 1 && abs(st->y[i] - st->y[me]) <= 1 && st->owner != i)
            v -= st->hp[me] == 1 ? 500 : 200; /* 끝 자리에서 다음 턴 맞을 수 있음 */
    }
    return v;
}

/* 적 e의 응수 후보: 우리를 칠 수 있는 공격, 우리 쪽 한 걸음, 자기 목표(열쇠/출구) 쪽 한 걸음, 제자리. 소유자는 이동만 */
static int mm_enemy_opts(const MmCtx *c, const MmState *st, int e, int out[8])
{
    static const int dx[4] = {0, 0, -1, 1}, dy[4] = {-1, 1, 0, 0};
    const Snapshot *s = c->s;
    int n = 0, d, me = c->me, best = MM_INF, bd = -1, kd = MM_INF, kdd = -1;
    if (st->owner == e) {
        for (d = 0; d < 4; ++d) {
            int nx = st->x[e] + dx[d], ny = st->y[e] + dy[d];
            if (mm_free(s, nx, ny) && (c->dexit[ny][nx] < c->dexit[st->y[e]][st->x[e]] || (P0_KJOWN && s->turn >= P0_KJOWN && e != c->me))) out[n++] = d;
        }
        if (!n) out[n++] = 0;
        return n;
    }
    for (d = 0; d < 4; ++d) { /* 우리(이동 후 위치 후보 5곳 중 하나라도)에 닿는 공격 방향 */
        int fx = st->x[e] + dx[d], fy = st->y[e] + dy[d], m, hit = 0;
        for (m = 0; m < 5 && !hit; ++m) {
            int px = st->x[me] + (m ? dx[m - 1] : 0), py = st->y[me] + (m ? dy[m - 1] : 0);
            if (d < 2 ? (py == fy && abs(px - fx) <= 1) : (px == fx && abs(py - fy) <= 1)) hit = 1;
        }
        if (hit) out[n++] = 4 + d;
    }
    for (d = 0; d < 4; ++d) {
        int nx = st->x[e] + dx[d], ny = st->y[e] + dy[d], md, gd;
        if (!mm_free(s, nx, ny) || s->terrain[ny][nx] == 'E') continue;
        md = abs(nx - st->x[me]) + abs(ny - st->y[me]);
        if (md < best) { best = md; bd = d; }
        gd = st->owner < 0 ? abs(nx - st->kx) + abs(ny - st->ky) : abs(nx - s->exit.x) + abs(ny - s->exit.y);
        if (gd < kd) { kd = gd; kdd = d; }
    }
    if (bd >= 0) out[n++] = bd;
    if (kdd >= 0 && kdd != bd) out[n++] = kdd;
    if (st->owner < 0 && abs(st->x[e] - st->kx) + abs(st->y[e] - st->ky) == 1) /* 열쇠 바로 옆이면 줍기도 */
        for (d = 0; d < 4; ++d) if (st->x[e] + dx[d] == st->kx && st->y[e] + dy[d] == st->ky) out[n++] = 8 + d;
    out[n++] = -2; /* 제자리(빈 곳 공격): 아래에서 아무도 안 맞는 방향으로 바꾼다 */
    return n;
}

static int mm_my_opts(const MmCtx *c, const MmState *st, int out[12])
{
    static const int dx[4] = {0, 0, -1, 1}, dy[4] = {-1, 1, 0, 0};
    const Snapshot *s = c->s;
    int me = c->me, n = 0, d, i, nearby = 0;
    for (d = 0; d < 4; ++d) {
        int nx = st->x[me] + dx[d], ny = st->y[me] + dy[d];
        if (!mm_free(s, nx, ny) || (s->terrain[ny][nx] == 'E' && st->owner != me)) continue;
        out[n++] = d;
    }
    for (i = 0; i < PLAYER_COUNT; ++i) if (i != me && st->act[i] && abs(st->x[i] - st->x[me]) <= 2 && abs(st->y[i] - st->y[me]) <= 2) nearby = 1;
    if (st->owner != me) for (d = 0; d < 4; ++d) if (nearby || d == 0) out[n++] = 4 + d; /* 적이 없으면 제자리용 공격 하나만 */
    if (st->owner < 0) for (d = 0; d < 4; ++d) if (st->x[me] + dx[d] == st->kx && st->y[me] + dy[d] == st->ky) out[n++] = 8 + d;
    return n;
}

static int mm_pick_set(const MmCtx *c, const MmState *st, int set[3])
{
    int n = 0, i, k, bi, bd;
    int me = c->me;
    if (st->owner >= 0 && st->owner != me && st->act[st->owner]) set[n++] = st->owner;
    for (k = 0; k < P0_MMK; ++k) {
        bi = -1; bd = 4;
        for (i = 0; i < PLAYER_COUNT; ++i) {
            int dd, j, used = 0;
            if (i == me || !st->act[i]) continue;
            for (j = 0; j < n; ++j) if (set[j] == i) used = 1;
            if (used) continue;
            dd = abs(st->x[i] - st->x[me]); if (abs(st->y[i] - st->y[me]) > dd) dd = abs(st->y[i] - st->y[me]);
            if (dd < bd) { bd = dd; bi = i; }
        }
        if (bi >= 0) set[n++] = bi;
    }
    return n;
}

/* 우리 차례: 최대화. 적 응수: 최소화(알파베타) */
static int mm_search(MmCtx *c, const MmState *st, const MmState *root, int depth, int alpha, int beta, int first_cmd, int *vals)
{
    int my[12], nm = mm_my_opts(c, st, my), a, best = -MM_INF * 10;
    if (c->stop) return 0;
    if (first_cmd >= 0) { my[0] = first_cmd; nm = 1; }
    for (a = 0; a < nm; ++a) {
        int set[3], ns = mm_pick_set(c, st, set), opt[3][8], no[3], idx[3] = {0, 0, 0}, worst = MM_INF * 10, j, b = beta;
        for (j = 0; j < ns; ++j) no[j] = mm_enemy_opts(c, st, set[j], opt[j]);
        for (;;) { /* 적 응수 조합 전부 */
            int cmd[PLAYER_COUNT], i, v;
            MmState nx = *st;
            for (i = 0; i < PLAYER_COUNT; ++i) cmd[i] = 4; /* 나머지 적: 제자리 */
            cmd[c->me] = my[a];
            for (j = 0; j < ns; ++j) {
                int o = opt[j][idx[j]];
                if (o == -2) { /* 우리 쪽이 아닌 방향의 빈 공격 */
                    int dd, e = set[j];
                    o = 4;
                    for (dd = 0; dd < 4; ++dd) {
                        int fx = st->x[e] + (dd == 2 ? -1 : dd == 3 ? 1 : 0), fy = st->y[e] + (dd == 0 ? -1 : dd == 1 ? 1 : 0);
                        if (abs(fx - st->x[c->me]) > 2 || abs(fy - st->y[c->me]) > 2) { o = 4 + dd; break; }
                    }
                    if (st->owner == e) o = 0;
                }
                cmd[set[j]] = o;
            }
            for (i = 0; i < PLAYER_COUNT; ++i) if (i != c->me && st->owner == i && cmd[i] >= 4) cmd[i] = 0;
            mm_resolve(c, &nx, cmd);
            ++c->nodes;
            if (c->timed && (c->nodes & 255) == 0 && p0_elapsed_ms() > P0_TIME_MS) c->stop = 1;
            if (c->stop) return 0;
            if (depth <= 1 || nx.esc >= 0 || !nx.act[c->me] || c->nodes > c->cap) v = mm_eval(c, &nx, root, depth - 1);
            else v = mm_search(c, &nx, root, depth - 1, alpha > best ? alpha : best, b, -1, NULL);
            if (c->stop) return 0;
            if (v < worst) worst = v;
            if (worst < b) b = worst;
            if (worst <= alpha || worst <= best) break; /* 이 수는 이미 다른 수보다 나쁘다 */
            for (j = 0; j < ns; ++j) { if (++idx[j] < no[j]) break; idx[j] = 0; }
            if (j == ns) break;
        }
        if (vals) vals[a] = worst;
        if (worst > best) best = worst;
        if (best >= beta && !vals) break;
    }
    return best;
}

/* 루트: 행동 k(0~11)마다 미니맥스 값. 가능하지 않은 행동은 -MM_INF*10 */
static void mm_root(const Snapshot *s, int me, int tx, int ty, int out[12])
{
    static const int dx[4] = {0, 0, -1, 1}, dy[4] = {-1, 1, 0, 0};
    MmCtx c;
    MmState root;
    int i, k, x, y, d, h, t, qx[MAP_MAX * MAP_MAX], qy[MAP_MAX * MAP_MAX], pass;
    c.s = s; c.me = me; c.nodes = 0; c.cap = P0_MMNODES; c.timed = 0; c.stop = 0;
    for (pass = 0; pass < 2; ++pass) {
        int (*dist)[MAP_MAX] = pass ? c.dexit : c.dgoal, gx = pass ? s->exit.x : tx, gy = pass ? s->exit.y : ty;
        for (y = 0; y < MAP_MAX; ++y) for (x = 0; x < MAP_MAX; ++x) dist[y][x] = MM_INF;
        dist[gy][gx] = 0; h = t = 0; qx[t] = gx; qy[t++] = gy;
        while (h < t) {
            x = qx[h]; y = qy[h++];
            for (d = 0; d < 4; ++d) {
                int nx = x + dx[d], ny = y + dy[d];
                if (!mm_free(s, nx, ny) || dist[ny][nx] != MM_INF) continue;
                if (s->terrain[ny][nx] == 'E') continue;
                dist[ny][nx] = dist[y][x] + 1; qx[t] = nx; qy[t++] = ny;
            }
        }
    }
    for (i = 0; i < PLAYER_COUNT; ++i) {
        root.x[i] = (signed char)s->players[i].pos.x; root.y[i] = (signed char)s->players[i].pos.y;
        root.hp[i] = (signed char)s->players[i].hp; root.act[i] = s->players[i].hp > 0 && !s->players[i].rank;
    }
    root.owner = (signed char)s->key_owner; root.kx = (signed char)s->key.x; root.ky = (signed char)s->key.y; root.esc = -1; root.lost = 0;
    /* 반복 심화: 깊이 P0_MMDEPTH 는 항상 완료(기존 동작), 이후 깊이는 시간이 되는 한 완료된 것만 쓴다.
       루트 행동은 직전 깊이 값이 높은 순으로 탐색하고, 이미 vmax-SLACK 이하로 확정된 행동은 창을 좁혀 일찍 끊는다(어차피 거를 행동) */
    {
        int depth, ord[12], tmp[12], prev[12], last_ms = 0, n, j;
        for (k = 0; k < 12; ++k) { ord[k] = k; prev[k] = 0; }
        for (depth = P0_MMDEPTH; depth <= P0_IDMAX; ++depth) {
            int vmax = -MM_INF * 10;
            double t_start = p0_elapsed_ms();
            if (depth > P0_MMDEPTH && t_start + last_ms * (double)P0_IDGROW > P0_TIME_MS) break;
            c.nodes = 0; c.stop = 0; c.timed = depth > P0_MMDEPTH; c.cap = depth > P0_MMDEPTH ? 2000000000 : P0_MMNODES;
            for (n = 0; n < 12; ++n) for (j = n + 1; j < 12; ++j) if (prev[ord[j]] > prev[ord[n]]) { int t2 = ord[n]; ord[n] = ord[j]; ord[j] = t2; }
            for (n = 0; n < 12; ++n) {
                int v;
                k = ord[n];
                tmp[k] = -MM_INF * 10;
                if (k / 4 == 1 && s->key_owner == me) continue;
                if (k / 4 == 2 && (s->key_owner != -1 || s->players[me].pos.x + dx[k % 4] != s->key.x || s->players[me].pos.y + dy[k % 4] != s->key.y)) continue;
                if (k / 4 == 0 && (!mm_free(s, s->players[me].pos.x + dx[k], s->players[me].pos.y + dy[k]))) continue;
                mm_search(&c, &root, &root, depth, vmax > -MM_INF * 5 ? vmax - P0_MMSLACK - 1 : -MM_INF * 10, MM_INF * 10, k, &v);
                if (c.stop) break;
                tmp[k] = v;
                if (v > vmax) vmax = v;
            }
            if (c.stop) break;
            for (k = 0; k < 12; ++k) { out[k] = tmp[k]; prev[k] = tmp[k]; }
            last_ms = (int)(p0_elapsed_ms() - t_start) + 1;
        }
    }
}

/* 생존 보장 경로: 열쇠를 든 채 (칸, 턴, 맞은 횟수) 상태를 턴 단위 너비 우선으로 펼쳐, 출구에 살아서 들어가는 가장 이른 경로를 찾는다.
   k턴째 칸 c가 위험 = 어떤 적이 k-1턴(P0_SAFEHOR턴 이하) 안에 c의 8방향 이웃에 설 수 있음(최악 가정, 충돌도 이 안에 들어간다). 1턴째에 남이 서 있는 칸은 못 간다.
   맞은 횟수 합(출구로 들어가는 턴 포함)이 HP-1 이하면 그 경로의 첫 걸음을 낸다. 없으면 -1. 소유자는 멈출 수 없어 매 턴 이동만 한다. */
#define SP_T 40
static int mm_safepath(const Snapshot *s, int me, int reach[PLAYER_COUNT][MAP_MAX][MAP_MAX])
{
    static const int dx[4] = {0, 0, -1, 1}, dy[4] = {-1, 1, 0, 0};
    unsigned char seen[2][MAP_MAX][MAP_MAX][2]; /* [턴 짝홀][y][x][맞은 횟수] */
    signed char first[2][MAP_MAX][MAP_MAX][2];
    int t, x, y, h, d, e, cur = 0, budget = s->players[me].hp - 1, any;
    if (budget < 0) return -1;
    memset(seen, 0, sizeof(seen));
    seen[0][s->players[me].pos.y][s->players[me].pos.x][0] = 1;
    first[0][s->players[me].pos.y][s->players[me].pos.x][0] = -1;
    for (t = 1; t <= SP_T; ++t) {
        int nxt = cur ^ 1, goal_dir = -1;
        memset(seen[nxt], 0, sizeof(seen[nxt]));
        any = 0;
        for (y = 0; y < s->size; ++y) for (x = 0; x < s->size; ++x) for (h = 0; h <= budget; ++h) {
            if (!seen[cur][y][x][h]) continue;
            for (d = 0; d < 4; ++d) {
                int nx = x + dx[d], ny = y + dy[d], nh = h, j, occ = 0;
                if (nx < 0 || ny < 0 || nx >= s->size || ny >= s->size || s->terrain[ny][nx] == '#') continue;
                if (t == 1) for (j = 0; j < PLAYER_COUNT; ++j)
                    if (j != me && s->players[j].hp > 0 && !s->players[j].rank && s->players[j].pos.x == nx && s->players[j].pos.y == ny) occ = 1;
                if (occ) continue;
                for (e = 0; e < PLAYER_COUNT; ++e) if (reach[e][ny][nx] <= t - 1 && reach[e][ny][nx] <= P0_SAFEHOR) { ++nh; break; }
                if (nh > budget) continue;
                if (s->terrain[ny][nx] == 'E') { if (goal_dir < 0) goal_dir = t == 1 ? d : first[cur][y][x][h]; continue; }
                if (seen[nxt][ny][nx][nh]) continue;
                seen[nxt][ny][nx][nh] = 1; any = 1;
                first[nxt][ny][nx][nh] = (signed char)(t == 1 ? d : first[cur][y][x][h]);
            }
        }
        if (goal_dir >= 0) return goal_dir;
        if (!any) return -1;
        cur = nxt;
    }
    return -1;
}

static Command ours_choose(const Snapshot *sp)
{
    static const int dx[4] = {0, 0, -1, 1}, dy[4] = {-1, 1, 0, 0};
    Snapshot s = *sp;
    int dk[MAP_MAX][MAP_MAX], dme[MAP_MAX][MAP_MAX], dex[MAP_MAX][MAP_MAX], dt[MAP_MAX][MAP_MAX], danger[MAP_MAX][MAP_MAX];
    int qx[MAP_MAX * MAP_MAX], qy[MAP_MAX * MAP_MAX];
    int finish = 0, ctgt = -1, clean, alive = 0, pass, h, t, x, y, d, i, j, k, me, carrier, sx = 0, sy = 0, tx = 0, ty = 0;
    int best = COMMAND_ATTACK_UP, bestScore = -1000000000, sc[12], mmv[12];
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
    /* 청소 중에도 위험 지도는 유지하고 쫓는 대상의 영향만 뺀다 */
    if (clean) {
        if (s.players[me].hp == 1) clean = 0; /* HP 1이면 추격을 멈춘다 */
        else {
            if (s.key_owner >= 0) ctgt = s.key_owner;
            else {
                int bd0 = 1000000;
                for (i = 0; i < PLAYER_COUNT; ++i) {
                    int v;
                    if (i == me || s.players[i].hp <= 0) continue;
                    v = abs(s.players[i].pos.x - p.x) + abs(s.players[i].pos.y - p.y) - (s.players[i].hp == 1 ? 2 : 0);
                    if (v < bd0) { bd0 = v; ctgt = i; }
                }
            }
            if (ctgt >= 0) /* 쫓는 대상의 영향만 위험 지도에서 뺀다 */
                for (y = -1; y <= 1; ++y) for (x = -1; x <= 1; ++x) {
                    int cx = s.players[ctgt].pos.x + x, cy = s.players[ctgt].pos.y + y;
                    if ((x || y) && cx >= 0 && cy >= 0 && cx < s.size && cy < s.size && s.terrain[cy][cx] != '#' && danger[cy][cx] > 0) --danger[cy][cx];
                }
        }
    }

    /* 1대1 결판: 살아 있는 적이 HP 1 하나뿐이고 나는 HP 2일 때.
       - 열쇠를 든 채 출구 접근 칸이 모두 그 적의 공격 범위라 막혀 있으면, 열쇠를 든 동안은 공격이 안 되므로 열쇠를 옆 칸에 내려놓는다.
       - 열쇠가 바닥에 있으면 줍기 전에 그 적부터 쓰러뜨린다(청소 모드). 쓰러뜨리면 평소대로 줍고 출구로 간다. */
    if (alive == 1 && s.players[me].hp == MAX_HP) {
        int lone = -1;
        for (i = 0; i < PLAYER_COUNT; ++i) if (i != me && s.players[i].hp > 0) lone = i;
        if (lone >= 0 && s.players[lone].hp == 1) {
            if (!carrier && s.key_owner < 0) {
                clean = 1; finish = 1; ctgt = lone; /* 쓰러뜨리기 전에는 열쇠를 줍지 않는다 */
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
                if (myd > enemyMin + s.turn / P0_LATE && gx >= 0) { tx = gx; ty = gy; }
            } else if (!carrier) {
                /* 남이 열쇠를 들었다: 경비 자리에 제때 도착하면 경비, 아니면 예측 경로 위에서 요격 */
                Position c = s.players[s.key_owner].pos;
                int tc = dex[c.y][c.x], step, found = 0;
                {
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
        if (pass < 3) { /* pass 3(목표까지 거리)은 아래 A*가 필요한 다섯 칸만 채운다 */
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
    }

    if (carrier) { /* 열쇠를 들었고 최악 가정에서도 살아서 들어가는 경로가 있으면 그대로 간다 */
        int reach2[PLAYER_COUNT][MAP_MAX][MAP_MAX], sd;
        mm_reach(&s, me, reach2);
        sd = mm_safepath(&s, me, reach2);
        if (sd >= 0) return (Command)sd;
    }
    { /* A*: 지금 칸과 이웃 4칸에서 목표까지의 위험 가중 비용만 구한다(점수표가 쓰는 값은 이 다섯 개뿐) */
        int reach[PLAYER_COUNT][MAP_MAX][MAP_MAX];
        mm_reach(&s, me, reach);
        for (y = 0; y < MAP_MAX; ++y) for (x = 0; x < MAP_MAX; ++x) dt[y][x] = 1000000;
        dt[p.y][p.x] = mm_astar(&s, p.x, p.y, 0, sx, sy, carrier, danger, reach);
        for (d = 0; d < 4; ++d) {
            int nx = p.x + dx[d], ny = p.y + dy[d];
            if (nx < 0 || ny < 0 || nx >= s.size || ny >= s.size || s.terrain[ny][nx] == '#') continue;
            if (s.terrain[ny][nx] == 'E' && !carrier) continue;
            dt[ny][nx] = mm_astar(&s, nx, ny, 1, sx, sy, carrier, danger, reach);
        }
    }

    /* 4) 이동 4개, 공격 4개, 집기 4개를 점수로 비교 */
    for (k = 0; k < 12; ++k) sc[k] = -1000000000;
    for (k = 0; k < 12; ++k) {
        int kind = k / 4, dir = k % 4, nx = p.x, ny = p.y, score = 0, risk, nearby = 0, keyhit = 0;
        if (kind == 1 && carrier) continue; /* 교수님 패치: 열쇠를 든 동안은 공격이 안 된다. 이동만 후보로 둔다 */
        if (kind == 0) {
            nx += dx[dir]; ny += dy[dir];
            if (nx < 0 || ny < 0 || nx >= s.size || ny >= s.size || s.terrain[ny][nx] == '#') continue;
            if (s.terrain[ny][nx] == 'E' && !carrier) continue;
            for (j = 0; j < PLAYER_COUNT; ++j) {
                if (j == me || s.players[j].hp <= 0) continue;
                if (s.players[j].pos.x == nx && s.players[j].pos.y == ny) nearby = 100; /* 남이 서 있는 칸 */
                else if (abs(s.players[j].pos.x - nx) + abs(s.players[j].pos.y - ny) == 1) { if (!(clean && j == ctgt)) ++nearby; } /* 같은 칸 충돌 위험(청소 중 쫓는 대상은 제외) */
            }
            if (nearby >= 100) continue;
            /* 열쇠 들고 출구 진입 (아무도 출구로 못 들어오므로 충돌도 없다).
               HP 1이고 출구 칸이 공격 범위면 맞아 죽을 수 있지만(사망 판정이 승리보다 먼저), 위험 때 멈추면
               시뮬 승률이 더 낮았다(73.1% -> 72.0%). 그래서 HP와 상관없이 바로 들어간다. */
            if (s.terrain[ny][nx] == 'E') return (Command)k;
            score -= nearby * (s.players[me].hp == 1 ? P0_NEAR1 : P0_NEAR2);
            if (dt[ny][nx] < 1000000) score += (dt[p.y][p.x] - dt[ny][nx]) * (clean ? P0_MV * P0_CLEANMOVE : P0_MV);
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
            score += contest ? P0_CONTEST : P0_PICK; /* 경쟁 줍기도 시도: 실패해도 피해 1, 성공하면 열쇠 (HP 1에서 막으면 승률이 63%로 폭락해 그대로 둔다) */
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
                val = s.players[j].hp == 1 ? P0_AV1 : P0_AV2;
                if (j == s.key_owner) { val *= P0_KEYMUL; if (cov) keyhit = 1; }
                score += val * cov / wsum;
            }
        }
        /* 도착(또는 제자리) 칸의 피격 위험 */
        risk = danger[ny][nx];
        if (risk && !keyhit) score -= (s.players[me].hp == 1 ? P0_RISK1 : P0_RISK2) * (carrier ? P0_CARRY : 1) + P0_RSTEP * (risk - 1);
        sc[k] = score;
        if (score > bestScore) { bestScore = score; best = k; }
    }
    if (P0_KJHUNT && !carrier && s.key_owner >= 0 && s.key_owner != me && s.turn >= P0_KJHUNT) { /* 소유자 몰기 점수를 더한다 */
        int hb[12];
        kj_hunt(&s, me, hb);
        for (k = 0; k < 12; ++k) if (sc[k] > -1000000000) sc[k] += hb[k] * P0_KJHW / 100;
    }
    if (P0_MMDEPTH > 0) { /* 미니맥스 거르기: 최선 값보다 P0_MMSLACK 넘게 나쁜 행동을 빼고 점수표 1등을 고른다 */
        int vmax = -MM_INF * 10, b2 = -1, bs2 = -1000000000;
        mm_root(&s, me, sx, sy, mmv);
        for (k = 0; k < 12; ++k) if (sc[k] > -1000000000 && mmv[k] > vmax) vmax = mmv[k];
        for (k = 0; k < 12; ++k) if (sc[k] > -1000000000 && mmv[k] >= vmax - P0_MMSLACK && sc[k] > bs2) { bs2 = sc[k]; b2 = k; }
        if (b2 >= 0 && P0_KJPERT && s.turn >= P0_KJPERT && (P0_KJPERTW & (carrier ? 4 : s.key_owner >= 0 ? 1 : 2)) && !(clean && s.players[me].hp == 1)) {
            /* 맴돌기 깨기: 통과한 행동 중 점수표 순서로 n번째를 턴 번호로 골라 낸다 */
            int pick = (int)(((unsigned)s.turn * 2654435761u) >> 20) % P0_KJPERTMOD, order[12], cnt = 0, a, b;
            for (k = 0; k < 12; ++k) if (sc[k] > -1000000000 && mmv[k] >= vmax - P0_MMSLACK) order[cnt++] = k;
            for (a = 0; a < cnt; ++a) for (b = a + 1; b < cnt; ++b) if (sc[order[b]] > sc[order[a]]) { int tmp = order[a]; order[a] = order[b]; order[b] = tmp; }
            if (pick >= cnt) pick = cnt - 1;
            b2 = order[pick];
        }
        if (b2 >= 0) best = b2;
    }
    return (Command)best;
}

/* 화면에 보일 팀 이름(FinalExam01부터 필수, UTF-8 63바이트 이하) */
char player0_name[PLAYER_NAME_BYTES] = "3인분";

/* ===================== 진입점 ===================== */
Command player0_choose_command(const char *state_path)
{
    Snapshot s;
    FILE *file;
    int ok;
    LARGE_INTEGER q;
    QueryPerformanceFrequency(&q); p0_freq = q.QuadPart;
    QueryPerformanceCounter(&q); p0_t0 = q.QuadPart;
    file = fopen(state_path, "r");

    if (!file) return COMMAND_ATTACK_UP;
    ok = snapshot_read(file, &s);
    fclose(file);
    if (!ok) return COMMAND_ATTACK_UP;
    return ours_choose(&s);
}
