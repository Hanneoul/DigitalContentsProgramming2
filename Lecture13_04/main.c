/*
==============================================================================
 Lecture13_04 | LAST GATE : 이벤트로 움직이는 세 차선 방어전
==============================================================================
 승리: 세 웨이브를 버텨 기지를 지킨다. 패배: 기지 HP가 0이 된다.
 A/D 차선 이동, F 발사, B 긴급 폭탄, . 대기, H 도움말, Q 종료. 명령 뒤 Enter.
 화면은 위에서 아래로 세 차선이다. @ 포탑에서 > 탄환을 쏘아 E/H를 막는다.

 1. 앞 강의의 코드가 게임이 되는 과정
    13_01의 객체 콜백 → Emit으로 사건 전달.
    13_02의 여러 구독자 → ApplyRules(점수/피해), Record(전투 기록).
    13_03의 사건 값 복사 → 갱신 완료 뒤 Dispatch.
    Input → 총알 갱신 → 적 갱신 → Dispatch → 웨이브 판정 → Render.
    이벤트는 구조체 값으로 저장한다. 풀 객체 주소를 저장하면 재사용된 적을
    가리킬 수 있으므로 종류·차선·수치만 복사한다.

 2. 순서가 게임 규칙이다
    적의 생존/사망은 충돌 순간 확정하고 보상은 나중에 적용한다.
    그래서 두 탄환이 같은 적을 중복 처치하지 않는다.
    총알은 두 칸 전진하므로 출발~도착 구간을 검사한다. 이것이 간단한 연속 충돌이다.
    적 이동 후에도 탄환과 겹침을 검사하여 서로 통과하는 문제를 막는다.
    콜백은 풀을 생성/삭제하거나 새 이벤트를 발행하지 않는다. 갱신과 반응을 분리한다.

 3. Pool(공동 저장소)과 Queue(줄 서는 대기열)
    고정 배열의 inactive 칸을 재사용한다. new/malloc/free 없이 수명을 관리한다.
    큐가 가득 차면 조용히 점수를 잃지 않고 오류로 종료한다.
    매 턴 최대 사건 수는 총알/적 풀 크기로 제한되며 큐 용량보다 작다.
    Subscribe는 NULL·중복·용량 초과를 거절한다. 등록은 게임 시작 때만 한다.

 4. 표준 API와 실습 순서
    getchar: 한 줄의 명령 읽기, EOF 정상 종료. printf/puts: ASCII 화면.
    snprintf: 제한된 크기의 전투 기록 문자열 생성.
    먼저 화면과 이동 → 풀과 발사 → 충돌 → 사건 → 두 구독자 → 웨이브를 붙인다.
    도움말·잘못된 입력은 턴을 소비하지 않는다. 벽 방향 이동과 빈 폭탄 사용은 소비한다.
    실험: Record 구독을 빼도 게임 규칙은 유지되는가? 점수 콜백만 교체하면?
    확장: 느려지는 적, 처치 시 분열하는 적, 다음 턴 큐, 파일로 저장하는 전투 기록.
==============================================================================
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { LANES = 3, LENGTH = 16, POOL = 12, QUEUE = 64, LOGS = 5, LISTENERS = 3 };
typedef enum { FIRED, DAMAGED, DESTROYED, BREACHED, WAVE_CLEAR } EventType;
typedef struct { EventType type; int lane, amount; } Event;
typedef struct { int active, lane, x, hp; } Object;
typedef struct Game Game;
typedef void (*Listener)(Game *, const Event *);
struct Game
{
    Object enemies[POOL], bullets[POOL];
    Event queue[QUEUE];
    Listener listeners[LISTENERS];
    int eventCount, listenerCount;
    int turn, lane, hp, score, bombs, wave, spawned, spawnClock, won, failed;
    unsigned int random;
    char log[LOGS][72];
};

// 표준 입력은 줄 단위다. 여러 명령을 한 줄에 붙이면 오류로 처리한다.
static int ReadCommand(void)
{
    int key = getchar(), ch = key, valid = 1;
    while (ch != '\n' && ch != EOF)
    {
        ch = getchar();
        if (ch != '\n' && ch != '\r' && ch != EOF && ch != ' ' && ch != '\t') valid = 0;
    }
    if (key >= 'A' && key <= 'Z') key += 'a' - 'A';
    return valid ? key : '?';
}

// 시작 시에만 등록한다. 중복 구독으로 점수가 두 번 더해지는 실수를 차단한다.
static int Subscribe(Game *game, Listener listener)
{
    if (listener == NULL || game->listenerCount >= LISTENERS) return 0;
    for (int i = 0; i < game->listenerCount; ++i)
        if (game->listeners[i] == listener) return 0;
    game->listeners[game->listenerCount++] = listener;
    return 1;
}

// 사건 당시의 값을 복사한다. 큐 포화는 게임 규칙 오류이므로 명시적으로 실패시킨다.
static void Emit(Game *game, EventType type, int lane, int amount)
{
    if (game->eventCount >= QUEUE) { game->failed = 1; return; }
    game->queue[game->eventCount++] = (Event){type, lane, amount};
}

// 점수·기지 피해·웨이브 보상 담당 구독자. 객체 풀은 수정하지 않는다.
static void ApplyRules(Game *game, const Event *event)
{
    if (event->type == DESTROYED) game->score += event->amount;
    if (event->type == BREACHED)
    {
        game->hp -= event->amount;
        if (game->hp < 0) game->hp = 0;
    }
    if (event->type == WAVE_CLEAR)
    {
        game->score += 100;
        if (game->bombs < 3) ++game->bombs;
    }
}

// 최근 사건만 남긴다. snprintf는 목적지 버퍼의 크기까지 받아 과도한 출력을 제한한다.
static void Record(Game *game, const Event *event)
{
    static const char *names[] = {"SHOT", "HIT", "ENEMY DOWN", "BASE HIT", "WAVE CLEARED"};
    for (int i = LOGS - 1; i > 0; --i) memcpy(game->log[i], game->log[i - 1], sizeof game->log[i]);
    snprintf(game->log[0], sizeof game->log[0], "%s | lane %d | value %d",
        names[event->type], event->lane + 1, event->amount);
}

// 현재 큐를 등록 순서대로 처리한다. 콜백 내부에서 Emit/Subscribe를 호출하지 않는 계약이다.
static void Dispatch(Game *game)
{
    for (int e = 0; e < game->eventCount; ++e)
        for (int i = 0; i < game->listenerCount; ++i)
            game->listeners[i](game, &game->queue[e]);
    game->eventCount = 0;
}

// 생성 성공 시 슬롯 주소, 풀 포화 시 NULL. 활성 객체를 덮어쓰지 않는다.
static Object *Spawn(Object pool[], int lane, int x, int hp)
{
    for (int i = 0; i < POOL; ++i)
        if (!pool[i].active)
        {
            pool[i] = (Object){1, lane, x, hp};
            return &pool[i];
        }
    return NULL;
}

static int ActiveCount(const Object pool[])
{
    int count = 0;
    for (int i = 0; i < POOL; ++i) count += pool[i].active != 0;
    return count;
}

// 충돌 시 즉시 비활성화하여 동일한 적의 중복 처치 사건을 막는다.
static void Hit(Game *game, Object *enemy, int damage)
{
    if (!enemy->active) return;
    enemy->hp -= damage;
    Emit(game, DAMAGED, enemy->lane, damage);
    if (enemy->hp <= 0)
    {
        enemy->active = 0;
        Emit(game, DESTROYED, enemy->lane, 20);
    }
}

// 고정된 시작값을 쓰므로 같은 명령으로 같은 판을 재현하며 디버깅할 수 있다.
static void Start(Game *game)
{
    Game fresh = {0};
    fresh.hp = 5; fresh.lane = 1; fresh.bombs = 2; fresh.wave = 1; fresh.random = 2026;
    *game = fresh;
    if (!Subscribe(game, ApplyRules) || !Subscribe(game, Record)) game->failed = 1;
}

static void Help(void)
{
    puts("  A/D: change lane | F: fire | B: bomb ALL lanes | .: wait");
    puts("  H: help | Q: quit | Every command needs Enter.");
    puts("  @ turret  > bullet  E enemy  H armored enemy. Defend all three lanes!");
    puts("  Bombs hit every enemy. Clearing a wave restores one bomb (up to three).");
    puts("  Survive three waves. Only help and invalid commands do not advance time.");
}

static void Render(const Game *game)
{
    printf("\n+=============== LAST GATE / WAVE %d ===============+\n", game->wave);
    printf("  BASE %d/5 | SCORE %d | BOMBS %d | TURN %d | INCOMING %d\n",
        game->hp, game->score, game->bombs, game->turn, 4 + game->wave * 2 - game->spawned);
    for (int lane = 0; lane < LANES; ++lane)
    {
        char row[LENGTH + 1];
        memset(row, '.', LENGTH); row[LENGTH] = '\0';
        row[0] = lane == game->lane ? '@' : '|';
        for (int i = 0; i < POOL; ++i)
        {
            if (game->bullets[i].active && game->bullets[i].lane == lane) row[game->bullets[i].x] = '>';
            if (game->enemies[i].active && game->enemies[i].lane == lane)
                row[game->enemies[i].x] = game->enemies[i].hp > 1 ? 'H' : 'E';
        }
        printf("  %d |", lane + 1);
        for (int x = 0; x < LENGTH; ++x) printf(" %c", row[x]);
        puts(" | PORTAL");
    }
    puts("  -------------------------------------------------");
    for (int i = LOGS - 1; i >= 0; --i) if (game->log[i][0] != '\0') printf("  %s\n", game->log[i]);
}

static int Act(Game *game, int key)
{
    switch (key)
    {
    case 'a': if (game->lane > 0) --game->lane; break;
    case 'd': if (game->lane < LANES - 1) ++game->lane; break;
    case 'f':
        if (Spawn(game->bullets, game->lane, 1, 1) != NULL) Emit(game, FIRED, game->lane, 1);
        else puts("  All bullet slots are busy.");
        break;
    case 'b':
        if (game->bombs == 0) { puts("  No bombs left!"); break; }
        --game->bombs;
        for (int i = 0; i < POOL; ++i) Hit(game, &game->enemies[i], 2);
        break;
    case '.': break;
    case 'h': Help(); return 0;
    default: puts("  Unknown command. H shows help."); return 0;
    }
    return 1;
}

// 탄환의 두 칸 이동 구간에서 가장 가까운 적 하나만 맞힌다.
static void UpdateBullets(Game *game)
{
    for (int i = 0; i < POOL; ++i)
    {
        Object *bullet = &game->bullets[i];
        if (!bullet->active) continue;
        int end = bullet->x + 2;
        Object *nearest = NULL;
        for (int j = 0; j < POOL; ++j)
        {
            Object *enemy = &game->enemies[j];
            if (enemy->active && enemy->lane == bullet->lane && enemy->x >= bullet->x && enemy->x <= end)
                if (nearest == NULL || enemy->x < nearest->x) nearest = enemy;
        }
        if (nearest != NULL) { Hit(game, nearest, 1); bullet->active = 0; }
        else if (end >= LENGTH) bullet->active = 0;
        else bullet->x = end;
    }
}

// 적은 두 턴에 한 칸 전진한다. 적 이동 직후 겹침도 검사한 뒤 기지 도달을 판정한다.
static void UpdateEnemies(Game *game)
{
    if (game->turn % 2 != 0) return;
    for (int i = 0; i < POOL; ++i)
    {
        Object *enemy = &game->enemies[i];
        if (!enemy->active) continue;
        --enemy->x;
        for (int j = 0; j < POOL && enemy->active; ++j)
        {
            Object *bullet = &game->bullets[j];
            if (bullet->active && bullet->lane == enemy->lane && bullet->x == enemy->x)
            { Hit(game, enemy, 1); bullet->active = 0; }
        }
        if (enemy->active && enemy->x <= 0)
        {
            enemy->active = 0;
            Emit(game, BREACHED, enemy->lane, 1);
        }
    }
}

// 스폰 예약을 성공했을 때만 소비한다. 풀 포화면 다음 턴에 다시 시도한다.
static void SpawnEnemy(Game *game)
{
    if (game->spawned >= 4 + game->wave * 2 || --game->spawnClock > 0) return;
    unsigned int next = game->random * 1664525u + 1013904223u;
    int lane = (int)((next >> 16) % LANES);
    int hp = game->wave >= 2 && game->spawned % 3 == 0 ? 2 : 1;
    if (Spawn(game->enemies, lane, LENGTH - 1, hp) == NULL) return;
    game->random = next;
    ++game->spawned;
    game->spawnClock = 4;
}

// 모든 피해 사건을 적용한 뒤 승패를 판단한다. 마지막 적이 기지를 파괴하면 패배 우선이다.
static void Tick(Game *game)
{
    ++game->turn;
    UpdateBullets(game);
    UpdateEnemies(game);
    SpawnEnemy(game);
    Dispatch(game);
    if (game->hp == 0 || game->failed) return;
    if (game->spawned == 4 + game->wave * 2 && ActiveCount(game->enemies) == 0)
    {
        Emit(game, WAVE_CLEAR, game->lane, game->wave);
        Dispatch(game);
        if (game->wave == 3) game->won = 1;
        else
        {
            ++game->wave; game->spawned = 0; game->spawnClock = 3;
            memset(game->bullets, 0, sizeof game->bullets);
        }
    }
}

int main(void)
{
    Game game;
    Start(&game); Help();
    for (;;)
    {
        Render(&game);
        if (game.failed) { fputs("Event system capacity error.\n", stderr); return EXIT_FAILURE; }
        if (game.won || game.hp == 0)
        {
            puts(game.won ? "  VICTORY! THE LAST GATE STANDS!" : "  BASE DESTROYED. GAME OVER.");
            puts("  R + Enter: retry | anything else: quit");
            if (ReadCommand() != 'r') break;
            Start(&game); continue;
        }
        fputs("  Command > ", stdout); fflush(stdout);
        int key = ReadCommand();
        if (key == EOF || key == 'q') break;
        if (Act(&game, key)) Tick(&game);
    }
    return ferror(stdin) || ferror(stdout) ? EXIT_FAILURE : EXIT_SUCCESS;
}
