/* 창을 띄우지 않고 실제 게임 규칙과 ANSI 출력을 검증하는 테스트이다. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../game.h"
#include "../render.h"
static int events[4];
static void Receive(Event event) { ++events[event]; }
static void Fresh(Game *game) { memset(events, 0, sizeof events); StartGame(game, 7, Receive); }
static void EmptyRoad(Game *game) { game->nextTraffic = 1000000; memset(game->traffic, 0, sizeof game->traffic); }

int main(int argc, char **argv)
{
    Game game;
    Fresh(&game);
    if (argc > 1 && strcmp(argv[1], "--preview") == 0)
    {
        game.distance = 345; game.speed = 0.76f; game.heat = 0.84f; game.fuel = 0.31f;
        game.traffic[0] = (Obstacle){1, 0, 52}; game.traffic[1] = (Obstacle){1, 2, 88};
        Render(&game); return 0;
    }
    assert(game.lane == 1 && game.fuel == 1 && game.heat == 0);
    EmptyRoad(&game);
    for (int i = 0; i < 40; ++i) UpdateGame(&game, 0.05f, 1, 0);
    assert(game.speed > 0.99f && game.distance > 40 && game.heat > 0);
    float oldHeat = game.heat, oldDistance = game.distance;
    UpdateGame(&game, 0.05f, 0, 0);
    assert(game.speed < 1 && game.speed > 0.9f && game.heat < oldHeat && game.distance > oldDistance);
    for (int i = 0; i < 100; ++i) UpdateGame(&game, 0.05f, 0, 0);
    assert(game.speed == 0 && game.heat == 0);
    puts("PASS acceleration / release / cooling / coasting");

    Fresh(&game); EmptyRoad(&game);
    for (int i = 0; i < 240; ++i) UpdateGame(&game, 0.05f, 1, 0);
    assert(game.phase == OVERHEATED && game.heat == 1 && game.speed == 0 && events[BOOM] == 1);
    oldDistance = game.distance;
    UpdateGame(&game, 0.05f, 1, 1);
    assert(game.distance == oldDistance && events[BOOM] == 1);
    puts("PASS full heat explodes once / game-over freezes distance");

    Fresh(&game);
    for (int i = 0; i < 361; ++i) UpdateGame(&game, 0.05f, 0, 0);
    assert(game.phase == EMPTY && game.fuel == 0 && game.distance == 0 && events[DRY] == 1);
    Fresh(&game); game.phase = READY;
    UpdateGame(&game, 0.05f, 1, 1);
    assert(game.fuel == 1 && game.distance == 0 && game.lane == 1);
    puts("PASS fuel deadline while stationary / ready does not consume fuel");

    Fresh(&game); EmptyRoad(&game);
    game.distance = 419; game.speed = 1; game.heat = 0.5f; game.fuel = 0.1f;
    UpdateGame(&game, 0.05f, 0, 0);
    assert(game.fuel == 1 && game.nextFuel == 840 && game.heat > 0.4f && events[REFUEL] == 1);
    UpdateGame(&game, 0.05f, 0, 0);
    assert(events[REFUEL] == 1);
    Fresh(&game); EmptyRoad(&game);
    game.distance = 419; game.speed = 1; game.fuel = 0.001f;
    UpdateGame(&game, 0.05f, 0, 0);
    assert(game.phase == EMPTY && events[REFUEL] == 0);
    puts("PASS checkpoint refills fuel only / deadline takes priority");

    Fresh(&game); game.traffic[0] = (Obstacle){1, 1, 13}; game.speed = 1;
    UpdateGame(&game, 0.05f, 0, 0);
    assert(game.phase == CRASHED && events[HIT] == 1 && game.speed == 0);
    Fresh(&game); game.traffic[0] = (Obstacle){1, 0, 13}; game.speed = 1;
    UpdateGame(&game, 0.05f, 0, 0);
    assert(game.phase == RUNNING);
    game.traffic[0].ahead = -13;
    UpdateGame(&game, 0.05f, 1, 0);
    assert(!game.traffic[0].active);
    game.distance = game.nextTraffic;
    UpdateGame(&game, 0.05f, 0, 0);
    assert(game.traffic[0].active && game.traffic[0].ahead == 100);
    puts("PASS collision / adjacent-lane avoidance / pool reuse");

    Fresh(&game); EmptyRoad(&game);
    for (int i = 0; i < 20; ++i) UpdateGame(&game, 0.05f, 0, -1);
    assert(game.lane == 0);
    for (int i = 0; i < 20; ++i) UpdateGame(&game, 0.05f, 0, 1);
    assert(game.lane == 2);
    Fresh(&game); game.onEvent = NULL; game.heat = 1;
    UpdateGame(&game, 0.05f, 1, 0);
    assert(game.phase == OVERHEATED);
    puts("PASS steering bounds / optional callback / full restart reset");

    for (unsigned seed = 1; seed <= 20; ++seed)
    {
        StartGame(&game, seed, Receive);
        int gas = 1;
        for (int tick = 0; tick < 4000 && game.phase == RUNNING && game.distance < 1261; ++tick)
        {
            if (game.heat > 0.70f) gas = 0;
            if (game.heat < 0.25f) gas = 1;
            int target = game.lane;
            for (int i = 0; i < TRAFFIC; ++i)
                if (game.traffic[i].active && game.traffic[i].lane == game.lane && game.traffic[i].ahead > -14 && game.traffic[i].ahead < 32)
                {
                    for (int lane = 0; lane < LANES; ++lane)
                    {
                        if (lane == game.lane || lane - game.lane > 1 || game.lane - lane > 1) continue;
                        int safe = 1;
                        for (int j = 0; j < TRAFFIC; ++j)
                            if (game.traffic[j].active && game.traffic[j].lane == lane && game.traffic[j].ahead > -14 && game.traffic[j].ahead < 40) safe = 0;
                        if (safe) { target = lane; break; }
                    }
                }
            UpdateGame(&game, 0.02f, gas, target - game.lane);
        }
        assert(game.phase == RUNNING && game.distance >= 1260 && game.nextFuel >= 1680);
    }
    puts("PASS twenty seeded runs survive three fuel points with cooling and avoidance");
    return 0;
}
