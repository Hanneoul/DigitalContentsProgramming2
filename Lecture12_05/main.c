/*
==============================================================================
 Lecture12_05 | CALLBACK DUNGEON : 함수 주소가 무기가 되는 던전
==============================================================================
 목표: 보물 두 개와 열쇠를 모으고 보스를 쓰러뜨린 뒤 E로 탈출한다.
 @ 용사, g 추격 고블린, G 수문장, B 보스, $ 보물, k 열쇠, + 회복, E 출구.

 1. 앞 예제에서 가져온 것
    12_03의 구조체 속 콜백 → Hero.weapon, Monster.think.
    12_04의 대상 전달 → Weapon(Game *, Monster *), Think(Game *, Monster *).
    같은 Attack은 장착한 무기가 검인지 화염인지 몰라도 weapon을 호출한다.
    몬스터도 동일한 갱신 루프에서 Chase 또는 Guard를 호출한다.
    Callback은 '되불러 주는 함수'이다. 이 예제는 모두 동기 호출이다.

 2. 한 턴의 지도
    한 줄 입력 → 이동/전투/장착 → 살아 있는 몬스터의 콜백 → 승패 → 화면
    성공한 행동과 벽을 향한 이동은 한 턴이다. 도움말과 잘못된 명령은 무료이다.
    공격·장착·회복에도 적이 행동한다. 장비 메뉴에서 시간이 멈추지는 않는다.
    플레이어가 적을 먼저 죽였다면 그 적은 이번 턴에 반격하지 못한다.

 3. 게임으로 배우는 설계
    Game은 공유 문맥(Context), 포인터는 그 원본에 접근하는 방법이다.
    Monster 배열은 고정된 적 목록이다. hp <= 0인 적은 그리거나 갱신하지 않는다.
    지도는 타일 배열, 캐릭터는 별도 구조체이다. Render에서만 둘을 겹쳐 그린다.
    이동할 칸을 검사한 뒤 위치를 바꾼다. 배열 경계 확인이 벽 검사보다 먼저다.
    보물 타일을 '.'으로 지우므로 같은 곳에서 무한 보상을 얻을 수 없다.

 4. 표준 API
    getchar/putchar: 한 문자 입출력. EOF를 구분하려면 getchar 결과는 int이다.
    printf: 형식을 정해 상태와 전투 기록을 출력한다.
    memcpy: 고정 지도 배열을 새 게임 상태로 복사한다.
    abs: 좌표 차이의 절댓값. 두 차이의 합은 격자에서의 맨해튼 거리이다.
    OS 전용 입력·화면 지우기 없이 표준 C17로 동작한다. 화면 문구는 ASCII이다.

 5. 실습 순서
    Render와 Move → 검 공격 → 화염 콜백 교체 → 적 행동 → 획득과 탈출.
    실험: 새 무기 함수를 같은 시그니처로 만들어 Hero.weapon에 등록한다.
    왜 Attack에 무기별 if를 추가하지 않아도 되는가? 호출 규약을 공유하기 때문이다.
    확장: 마나를 소모해 적을 밀치는 무기, 도망가는 몬스터, 보물 개수에 따른 엔딩.
==============================================================================
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { WIDTH = 15, HEIGHT = 9, ENEMIES = 4, MAX_HP = 30, MAX_MANA = 12 };
typedef struct Game Game;
typedef struct Monster Monster;
typedef void (*Weapon)(Game *, Monster *);
typedef void (*Think)(Game *, Monster *);

typedef struct
{
    int x, y, hp, mana, potions, treasure, key;
    Weapon weapon;
    const char *weaponName;
} Hero;

struct Monster
{
    int x, y, hp, power;
    char icon;
    const char *name;
    Think think;
};

struct Game
{
    char map[HEIGHT][WIDTH + 1];
    Hero hero;
    Monster enemies[ENEMIES];
    int turn, won;
};

static const char dungeon[HEIGHT][WIDTH + 1] = {
    "###############",
    "#....#...$...E#",
    "#....#........#",
    "#.$..#..###...#",
    "#.............#",
    "#..###...#....#",
    "#........#....#",
    "#.+...k..#.+..#",
    "###############"
};

// 한 줄에 한 명령만 허용한다. 긴 줄과 여러 글자를 다음 턴으로 넘기지 않는다.
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

// 살아 있는 적만 검색한다. 시체는 이동을 막지 않는다.
static Monster *MonsterAt(Game *game, int x, int y)
{
    for (int i = 0; i < ENEMIES; ++i)
        if (game->enemies[i].hp > 0 && game->enemies[i].x == x && game->enemies[i].y == y)
            return &game->enemies[i];
    return NULL;
}

static int Walkable(const Game *game, int x, int y)
{
    return x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT && game->map[y][x] != '#';
}

// 무기와 적 공격이 공유하는 피해 처리. 처치 보상은 생존→사망 전환에서만 지급한다.
static void HurtMonster(Game *game, Monster *enemy, int damage)
{
    if (enemy->hp <= 0) return;
    enemy->hp -= damage;
    printf("  HIT %s -%d\n", enemy->name, damage);
    if (enemy->hp <= 0)
    {
        enemy->hp = 0;
        game->hero.mana += 3;
        if (game->hero.mana > MAX_MANA) game->hero.mana = MAX_MANA;
        printf("  %s defeated! Mana recovered.\n", enemy->name);
    }
}

static void Sword(Game *game, Monster *enemy)
{
    HurtMonster(game, enemy, 6);
}

// 콜백 안에서 자원 조건을 검사한다. 마나 부족이면 공격을 취소하되 턴은 소비한다.
static void Fire(Game *game, Monster *enemy)
{
    if (game->hero.mana < 3) { puts("  Not enough mana. Equip the sword!"); return; }
    game->hero.mana -= 3;
    puts("  >>> FLAME STRIKE <<<");
    HurtMonster(game, enemy, 11);
}

static void Bite(Game *game, Monster *enemy)
{
    game->hero.hp -= enemy->power;
    if (game->hero.hp < 0) game->hero.hp = 0;
    printf("  %s attacks! HP -%d\n", enemy->name, enemy->power);
}

// 수문장과 보스는 자리를 지키며 인접한 용사만 공격한다.
static void Guard(Game *game, Monster *enemy)
{
    if (abs(enemy->x - game->hero.x) + abs(enemy->y - game->hero.y) == 1) Bite(game, enemy);
}

// 추격자는 가까이 있을 때 축 하나를 따라 움직인다. 최단 경로 탐색은 후속 과제이다.
static void Chase(Game *game, Monster *enemy)
{
    int dx = game->hero.x - enemy->x, dy = game->hero.y - enemy->y;
    int distance = abs(dx) + abs(dy);
    if (distance == 1) { Bite(game, enemy); return; }
    if (distance > 5) return;
    int nx = enemy->x + (dx > 0) - (dx < 0), ny = enemy->y;
    if (dx == 0 || !Walkable(game, nx, ny) || MonsterAt(game, nx, ny) != NULL)
    {
        nx = enemy->x;
        ny = enemy->y + (dy > 0) - (dy < 0);
    }
    if (Walkable(game, nx, ny) && MonsterAt(game, nx, ny) == NULL)
    { enemy->x = nx; enemy->y = ny; }
}

// 상태 전체를 새로 구성하므로 재시작 때 보물·처치·콜백이 이전 판에서 남지 않는다.
static void Start(Game *game)
{
    Game fresh = {0};
    memcpy(fresh.map, dungeon, sizeof dungeon);
    fresh.hero = (Hero){1, 1, MAX_HP, MAX_MANA, 2, 0, 0, Sword, "SWORD"};
    fresh.enemies[0] = (Monster){3, 2, 12, 2, 'g', "Goblin", Chase};
    fresh.enemies[1] = (Monster){7, 5, 12, 2, 'g', "Hunter", Chase};
    fresh.enemies[2] = (Monster){10, 2, 18, 3, 'G', "Sentinel", Guard};
    fresh.enemies[3] = (Monster){12, 1, 28, 4, 'B', "Gate keeper", Guard};
    *game = fresh;
}

static void Help(void)
{
    puts("  W/A/S/D: move   F: attack adjacent enemy   1/2: sword/flame");
    puts("  P: potion (+12 HP, +4 mana)   .: wait   H: help   Q: quit");
    puts("  Collect BOTH treasures ($) and key (k), defeat B, reach E.");
    puts("  Every action except help/invalid input costs a turn. Commands need Enter.");
}

// 지형 위에 캐릭터를 그리기만 하므로 이동해도 바닥이나 출구가 사라지지 않는다.
static void Render(Game *game)
{
    printf("\n+-- CALLBACK DUNGEON -- TURN %d --+\n", game->turn);
    printf("  HP %d/%d | MP %d/%d | POTIONS %d | %s\n", game->hero.hp, MAX_HP,
        game->hero.mana, MAX_MANA, game->hero.potions, game->hero.weaponName);
    printf("  TREASURE %d/2 | KEY %s | BOSS %s\n", game->hero.treasure,
        game->hero.key ? "YES" : "NO", game->enemies[3].hp == 0 ? "DOWN" : "ALIVE");
    for (int y = 0; y < HEIGHT; ++y)
    {
        fputs("  ", stdout);
        for (int x = 0; x < WIDTH; ++x)
        {
            Monster *enemy = MonsterAt(game, x, y);
            char tile = enemy != NULL ? enemy->icon : game->map[y][x];
            if (x == game->hero.x && y == game->hero.y) tile = '@';
            printf("%c ", tile);
        }
        putchar('\n');
    }
}

// 획득은 이동 성공 뒤에만 처리한다. 일회용 타일은 소비 직후 바닥으로 바꾼다.
static void Move(Game *game, int dx, int dy)
{
    int x = game->hero.x + dx, y = game->hero.y + dy;
    if (!Walkable(game, x, y)) { puts("  A wall blocks your path."); return; }
    if (MonsterAt(game, x, y) != NULL) { puts("  Enemy ahead! Use F to attack."); return; }
    game->hero.x = x; game->hero.y = y;
    char tile = game->map[y][x];
    if (tile == '$') { ++game->hero.treasure; puts("  TREASURE FOUND!"); }
    if (tile == 'k') { game->hero.key = 1; puts("  EXIT KEY FOUND!"); }
    if (tile == '+') { game->hero.hp = MAX_HP; game->hero.mana = MAX_MANA; puts("  Sanctuary: fully restored."); }
    if (tile == '$' || tile == 'k' || tile == '+') game->map[y][x] = '.';
    if (tile == 'E')
    {
        game->won = game->hero.key && game->hero.treasure == 2 && game->enemies[3].hp == 0;
        if (!game->won) puts("  Exit sealed: two treasures, key and defeated boss required.");
    }
}

// 대상 선택과 공격 방법을 분리한다. 장비만 바꾸면 같은 호출로 다른 기술을 사용한다.
static void Attack(Game *game)
{
    for (int i = 0; i < ENEMIES; ++i)
    {
        Monster *enemy = &game->enemies[i];
        if (enemy->hp > 0 && abs(enemy->x - game->hero.x) + abs(enemy->y - game->hero.y) == 1)
        {
            if (game->hero.weapon != NULL) game->hero.weapon(game, enemy);
            return;
        }
    }
    puts("  Your weapon cuts the air. No adjacent target.");
}

// 유효한 행동만 1을 반환한다. 게임 루프가 턴 소비와 적 갱신을 결정한다.
static int Act(Game *game, int key)
{
    switch (key)
    {
    case 'w': Move(game, 0, -1); break;
    case 's': Move(game, 0, 1); break;
    case 'a': Move(game, -1, 0); break;
    case 'd': Move(game, 1, 0); break;
    case 'f': Attack(game); break;
    case '1': game->hero.weapon = Sword; game->hero.weaponName = "SWORD"; puts("  Sword equipped."); break;
    case '2': game->hero.weapon = Fire; game->hero.weaponName = "FLAME"; puts("  Flame equipped."); break;
    case 'p':
        if (game->hero.potions == 0) { puts("  No potions left."); break; }
        --game->hero.potions;
        game->hero.hp += 12; game->hero.mana += 4;
        if (game->hero.hp > MAX_HP) game->hero.hp = MAX_HP;
        if (game->hero.mana > MAX_MANA) game->hero.mana = MAX_MANA;
        puts("  Potion used."); break;
    case '.': puts("  You wait. The dungeon does not."); break;
    case 'h': Help(); return 0;
    default: puts("  Unknown command. H shows help."); return 0;
    }
    return 1;
}

int main(void)
{
    Game game;
    Start(&game);
    Help();
    for (;;)
    {
        Render(&game);
        if (game.won || game.hero.hp == 0)
        {
            puts(game.won ? "  YOU ESCAPED! DUNGEON CLEAR!" : "  YOU FELL. GAME OVER.");
            puts("  R + Enter: retry | anything else: quit");
            if (ReadCommand() != 'r') break;
            Start(&game);
            continue;
        }
        fputs("  Action > ", stdout); fflush(stdout);
        int key = ReadCommand();
        if (key == EOF || key == 'q') break;
        if (!Act(&game, key)) continue;
        ++game.turn;
        if (game.won) continue;
        for (int i = 0; i < ENEMIES && game.hero.hp > 0; ++i)
            if (game.enemies[i].hp > 0 && game.enemies[i].think != NULL)
                game.enemies[i].think(&game, &game.enemies[i]);
    }
    return ferror(stdin) || ferror(stdout) ? EXIT_FAILURE : EXIT_SUCCESS;
}
