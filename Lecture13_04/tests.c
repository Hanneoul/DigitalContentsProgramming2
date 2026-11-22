/* 이벤트/풀의 경계와 실제 세 웨이브 클리어를 검사한다. 게임 프로젝트에는 포함하지 않는다. */
#undef NDEBUG
#include <assert.h>
#define main gate_main
#include "main.c"
#undef main

static void Extra(Game *game, const Event *event) { (void)game; (void)event; }

int main(int argc, char **argv)
{
    Game game;
    Start(&game);
    assert(!Subscribe(&game, NULL) && !Subscribe(&game, Record));
    assert(Subscribe(&game, Extra) && !Subscribe(&game, Extra));
    for (int i = 0; i < POOL; ++i) assert(Spawn(game.enemies, 0, 10, 1) != NULL);
    assert(Spawn(game.enemies, 0, 10, 1) == NULL);
    game.enemies[2].active = 0;
    assert(Spawn(game.enemies, 1, 15, 2) == &game.enemies[2]);
    Start(&game);
    Object *enemy = Spawn(game.enemies, 1, 3, 1);
    Spawn(game.bullets, 1, 1, 1); Spawn(game.bullets, 1, 1, 1);
    UpdateBullets(&game);
    assert(!enemy->active && game.score == 0 && game.eventCount == 2);
    Spawn(game.enemies, 2, 15, 2); // 같은 슬롯을 재사용해도 큐의 차선은 1로 남는다.
    assert(game.queue[1].lane == 1);
    Dispatch(&game); assert(game.score == 20 && game.eventCount == 0);
    Dispatch(&game); assert(game.score == 20);
    Start(&game); game.turn = 2;
    Spawn(game.enemies, 0, 5, 1); Spawn(game.bullets, 0, 4, 1);
    UpdateEnemies(&game); Dispatch(&game);
    assert(game.score == 20 && !game.enemies[0].active && !game.bullets[0].active);
    Start(&game);
    for (int i = 0; i <= QUEUE; ++i) Emit(&game, FIRED, 0, 1);
    assert(game.failed && game.eventCount == QUEUE);
    Start(&game);
    for (int i = 0; i < 200 && game.hp > 0; ++i) Tick(&game);
    assert(game.hp == 0 && !game.won);
    Start(&game);
    game.hp = 1; game.wave = 3; game.spawned = 10; game.turn = 1;
    Spawn(game.enemies, 0, 1, 1); Tick(&game);
    assert(game.hp == 0 && !game.won); // 마지막 적이 기지를 파괴하면 웨이브 완료보다 패배가 우선이다.
    Start(&game);
    FILE *route = argc > 1 ? fopen(argv[1], "w") : NULL;
    assert(argc == 1 || route != NULL);
    for (int step = 0; step < 600 && !game.won && game.hp > 0; ++step)
    {
        int key = '.', closest = LENGTH + 1, target = game.lane;
        for (int i = 0; i < POOL; ++i)
            if (game.enemies[i].active && game.enemies[i].x < closest)
            { closest = game.enemies[i].x; target = game.enemies[i].lane; }
        if (ActiveCount(game.enemies) >= 4 && game.bombs > 0) key = 'b';
        else if (target < game.lane) key = 'a';
        else if (target > game.lane) key = 'd';
        else if (closest <= LENGTH) key = 'f';
        if (route != NULL) fprintf(route, "%c\n", key);
        assert(Act(&game, key)); Tick(&game);
        assert(!game.failed && game.eventCount == 0);
    }
    assert(game.won && game.hp > 0 && game.wave == 3);
    if (route != NULL) { fputs("q\n", route); assert(fclose(route) == 0); }
    Start(&game);
    assert(game.hp == 5 && game.score == 0 && game.wave == 1 && ActiveCount(game.enemies) == 0);
    puts("PASS: listeners, pool reuse, collision, queue bounds, loss and three-wave victory");
    return 0;
}
