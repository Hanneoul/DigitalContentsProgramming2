#include <windows.h>
#include "game.h"

static int failures;
#define CHECK(test) do { if (!(test)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #test); ++failures; } } while (0)
static const char valid_map[] = "##########\n##P0@-()##\n##P1P2P3##\nH0P4P5P6##\n##########\n";

/* Use a real temporary stream to cover the same parser as production map loading. */
static int load_text(Game *g, const char *text)
{
    char error[256];
    FILE *file = NULL;
    int ok;
    if (tmpfile_s(&file) != 0) { CHECK(0); return 0; }
    fputs(text, file); rewind(file);
    ok = game_load(g, file, error, sizeof(error));
    (fclose)(file); /* tmpfile is intentionally outside the fopen tracker. */
    return ok;
}

/* Create a deterministic empty arena; only explicitly placed actors are alive. */
static void arena(Game *g, Command commands[PLAYER_COUNT])
{
    int i;
    memset(g, 0, sizeof(*g));
    g->size = 7; g->key_owner = g->winner = -1;
    g->key.x = g->key_start.x = 5; g->key.y = g->key_start.y = 5;
    g->exit.x = 6; g->exit.y = 6;
    memset(g->terrain, ' ', sizeof(g->terrain));
    g->terrain[6][6] = 'E';
    for (i = 0; i < PLAYER_COUNT; ++i) {
        g->players[i].pos.x = g->players[i].spawn.x = i;
        g->players[i].pos.y = g->players[i].spawn.y = 0;
        g->players[i].rest = 100;
        commands[i] = COMMAND_ATTACK_UP;
    }
}

/* Activate a player and give it a separate initial spawn for lifecycle tests. */
static void place(Game *g, int id, int x, int y, int hp)
{
    g->players[id].pos.x = x; g->players[id].pos.y = y;
    g->players[id].hp = hp; g->players[id].rest = 0;
}

/* Exercise simultaneous conflicts, swaps, propagation and no-index-priority claims. */
static void test_collisions(void)
{
    Game g; Command c[PLAYER_COUNT];
    arena(&g, c);
    place(&g, 0, 2, 1, 2); place(&g, 1, 1, 2, 2); place(&g, 2, 3, 2, 2);
    c[0] = COMMAND_DOWN; c[1] = COMMAND_RIGHT; c[2] = COMMAND_LEFT;
    game_resolve(&g, c);
    CHECK(g.players[0].hp == 1 && g.players[1].hp == 1 && g.players[2].hp == 1);
    CHECK(g.players[0].pos.y == 1 && g.players[1].pos.x == 1 && g.players[2].pos.x == 3);
    arena(&g, c);
    place(&g, 0, 1, 1, 2); place(&g, 1, 2, 1, 2);
    c[0] = COMMAND_RIGHT; c[1] = COMMAND_LEFT;
    game_resolve(&g, c);
    CHECK(g.players[0].pos.x == 2 && g.players[1].pos.x == 1 && g.players[0].hp == 2);
    arena(&g, c);
    place(&g, 0, 1, 1, 2); place(&g, 1, 2, 1, 2); place(&g, 2, 3, 1, 2);
    c[0] = c[1] = COMMAND_RIGHT;
    game_resolve(&g, c);
    CHECK(g.cancelled[0] && g.cancelled[1] && !g.cancelled[2]);
    CHECK(g.players[0].pos.x == 1 && g.players[1].pos.x == 2);
    arena(&g, c);
    place(&g, 0, 1, 1, 2); place(&g, 1, 3, 1, 2); g.key_owner = 0;
    c[0] = COMMAND_DROP_RIGHT; c[1] = COMMAND_LEFT;
    game_resolve(&g, c);
    CHECK(g.cancelled[0] && g.cancelled[1] && g.key_owner == 0);
    arena(&g, c);
    place(&g, 0, 1, 1, 2); place(&g, 1, 2, 1, 2); g.key_owner = 0;
    c[0] = COMMAND_DROP_RIGHT;
    game_resolve(&g, c);
    CHECK(g.cancelled[0] && g.players[0].hp == 1 && g.players[1].hp == 2);
    arena(&g, c); place(&g, 0, 0, 0, 2); c[0] = COMMAND_LEFT;
    game_resolve(&g, c); CHECK(g.cancelled[0] && g.players[0].hp == 1);
    arena(&g, c); place(&g, 0, 1, 1, 2); g.terrain[1][2] = '#'; c[0] = COMMAND_RIGHT;
    game_resolve(&g, c); CHECK(g.cancelled[0] && g.players[0].hp == 1);
}

/* Verify all four three-cell attack fans, movement dodges and the per-turn damage cap. */
static void test_attacks(void)
{
    Game g; Command c[PLAYER_COUNT];
    const int dx[4] = {0, 0, -1, 1}, dy[4] = {-1, 1, 0, 0};
    int d, k;
    for (d = 0; d < 4; ++d) {
        arena(&g, c); place(&g, 0, 3, 3, 2); c[0] = (Command)(COMMAND_ATTACK_UP + d);
        for (k = -1; k <= 1; ++k) place(&g, k + 2, 3 + dx[d] + (d < 2 ? k : 0), 3 + dy[d] + (d >= 2 ? k : 0), 2);
        game_resolve(&g, c);
        CHECK(g.players[1].hp == 1 && g.players[2].hp == 1 && g.players[3].hp == 1);
    }
    arena(&g, c); place(&g, 0, 2, 2, 2); place(&g, 1, 1, 3, 2); place(&g, 2, 3, 3, 2);
    c[0] = (Command)999; c[1] = c[2] = COMMAND_ATTACK_UP;
    game_resolve(&g, c); CHECK(g.players[0].hp == 1 && g.cancelled[0]);
    arena(&g, c); place(&g, 0, 2, 3, 2); place(&g, 1, 2, 2, 2);
    c[0] = COMMAND_ATTACK_UP; c[1] = COMMAND_UP;
    game_resolve(&g, c); CHECK(g.players[1].hp == 2 && g.players[1].pos.y == 1);
    arena(&g, c); place(&g, 0, 2, 3, 1); place(&g, 1, 2, 2, 1);
    c[0] = COMMAND_ATTACK_UP; c[1] = COMMAND_ATTACK_DOWN;
    game_resolve(&g, c); CHECK(g.players[0].hp == 0 && g.players[1].hp == 0);
}

/* Item commands are explicit; death and victory are resolved after all attacks. */
static void test_items(void)
{
    Game g; Command c[PLAYER_COUNT];
    arena(&g, c); place(&g, 0, 4, 5, 2); c[0] = COMMAND_RIGHT;
    game_resolve(&g, c); CHECK(g.key_owner == -1); /* Walking over key does not pick it up. */
    arena(&g, c); place(&g, 0, 4, 5, 2); c[0] = COMMAND_PICK_RIGHT;
    game_resolve(&g, c); CHECK(g.key_owner == 0);
    c[0] = COMMAND_DROP_UP; game_resolve(&g, c);
    CHECK(g.key_owner == -1 && g.key.x == 4 && g.key.y == 4);
    arena(&g, c); place(&g, 0, 4, 5, 2); place(&g, 1, 6, 5, 2);
    c[0] = COMMAND_PICK_RIGHT; c[1] = COMMAND_PICK_LEFT;
    game_resolve(&g, c); CHECK(g.key_owner == -1 && g.players[0].hp == 1 && g.players[1].hp == 1);
    arena(&g, c); place(&g, 0, 5, 6, 2); c[0] = COMMAND_RIGHT;
    game_resolve(&g, c); CHECK(g.winner == -1 && g.players[0].pos.x == 5 && g.players[0].hp == 1);
    arena(&g, c); place(&g, 0, 5, 6, 2); g.key_owner = 0; c[0] = COMMAND_RIGHT;
    game_resolve(&g, c); CHECK(g.winner == 0);
    arena(&g, c); place(&g, 0, 5, 6, 1); place(&g, 1, 6, 5, 2); g.key_owner = 0;
    c[0] = COMMAND_RIGHT; c[1] = COMMAND_ATTACK_DOWN;
    game_resolve(&g, c);
    CHECK(g.winner == -1 && g.key_owner == -1 && g.players[0].hp == 0);
    CHECK(g.key.x == g.key_start.x && g.key.y == g.key_start.y);
    arena(&g, c); place(&g, 0, 2, 2, 1); g.key_owner = 0; c[0] = (Command)-1;
    game_resolve(&g, c); CHECK(g.key_owner == -1 && g.key.x == 2 && g.key.y == 2);
    arena(&g, c); place(&g, 0, 4, 5, 1); place(&g, 1, 4, 6, 2);
    c[0] = COMMAND_PICK_RIGHT; c[1] = COMMAND_ATTACK_UP;
    game_resolve(&g, c); CHECK(g.players[0].hp == 0 && g.key_owner == -1 && g.key.x == 4 && g.key.y == 5);
}

/* Rest is 1,2,4... whole skipped turns, not the death turn or the respawn turn. */
static void test_respawn(void)
{
    Game g; Command c[PLAYER_COUNT];
    int death, t;
    arena(&g, c); place(&g, 0, 2, 2, 1); c[0] = (Command)-1;
    for (death = 1; death <= 3; ++death) {
        g.players[0].hp = 1;
        game_resolve(&g, c);
        CHECK(g.players[0].deaths == (unsigned)death && g.players[0].rest == (UINT64_C(1) << (death - 1)));
        for (t = 0; t < (1 << (death - 1)); ++t) { game_begin_turn(&g); CHECK(g.players[0].hp == 0); }
        game_begin_turn(&g); CHECK(g.players[0].hp == 2 && g.players[0].pos.x == 0 && g.players[0].pos.y == 0);
    }
    g.players[0].hp = 0; g.players[0].rest = 0; place(&g, 1, 0, 0, 2);
    game_begin_turn(&g); CHECK(g.players[0].hp == 0 && g.players[0].deaths == 3);
    g.players[1].pos.x = 1;
    game_begin_turn(&g); CHECK(g.players[0].hp == 2);
}

/* Check map errors and serialize a v2 snapshot through an actual isolated worker. */
static void test_files(void)
{
    Game g; Snapshot s;
    char path[MAX_PATH], directory[MAX_PATH], log_path[MAX_PATH], changed[sizeof(valid_map)];
    FILE *file;
    DWORD length = GetTempPathA(sizeof(directory), directory);
    CHECK(length > 0 && length < sizeof(directory));
    if (!length || length >= sizeof(directory)) return;
    if (!GetTempFileNameA(directory, "fgc", 0, path)) { CHECK(0); return; }
    if (!GetTempFileNameA(directory, "fgl", 0, log_path)) { CHECK(0); DeleteFileA(path); return; }
    CHECK(load_text(&g, valid_map)); CHECK(g.size == 5 && g.players[6].pos.x == 0 && g.terrain[3][3] == ' ');
    CHECK(!load_text(&g, "")); CHECK(!load_text(&g, "###\n"));
    CHECK(!load_text(&g, "######\n####\n######\n"));
    strcpy(changed, valid_map); changed[0] = '?'; CHECK(!load_text(&g, changed));
    strcpy(changed, valid_map); changed[14] = '1'; CHECK(!load_text(&g, changed));
    strcpy(changed, valid_map); changed[33] = ' '; changed[34] = ' '; CHECK(!load_text(&g, changed));
    CHECK(!load_text(&g, "##########\n##P0@-()##\n##########\nH0P1P2P3P4\nP5        \n"));
    CHECK(load_text(&g, "##########\r\n##P0@-()##\r\n##P1P2P3##\r\nH0P4P5P6##\r\n##########"));
    g.turn = 13;
    CHECK(snapshot_write(&g, 0, path));
    file = fopen(path, "r"); CHECK(file != NULL);
    if (file) { CHECK(snapshot_read(file, &s)); fclose(file); CHECK(s.turn == 13 && s.self == 0 && s.key.x == 2 && s.key.y == 1 && s.players[6].hp == 2); }
    CHECK(worker_command(0, path, log_path) == COMMAND_PICK_RIGHT);
    file = fopen(log_path, "r"); CHECK(file != NULL);
    if (file) { CHECK(fgetc(file) == EOF); fclose(file); }
    DeleteFileA(path); DeleteFileA(log_path);
}

/* Instructor regression entry point; normal gameplay never executes test fixtures. */
int self_test(void)
{
    const char *keys = "wsadtgfhikjl";
    Command command;
    int i;
    /* Verify every key, both item modes, invalid keys and uppercase input. */
    for (i = 0; i < 12; ++i) {
        CHECK(input_decode(keys[i], 0, &command) && (int)command == i);
        CHECK(input_decode(keys[i], 1, &command) && (int)command == (i < 8 ? i : i + 4));
    }
    CHECK(!input_decode(27, 0, &command) && !input_decode('\n', 0, &command));
    CHECK(input_decode('W', 0, &command) && command == COMMAND_UP);
    test_collisions(); test_attacks(); test_items(); test_respawn(); test_files();
    printf("Self-test: %s (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
