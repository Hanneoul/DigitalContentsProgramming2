/* 게임에 연결하지 않는 별도 검증 프로그램. main.c의 실제 규칙을 직접 검사한다. */
#undef NDEBUG
#include <assert.h>
#define main dungeon_main
#include "main.c"
#undef main

// 벽과 살아 있는 적을 피하는 BFS. 테스트용 자동 플레이어이며 게임 AI는 아니다.
static int NextStep(Game *game, int tx, int ty)
{
    int queue[WIDTH * HEIGHT], first[WIDTH * HEIGHT];
    int begin = 0, end = 0;
    const int dx[] = {1, -1, 0, 0}, dy[] = {0, 0, 1, -1};
    const int key[] = {'d', 'a', 's', 'w'};
    for (int i = 0; i < WIDTH * HEIGHT; ++i) first[i] = -1;
    int start = game->hero.y * WIDTH + game->hero.x;
    first[start] = 0; queue[end++] = start;
    while (begin < end)
    {
        int at = queue[begin++], x = at % WIDTH, y = at / WIDTH;
        if (x == tx && y == ty) return first[at];
        for (int d = 0; d < 4; ++d)
        {
            int nx = x + dx[d], ny = y + dy[d], next = ny * WIDTH + nx;
            if (!Walkable(game, nx, ny) || MonsterAt(game, nx, ny) != NULL) continue;
            if (first[next] != -1) continue;
            first[next] = at == start ? key[d] : first[at];
            queue[end++] = next;
        }
    }
    return 0;
}

static void Turn(Game *game, int key)
{
    if (!Act(game, key)) return;
    ++game->turn;
    if (game->won) return;
    for (int i = 0; i < ENEMIES && game->hero.hp > 0; ++i)
        if (game->enemies[i].hp > 0 && game->enemies[i].think != NULL)
            game->enemies[i].think(game, &game->enemies[i]);
}

int main(int argc, char **argv)
{
    Game game;
    Start(&game);
    assert(game.hero.weapon == Sword && game.hero.hp == MAX_HP);
    int turn = game.turn;
    Turn(&game, '?'); assert(game.turn == turn);
    Move(&game, -1, 0); assert(game.hero.x == 1 && game.hero.y == 1);
    game.hero.x = 1; game.hero.y = 3;
    Move(&game, 1, 0); assert(game.hero.treasure == 1 && game.map[3][2] == '.');
    Move(&game, -1, 0); Move(&game, 1, 0); assert(game.hero.treasure == 1);
    game.hero.x = 12; game.hero.y = 2;
    Move(&game, 1, 0); Move(&game, 0, -1); assert(!game.won);
    Start(&game);
    game.hero.weapon = Fire; game.hero.mana = 0;
    int hp = game.enemies[0].hp;
    Fire(&game, &game.enemies[0]); assert(game.enemies[0].hp == hp);
    game.hero.weapon = NULL; game.hero.x = 3; game.hero.y = 1;
    Attack(&game); assert(game.enemies[0].hp == hp);
    HurtMonster(&game, &game.enemies[0], 99);
    int mana = game.hero.mana;
    HurtMonster(&game, &game.enemies[0], 99); assert(game.hero.mana == mana);
    Start(&game); game.hero.hp = 1;
    for (int i = 0; i < 20 && game.hero.hp > 0; ++i) Turn(&game, '.');
    assert(game.hero.hp == 0);
    Start(&game);
    FILE *route = argc > 1 ? fopen(argv[1], "w") : NULL;
    assert(argc == 1 || route != NULL);
    for (int step = 0; step < 300 && !game.won && game.hero.hp > 0; ++step)
    {
        int key = 0, adjacent = 0, intercept = 0;
        for (int i = 0; i < ENEMIES; ++i)
        {
            if (game.enemies[i].hp > 0 && abs(game.enemies[i].x - game.hero.x) + abs(game.enemies[i].y - game.hero.y) == 1) adjacent = 1;
            Monster *enemy = &game.enemies[i];
            if (enemy->hp > 0 && enemy->think == Chase &&
                abs(enemy->x - game.hero.x) + abs(enemy->y - game.hero.y) <= 4)
            {
                const int dx[] = {0, 0, 1, -1}, dy[] = {1, -1, 0, 0};
                for (int d = 0; d < 4 && intercept == 0; ++d)
                    if (Walkable(&game, enemy->x + dx[d], enemy->y + dy[d]))
                        intercept = NextStep(&game, enemy->x + dx[d], enemy->y + dy[d]);
            }
        }
        if (game.hero.hp <= 12 && game.hero.potions > 0) key = 'p';
        else if (adjacent)
        {
            if (game.hero.mana >= 3 && game.hero.weapon != Fire) key = '2';
            else if (game.hero.mana < 3 && game.hero.weapon != Sword) key = '1';
            else key = 'f';
        }
        else if (intercept != 0) key = intercept;
        else if (game.map[3][2] == '$') key = NextStep(&game, 2, 3);
        else if (!game.hero.key) key = NextStep(&game, 6, 7);
        else if (game.map[1][9] == '$') key = NextStep(&game, 9, 1);
        else if (game.enemies[3].hp > 0) key = NextStep(&game, 12, 2);
        else key = NextStep(&game, 13, 1);
        assert(key != 0);
        if (route != NULL) fprintf(route, "%c\n", key);
        Turn(&game, key);
    }
    fprintf(stderr, "route result: turn=%d hp=%d pos=%d,%d treasure=%d key=%d boss=%d won=%d\n",
        game.turn, game.hero.hp, game.hero.x, game.hero.y, game.hero.treasure, game.hero.key, game.enemies[3].hp, game.won);
    assert(game.won && game.hero.hp > 0 && game.hero.treasure == 2 && game.hero.key);
    if (route != NULL) { fputs("q\n", route); assert(fclose(route) == 0); }
    Start(&game);
    assert(!game.won && game.turn == 0 && game.hero.treasure == 0 && game.enemies[3].hp == 28);
    puts("PASS: dungeon rules, failure, restart and complete playable victory route");
    return 0;
}
