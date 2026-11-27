#include <windows.h>
#include "game.h"

static int failures;
#define CHECK(test) do { if (!(test)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #test); ++failures; } } while (0)
static const char valid_map[] = "##########\n##P0@-()##\n##P1P2P3##\n  P4P5P6##\n##########\n";

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
        g->players[i].attack_charge = MAX_ATTACK_CHARGE;
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

/* Validate maps/snapshots and all six worker contracts without prescribing AI strategy. */
static void test_files(void)
{
    Game g; Snapshot s;
    char path[MAX_PATH], directory[MAX_PATH], log_path[MAX_PATH], changed[sizeof(valid_map)];
    FILE *file;
    int id;
    DWORD length = GetTempPathA(sizeof(directory), directory);
    CHECK(length > 0 && length < sizeof(directory));
    if (!length || length >= sizeof(directory)) return;
    if (!GetTempFileNameA(directory, "fgc", 0, path)) { CHECK(0); return; }
    if (!GetTempFileNameA(directory, "fgl", 0, log_path)) { CHECK(0); DeleteFileA(path); return; }
    CHECK(load_text(&g, valid_map)); CHECK(g.size == 5 && PLAYER_COUNT == 6 && g.terrain[3][3] == ' ');
    /* A UTF-8 editor signature must not count toward the first row's cell width. */
    {
        char with_bom[sizeof(valid_map) + 3];
        memcpy(with_bom, "\xEF\xBB\xBF", 3);
        memcpy(with_bom + 3, valid_map, sizeof(valid_map));
        CHECK(load_text(&g, with_bom));
        CHECK(g.size == 5 && g.players[0].pos.x == 1);
    }
    CHECK(!load_text(&g, "")); CHECK(!load_text(&g, "###\n"));
    CHECK(!load_text(&g, "######\n####\n######\n"));
    strcpy_s(changed, sizeof(changed), valid_map); changed[0] = '?'; CHECK(!load_text(&g, changed));
    strcpy_s(changed, sizeof(changed), valid_map); changed[14] = '1'; CHECK(!load_text(&g, changed));
    strcpy_s(changed, sizeof(changed), valid_map); changed[35] = ' '; changed[36] = ' '; CHECK(!load_text(&g, changed));
    CHECK(!load_text(&g, "##########\n##P0@-()##\n##########\n  P1P2P3P4\nP5        \n"));
    CHECK(load_text(&g, "##########\r\n##P0@-()##\r\n##P1P2P3##\r\n  P4P5P6##\r\n##########"));
    g.turn = 13;
    CHECK(snapshot_write(&g, 0, path));
    file = fopen(path, "r"); CHECK(file != NULL);
    if (file) { CHECK(snapshot_read(file, &s)); fclose(file); CHECK(s.turn == 13 && s.self == 0 && s.key.x == 2 && s.key.y == 1 && s.players[5].hp == 2); }
    for (id = 0; id < PLAYER_COUNT; ++id) {
        Command command;
        CHECK(snapshot_write(&g, id, path));
        command = worker_command(id, path, log_path);
        CHECK((int)command >= 0 && command < COMMAND_COUNT);
    }
    file = fopen(log_path, "r"); CHECK(file != NULL);
    if (file) { CHECK(fgetc(file) == EOF); fclose(file); }
    DeleteFileA(path); DeleteFileA(log_path);
}

/* A carrier cannot attack in any direction, and a fatal penalty drops the key. */
static void test_carrier_attack_penalty(void)
{
    Game g;
    Command c[PLAYER_COUNT];
    const int dx[4] = {0, 0, -1, 1}, dy[4] = {-1, 1, 0, 0};
    int d;
    for (d = 0; d < 4; ++d) {
        arena(&g, c);
        place(&g, 0, 3, 3, 2);
        place(&g, 1, 3 + dx[d], 3 + dy[d], 2);
        g.key_owner = 0;
        c[0] = c[1] = (Command)(COMMAND_ATTACK_UP + d);
        game_resolve(&g, c);
        CHECK(g.cancelled[0] && g.players[0].hp == 1 && g.damaged[0]);
        CHECK(g.players[1].hp == 2 && !g.attack[3 + dy[d]][3 + dx[d]]);
        CHECK(g.key_owner == 0);
    }
    arena(&g, c);
    place(&g, 0, 2, 2, 1);
    g.key_owner = 0;
    c[0] = COMMAND_ATTACK_UP;
    game_resolve(&g, c);
    CHECK(g.players[0].hp == 0 && g.players[0].deaths == 1 && g.players[0].rest == 1);
    CHECK(g.key_owner == -1 && g.key.x == 2 && g.key.y == 2);
    /* Incoming attack and command penalty still respect the original damage cap. */
    arena(&g, c);
    place(&g, 0, 2, 2, 2); place(&g, 1, 2, 3, 2);
    g.key_owner = 0;
    c[0] = c[1] = COMMAND_ATTACK_UP;
    game_resolve(&g, c);
    CHECK(g.players[0].hp == 1 && g.players[1].hp == 2);
}

/* Boundaries and exact semantics: one escape disables the no-escape cutoff. */
static void test_no_escape_draw(void)
{
    Game g;
    Command c[PLAYER_COUNT];
    int id;
    arena(&g, c);
    g.turn = 999; game_check_no_escape(&g);
    CHECK(!g.game_over && !g.draw);
    g.turn = 1000; game_check_no_escape(&g);
    CHECK(g.game_over && g.draw && g.finished_count == 0 && g.winner == -1);
    for (id = 0; id < PLAYER_COUNT; ++id) CHECK(g.players[id].rank == 0);
    arena(&g, c); g.turn = 1000;
    place(&g, 0, 5, 6, 2); g.key_owner = 0; c[0] = COMMAND_RIGHT;
    game_resolve(&g, c);
    CHECK(g.finished_count == 1 && !g.game_over && !g.draw);
    g.turn = 2000; game_check_no_escape(&g);
    CHECK(!g.game_over && !g.draw);
}

/* Escape order survives later turns; removed players neither respawn nor obstruct. */
static void test_escape_ranking(void)
{
    Game g;
    Command c[PLAYER_COUNT];
    const int order[5] = {2, 0, 4, 1, 3};
    int n, id;
    arena(&g, c);
    for (n = 0; n < 5; ++n) {
        id = order[n];
        game_begin_turn(&g);
        place(&g, id, 5, 6, 2);
        g.key_owner = id; c[id] = COMMAND_RIGHT;
        game_resolve(&g, c);
        CHECK(g.players[id].rank == n + 1 && g.players[id].hp == 0);
        CHECK(g.finish_order[n] == id && g.finish_turn[n] == g.turn);
        CHECK(g.key_owner == -1 && g.key.x == g.key_start.x && g.key.y == g.key_start.y);
        CHECK(g.winner == 2);
        if (n < 4) CHECK(!g.game_over && g.finished_count == n + 1);
    }
    CHECK(g.game_over && g.finished_count == PLAYER_COUNT);
    CHECK(g.finish_order[5] == 5 && g.players[5].rank == 6 && g.finish_turn[5] == -1);
    game_begin_turn(&g);
    for (id = 0; id < PLAYER_COUNT; ++id) CHECK(g.players[id].hp == 0 && g.players[id].rank > 0);
    game_resolve(&g, c);
    CHECK(g.finished_count == PLAYER_COUNT);
}

/* Unique-name storage: next free slot, no overwrite, and exactly 100-name exhaustion. */
static void test_rank_files(void)
{
    Game g;
    Command c[PLAYER_COUNT];
    char directory[MAX_PATH], temp[MAX_PATH], saved[MAX_PATH], candidate[MAX_PATH];
    HANDLE file;
    FILE *readback;
    int i;
    arena(&g, c);
    g.game_over = 1; g.finished_count = PLAYER_COUNT;
    for (i = 0; i < PLAYER_COUNT; ++i) { g.finish_order[i] = PLAYER_COUNT - 1 - i; g.finish_turn[i] = i + 1; }
    CHECK(GetTempPathA(sizeof(temp), temp) > 0);
    if (!GetTempFileNameA(temp, "frk", 0, directory)) { CHECK(0); return; }
    DeleteFileA(directory);
    if (!CreateDirectoryA(directory, NULL)) { CHECK(0); return; }
    CHECK(rank_save(&g, directory, saved, sizeof(saved)) == 1 && strstr(saved, "rank00.txt") != NULL);
    CHECK(rank_save(&g, directory, saved, sizeof(saved)) == 1 && strstr(saved, "rank01.txt") != NULL);
    for (i = 2; i < 100; ++i) {
        snprintf(candidate, sizeof(candidate), "%s\\rank%02d.txt", directory, i);
        file = CreateFileA(candidate, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        CHECK(file != INVALID_HANDLE_VALUE);
        if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    }
    CHECK(rank_save(&g, directory, saved, sizeof(saved)) == 0 && saved[0] == '\0');
    /* Reuse a gap in the sequence, even if higher-numbered files already exist. */
    snprintf(candidate, sizeof(candidate), "%s\\rank37.txt", directory);
    CHECK(DeleteFileA(candidate));
    CHECK(rank_save(&g, directory, saved, sizeof(saved)) == 1 && strstr(saved, "rank37.txt") != NULL);
    snprintf(candidate, sizeof(candidate), "%s\\rank100.txt", directory);
    CHECK(GetFileAttributesA(candidate) == INVALID_FILE_ATTRIBUTES);
    snprintf(candidate, sizeof(candidate), "%s\\rank00.txt", directory);
    readback = fopen(candidate, "r");
    CHECK(readback != NULL);
    if (readback) {
        char content[2048];
        size_t count = fread(content, 1, sizeof(content) - 1, readback);
        content[count] = '\0'; fclose(readback);
        CHECK(strstr(content, "1. P5 - ESCAPED") != NULL && strstr(content, "6. P0 - LAST REMAINING") != NULL);
    }
    snprintf(candidate, sizeof(candidate), "%s\\rank37.txt", directory);
    CHECK(DeleteFileA(candidate));
    g.draw = 1; g.finished_count = 0; g.turn = 1000;
    CHECK(rank_save(&g, directory, saved, sizeof(saved)) == 1);
    readback = fopen(saved, "r");
    CHECK(readback != NULL);
    if (readback) {
        char content[2048];
        size_t count = fread(content, 1, sizeof(content) - 1, readback);
        content[count] = '\0'; fclose(readback);
        CHECK(strstr(content, "P0 - DRAW (TIED)") && strstr(content, "P5 - DRAW (TIED)"));
        CHECK(strstr(content, "ESCAPED") == NULL && strstr(content, "LAST REMAINING") == NULL);
    }
    for (i = 0; i < 100; ++i) {
        snprintf(candidate, sizeof(candidate), "%s\\rank%02d.txt", directory, i);
        DeleteFileA(candidate);
    }
    RemoveDirectoryA(directory);
    g.game_over = 0;
    CHECK(rank_save(&g, temp, saved, sizeof(saved)) == -1);
}

/* Charges bound consecutive attacks; all non-attack intents recharge exactly one. */
static void test_attack_charge(void)
{
    Game g;
    Command c[PLAYER_COUNT];
    int n;
    CHECK(load_text(&g, valid_map));
    for (n = 0; n < PLAYER_COUNT; ++n) CHECK(g.players[n].attack_charge == MAX_ATTACK_CHARGE);
    arena(&g, c); place(&g, 0, 3, 3, 2);
    for (n = 1; n <= MAX_ATTACK_CHARGE; ++n) {
        game_begin_turn(&g);
        game_resolve(&g, c);
        CHECK(g.players[0].attack_charge == MAX_ATTACK_CHARGE - n);
        CHECK(g.players[0].hp == 2 && !g.cancelled[0]);
    }
    game_begin_turn(&g); game_resolve(&g, c);
    CHECK(g.players[0].attack_charge == 0 && g.players[0].hp == 1 && g.cancelled[0]);
    CHECK(!g.attack[2][3]); /* Empty stock produces no attack fan. */
    game_begin_turn(&g); c[0] = COMMAND_RIGHT; game_resolve(&g, c);
    CHECK(g.players[0].attack_charge == 1 && g.players[0].pos.x == 4);
    game_begin_turn(&g); c[0] = COMMAND_ATTACK_UP; game_resolve(&g, c);
    CHECK(g.players[0].attack_charge == 0 && g.players[0].hp == 1 && !g.cancelled[0]);

    arena(&g, c); place(&g, 0, 3, 3, 2);
    g.players[0].attack_charge = 0; g.key.x = 4; g.key.y = 3;
    c[0] = COMMAND_PICK_RIGHT; game_resolve(&g, c);
    CHECK(g.players[0].attack_charge == 1 && g.key_owner == 0);
    c[0] = COMMAND_DROP_DOWN; game_resolve(&g, c);
    CHECK(g.players[0].attack_charge == 2 && g.key_owner == -1);
    c[0] = COMMAND_LEFT; game_resolve(&g, c); game_resolve(&g, c);
    CHECK(g.players[0].attack_charge == MAX_ATTACK_CHARGE);
    c[0] = (Command)999; game_resolve(&g, c);
    CHECK(g.players[0].attack_charge == MAX_ATTACK_CHARGE && g.players[0].hp == 1);

    arena(&g, c); place(&g, 0, 3, 3, 2);
    g.players[0].attack_charge = 0; g.terrain[3][4] = '#';
    c[0] = COMMAND_RIGHT; game_resolve(&g, c);
    CHECK(g.players[0].attack_charge == 1 && g.cancelled[0] && g.players[0].hp == 1);
    arena(&g, c); place(&g, 0, 3, 3, 2);
    g.key_owner = 0; g.players[0].attack_charge = 2;
    game_resolve(&g, c);
    CHECK(g.players[0].attack_charge == 2 && g.players[0].hp == 1);

    arena(&g, c); g.players[0].attack_charge = 0;
    g.players[0].deaths = 1; g.players[0].rest = 1;
    c[0] = COMMAND_RIGHT; game_resolve(&g, c);
    CHECK(g.players[0].attack_charge == 0);
    game_begin_turn(&g);
    CHECK(g.players[0].hp == 0 && g.players[0].attack_charge == 0);
    game_begin_turn(&g);
    CHECK(g.players[0].hp == MAX_HP && g.players[0].attack_charge == MAX_ATTACK_CHARGE);
    g.players[0].hp = 0; g.players[0].rank = 1; g.players[0].attack_charge = 0;
    game_begin_turn(&g); game_resolve(&g, c);
    CHECK(g.players[0].hp == 0 && g.players[0].attack_charge == 0);
}

/* A temporary external reader/scan may briefly deny file replacement. */
static DWORD WINAPI release_snapshot_lock(LPVOID parameter)
{
    Sleep(60);
    CloseHandle((HANDLE)parameter);
    return 0;
}

/* Preserve complete state on permanent lock, recover after a short lock, and
   prove repeated saves close their tracked FILEs rather than exhausting slots. */
static void test_snapshot_write_failures(void)
{
    Game g;
    Command commands[PLAYER_COUNT];
    Snapshot parsed;
    char directory[MAX_PATH], path[MAX_PATH], bad_path[MAX_PATH];
    HANDLE lock, thread;
    FILE *file;
    int i;
    arena(&g, commands); g.turn = 42;
    if (!GetTempPathA(sizeof(directory), directory) || !GetTempFileNameA(directory, "fsw", 0, path)) { CHECK(0); return; }
    CHECK(snapshot_write(&g, 0, path));
    lock = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    CHECK(lock != INVALID_HANDLE_VALUE);
    if (lock != INVALID_HANDLE_VALUE) {
        g.turn = 43;
        CHECK(!snapshot_write(&g, 0, path));
        CHECK(strstr(snapshot_write_error(), "replace public snapshot") != NULL);
        CHECK(strstr(snapshot_write_error(), "Win32=32") || strstr(snapshot_write_error(), "Win32=5") || strstr(snapshot_write_error(), "Win32=33"));
        CloseHandle(lock);
        file = fopen(path, "r"); CHECK(file != NULL);
        if (file) { CHECK(snapshot_read(file, &parsed) && parsed.turn == 42); fclose(file); }
        lock = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        CHECK(lock != INVALID_HANDLE_VALUE);
        if (lock != INVALID_HANDLE_VALUE) {
            thread = CreateThread(NULL, 0, release_snapshot_lock, lock, 0, NULL);
            CHECK(thread != NULL);
            if (thread) {
                CHECK(snapshot_write(&g, 0, path));
                CHECK(WaitForSingleObject(thread, 2000) == WAIT_OBJECT_0);
                CloseHandle(thread);
            } else CloseHandle(lock);
        }
    }
    for (i = 0; i < 300; ++i) { g.turn = i + 1; CHECK(snapshot_write(&g, 0, path)); }
    file = fopen(path, "r"); CHECK(file != NULL);
    if (file) { CHECK(snapshot_read(file, &parsed) && parsed.turn == 300); fclose(file); }
    snprintf(bad_path, sizeof(bad_path), "%s\\missing\\state.dat", path);
    CHECK(!snapshot_write(&g, 0, bad_path));
    CHECK(strstr(snapshot_write_error(), "open temporary") != NULL);
    DeleteFileA(path);
}

/* Instructor regression entry point; normal gameplay never executes test fixtures. */
int self_test(void)
{
    test_collisions(); test_attacks(); test_items(); test_respawn(); test_files();
    test_carrier_attack_penalty();
    test_attack_charge();
    test_snapshot_write_failures();
    failures += feature_test();
    test_escape_ranking(); test_rank_files(); test_no_escape_draw();
    printf("Self-test: %s (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
