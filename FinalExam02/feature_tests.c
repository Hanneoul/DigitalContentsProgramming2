#include <windows.h>
#include "game.h"

static int failures;
#define VERIFY(condition) do { if (!(condition)) { fprintf(stderr, "Feature FAIL %d: %s\n", __LINE__, #condition); ++failures; } } while (0)

/* Empty arena isolates timer semantics from the students' chosen strategies. */
static void setup(Game *g, Command commands[PLAYER_COUNT])
{
    int i;
    memset(g, 0, sizeof(*g));
    g->size = 7; g->key_owner = g->winner = g->last_escape = -1;
    g->key_start.x = 5; g->key_start.y = 5;
    g->key.x = 2; g->key.y = 1;
    g->exit.x = 6; g->exit.y = 6;
    memset(g->terrain, ' ', sizeof(g->terrain));
    g->terrain[6][6] = 'E';
    for (i = 0; i < PLAYER_COUNT; ++i) {
        commands[i] = COMMAND_ATTACK_UP;
        g->players[i].attack_charge = MAX_ATTACK_CHARGE;
    }
}

/* Optional state extension must round-trip and old map-only snapshots still read. */
static void test_snapshot(Game *g)
{
    FILE *file = NULL;
    Snapshot s;
    char path[MAX_PATH], directory[MAX_PATH], line[128];
    int i;
    if (!GetTempPathA(sizeof(directory), directory) || !GetTempFileNameA(directory, "fkt", 0, path)) { VERIFY(0); return; }
    g->key_idle_turns = 17;
    for (i = 0; i < PLAYER_COUNT; ++i) g->players[i].attack_charge = i % (MAX_ATTACK_CHARGE + 1);
    VERIFY(snapshot_write(g, 0, path));
    file = fopen(path, "r");
    VERIFY(file != NULL);
    if (file) {
        VERIFY(snapshot_read(file, &s) && s.key_respawn_remaining == 13);
        for (i = 0; i < PLAYER_COUNT; ++i) VERIFY(s.players[i].attack_charge == i % (MAX_ATTACK_CHARGE + 1));
        fclose(file);
    }
    /* Remove the optional last record by copying the stable header/map prefix. */
    file = fopen(path, "r");
    if (file) {
        FILE *legacy = NULL;
        if (!tmpfile_s(&legacy)) {
            for (i = 0; i < 2 + PLAYER_COUNT + g->size && fgets(line, sizeof(line), file); ++i) fputs(line, legacy);
            rewind(legacy);
            VERIFY(snapshot_read(legacy, &s) && s.key_respawn_remaining == KEY_RESPAWN_TURNS);
            for (i = 0; i < PLAYER_COUNT; ++i) VERIFY(s.players[i].attack_charge == MAX_ATTACK_CHARGE);
            (fclose)(legacy);
        } else VERIFY(0);
        fclose(file);
    }
    /* Reject out-of-range, negative, truncated and overflowing charge records. */
    {
        const char *bad[] = {
            "ATTACK_STATUS 4 0 0 0 0 0\n", "ATTACK_STATUS -1 0 0 0 0 0\n",
            "ATTACK_STATUS 1 2\n", "ATTACK_STATUS 9999999999999999999999 0 0 0 0 0\n"
        };
        for (i = 0; i < (int)(sizeof(bad) / sizeof(bad[0])); ++i) {
            VERIFY(snapshot_write(g, 0, path));
            file = fopen(path, "a");
            VERIFY(file != NULL);
            if (file) { fputs(bad[i], file); fclose(file); }
            file = fopen(path, "r");
            VERIFY(file != NULL);
            if (file) { VERIFY(!snapshot_read(file, &s)); fclose(file); }
        }
    }
    remove(path);
}

/* Boundary coverage: full 30 turns, pickup on deadline, drops and carrier death. */
int feature_test(void)
{
    Game g;
    Command c[PLAYER_COUNT];
    char name[PLAYER_NAME_BYTES];
    int i;
    failures = 0;
    /* 100ms work => 400ms sleep, deadline/overrun => no sleep; --fast bypasses it. */
    VERIFY(frame_wait_ms(1000, 1100, 500) == 400);
    VERIFY(frame_wait_ms(1000, 1499, 500) == 1);
    VERIFY(frame_wait_ms(1000, 1500, 500) == 0);
    VERIFY(frame_wait_ms(1000, 1800, 500) == 0);
    VERIFY(frame_wait_ms(1800, 1900, 500) == 400);
    VERIFY(frame_wait_ms(1000, 1000, 0) == 0);
    VERIFY(!strcmp(display_player_style(0, 2, 0, 0), "\x1b[91m"));
    VERIFY(!strcmp(display_player_style(0, 1, 0, 0), "\x1b[31m"));
    VERIFY(!strcmp(display_player_style(0, 1, 1, 0), "\x1b[43;30m"));
    VERIFY(!strcmp(display_player_style(0, 2, 1, 0), "\x1b[103;30;1m"));
    for (i = 0; i < PLAYER_COUNT; ++i)
        VERIFY(strcmp(display_player_style(i, 1, 0, 0), display_player_style(i, 2, 0, 0)) != 0);
    setup(&g, c);
    for (i = 0; i < 29; ++i) game_resolve(&g, c);
    VERIFY(game_key_remaining(&g) == 1 && g.key.x == 2);
    game_resolve(&g, c);
    VERIFY(g.key.x == 5 && g.key.y == 5 && game_key_remaining(&g) == 30);
    setup(&g, c); g.key_idle_turns = 29;
    g.players[0].hp = 2; g.players[0].pos.x = 1; g.players[0].pos.y = 1;
    c[0] = COMMAND_PICK_RIGHT; game_resolve(&g, c);
    VERIFY(g.key_owner == 0 && game_key_remaining(&g) == 0);
    c[0] = COMMAND_RIGHT; game_resolve(&g, c);
    VERIFY(g.key_idle_turns == 0);
    c[0] = COMMAND_DROP_DOWN; game_resolve(&g, c);
    VERIFY(g.key_owner == -1 && game_key_remaining(&g) == 30 && g.key.y == 2);
    c[0] = COMMAND_ATTACK_UP; game_resolve(&g, c);
    VERIFY(game_key_remaining(&g) == 29);
    setup(&g, c); g.key_idle_turns = 29;
    g.players[0].hp = 1; g.players[0].pos.x = 1; g.players[0].pos.y = 1;
    g.players[1].hp = 2; g.players[1].pos.x = 1; g.players[1].pos.y = 2;
    c[0] = COMMAND_PICK_RIGHT; game_resolve(&g, c);
    VERIFY(g.players[0].hp == 0 && g.key_owner == -1 && g.key.x == 1 && game_key_remaining(&g) == 30);
    setup(&g, c); g.key_owner = 0; g.key_idle_turns = 20;
    g.players[0].hp = 1; g.players[0].pos.x = 5; g.players[0].pos.y = 6;
    g.players[1].hp = 2; g.players[1].pos.x = 6; g.players[1].pos.y = 5;
    c[0] = COMMAND_RIGHT; c[1] = COMMAND_ATTACK_DOWN; game_resolve(&g, c);
    VERIFY(g.key_owner == -1 && g.key.x == 5 && g.key.y == 5 && game_key_remaining(&g) == 30);
    setup(&g, c); test_snapshot(&g);
    VERIFY(display_gauge_fill(0, 8, 13) == 0 && display_gauge_fill(8, 8, 13) == 13);
    VERIFY(display_gauge_fill(4, 8, 13) == 7 && display_gauge_fill(1, 8, 13) == 2);
    VERIFY(display_gauge_fill(UINT64_MAX - 1, UINT64_MAX, 13) == 13);
    VERIFY(display_gauge_fill(1, UINT64_MAX, 13) == 1);
    VERIFY(display_name(name, sizeof(name), u8"홍길동", 16) == 16 && strlen(name) == 19);
    VERIFY(display_name(name, sizeof(name), "abcdefghijklmnopq", 16) == 16 && !strcmp(name, "abcdefghijklmnop"));
    VERIFY(display_name(name, sizeof(name), "A\tB\n", 16) == 16 && !strncmp(name, "AB", 2));
    VERIFY(display_sidebar_fits(13, 80) && !display_sidebar_fits(17, 80) && display_sidebar_fits(17, 120));
    printf("Feature tests: %s (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
