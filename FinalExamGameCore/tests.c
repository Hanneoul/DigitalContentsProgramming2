#include <windows.h>
#include "game.h"

static int failures;
#define CHECK(test) do { if (!(test)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #test); ++failures; } } while (0)

/* 5 x 5; P6 is deliberately ignored and becomes floor. */
static const char valid_map[] =
    "##########\n"
    "##P0@-()##\n"
    "##P1P2P3##\n"
    "P6P4P5  ##\n"
    "##########\n";

static int load_text(Game *game, const char *text)
{
    char error[256];
    FILE *file = NULL;
    int ok;
    /* tmpfile is deliberately outside the fopen tracker; close via real CRT. */
    if (tmpfile_s(&file) != 0) { CHECK(0); return 0; }
    fputs(text, file);
    rewind(file);
    ok = game_load(game, file, error, sizeof(error));
    (fclose)(file);
    return ok;
}

int self_test(void)
{
    Game game;
    char path[MAX_PATH], directory[MAX_PATH], log_path[MAX_PATH], token[32];
    char changed[sizeof(valid_map)];
    FILE *file;
    int version, n, round, turn, self, owner, kr, kc, er, ec;
    DWORD length = GetTempPathA(sizeof(directory), directory);
    CHECK(length > 0 && length < sizeof(directory));
    if (!length || length >= sizeof(directory)) return 1;
    CHECK(GetTempFileNameA(directory, "fgc", 0, path) != 0);
    CHECK(GetTempFileNameA(directory, "fgl", 0, log_path) != 0);
    CHECK(load_text(&game, valid_map));
    CHECK(game.size == 5 && game.key_owner == -1 && game.winner == -1);
    CHECK(game.terrain[3][0] == ' ');
    CHECK(!game_move(&game, 0, COMMAND_UP));
    CHECK(!game_move(&game, 0, (Command)999));
    CHECK(!game_move(&game, -1, COMMAND_RIGHT));
    game.players[4].row = 3; game.players[4].col = 0;
    CHECK(!game_move(&game, 4, COMMAND_LEFT));
    game.players[3].row = 2; game.players[3].col = 3;
    CHECK(game_move(&game, 3, COMMAND_UP));
    CHECK(game.winner == -1); /* Exit without key stays locked. */
    CHECK(game_move(&game, 0, COMMAND_RIGHT));
    CHECK(game.key_owner == 0);
    CHECK(game_move(&game, 2, COMMAND_UP)); /* Shared cell, no key theft. */
    CHECK(game.key_owner == 0);
    CHECK(game_move(&game, 0, COMMAND_RIGHT));
    CHECK(game.winner == 0);
    CHECK(!game_move(&game, 1, COMMAND_UP));

    CHECK(!load_text(&game, ""));
    CHECK(!load_text(&game, "###\n"));
    CHECK(!load_text(&game, "######\n####\n######\n"));
    strcpy(changed, valid_map);
    changed[0] = '?'; CHECK(!load_text(&game, changed));
    strcpy(changed, valid_map);
    changed[14] = '1'; CHECK(!load_text(&game, changed)); /* Duplicate P1. */
    strcpy(changed, valid_map);
    changed[15] = ' '; changed[16] = ' '; CHECK(!load_text(&game, changed));
    CHECK(!load_text(&game,
        "##########\n##P0@-()##\n##########\nP1P2P3P4P5\n##########\n"));
    CHECK(load_text(&game,
        "##########\r\n##P0@-()##\r\n##P1P2P3##\r\nP6P4P5  ##\r\n##########"));
    CHECK(load_text(&game, valid_map));
    game.round = 3; game.turn = 13;
    CHECK(snapshot_write(&game, 0, path));
    file = fopen(path, "r");
    CHECK(file != NULL);
    if (file) {
        CHECK(fscanf(file, "%31s%d", token, &version) == 2);
        CHECK(!strcmp(token, "FGC_STATE") && version == 1);
        CHECK(fscanf(file, "%d%d%d%d%d%d%d%d%d", &n, &round, &turn, &self, &owner, &kr, &kc, &er, &ec) == 9);
        CHECK(n == 5 && round == 3 && turn == 13 && self == 0 && owner == -1 && kr == 1 && kc == 2 && er == 1 && ec == 3);
        fclose(file);
    }
    CHECK(worker_command(0, path, log_path) == COMMAND_RIGHT);
    CHECK(game_move(&game, 0, COMMAND_RIGHT));
    CHECK(snapshot_write(&game, 0, path));
    CHECK(worker_command(0, path, log_path) == COMMAND_RIGHT);
    CHECK(game_move(&game, 0, COMMAND_RIGHT) && game.winner == 0);
    file = fopen(log_path, "r");
    CHECK(file != NULL);
    if (file) { CHECK(fgetc(file) == EOF); fclose(file); }
    DeleteFileA(path);
    DeleteFileA(log_path);
    printf("Self-test: %s (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
