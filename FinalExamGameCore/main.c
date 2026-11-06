#include <windows.h>
#include <errno.h>
#include "game.h"

/* Parse bounded CLI integers without accepting trailing junk or overflow. */
static int number(const char *text, int minimum, int maximum, int *result)
{
    char *end;
    long value;
    errno = 0;
    value = strtol(text, &end, 10);
    if (errno || !*text || *end || value < minimum || value > maximum) return 0;
    *result = (int)value;
    return 1;
}

/* The parent owns the game. Worker mode executes only one submitted AI function. */
int main(int argc, char **argv)
{
    Game game;
    FILE *map;
    char base[MAX_PATH], default_map[MAX_PATH], run_dir[MAX_PATH], error[256];
    char snapshots[PLAYER_COUNT][MAX_PATH], log_path[MAX_PATH];
    const char *map_path;
    char *slash;
    DWORD length;
    int i, max_turns = 200, delay = 2000, plain = 0, automatic = 0, result = 0, quit = 0;
    if ((argc == 5 || argc == 6) && strcmp(argv[1], "--worker") == 0) {
        int player;
        if (!number(argv[2], 0, PLAYER_COUNT - 1, &player)) return 90;
        return worker_main(player, argv[3], argv[4], argc == 6 ? argv[5] : NULL);
    }
    diagnostics_begin();
    if (argc == 2 && strcmp(argv[1], "--self-test") == 0) {
        result = self_test();
        return diagnostics_end() ? 1 : result;
    }
    length = GetModuleFileNameA(NULL, base, sizeof(base));
    if (!length || length >= sizeof(base) - 80) { fputs("Executable path too long.\n", stderr); return 1; }
    slash = strrchr(base, '\\');
    if (!slash) return 1;
    *slash = '\0';
    snprintf(default_map, sizeof(default_map), "%s\\dungeonmap.dat", base);
    map_path = default_map;
    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--fast") == 0) delay = 0;
        else if (strcmp(argv[i], "--plain") == 0) plain = 1;
        else if (strcmp(argv[i], "--auto") == 0) automatic = 1;
        else if (strcmp(argv[i], "--map") == 0 && i + 1 < argc) map_path = argv[++i];
        else if ((!strcmp(argv[i], "--max-turns") || !strcmp(argv[i], "--max-rounds")) && i + 1 < argc && number(argv[i + 1], 1, 10000, &max_turns)) ++i;
        else {
            fputs("Usage: FinalExamGameCore [--map path] [--auto] [--fast] [--plain] [--max-turns 1..10000] [--self-test]\n", stderr);
            return 1;
        }
    }
    map = fopen(map_path, "r");
    if (!map) { fprintf(stderr, "Cannot open map: %s\n", map_path); return 1; }
    result = game_load(&game, map, error, sizeof(error));
    fclose(map);
    if (!result) { fprintf(stderr, "Invalid map: %s\n", error); return 1; }
    snprintf(run_dir, sizeof(run_dir), "%s\\run-%lu-%llu", base, GetCurrentProcessId(), GetTickCount64());
    if (!CreateDirectoryA(run_dir, NULL)) { fputs("Cannot create snapshot directory.\n", stderr); return 1; }
    display_init(plain);
    if (!automatic) input_init();
    result = 0;
    while (game.turn < max_turns && game.winner < 0) {
        Command commands[PLAYER_COUNT];
        int attacks = 0;
        game_begin_turn(&game);
        display_game(&game, "같은 상태에서 모든 명령을 수집한 뒤 동시에 처리합니다.", 0);
        /* Write EVERY snapshot before executing ANY submitted code or updating state. */
        for (i = 0; i < PLAYER_COUNT; ++i) {
            commands[i] = (Command)-1;
            snprintf(snapshots[i], sizeof(snapshots[i]), "%s\\player%d_state.dat", run_dir, i);
            if (!snapshot_write(&game, i, snapshots[i])) { fputs("Snapshot write failed.\n", stderr); result = 1; goto done; }
        }
        /* Even while H0 is dead, wait for input to permit stepping turns and ESC. */
        if (!automatic && !input_command(game.key_owner == HUMAN_PLAYER, &commands[HUMAN_PLAYER])) { quit = 1; break; }
        for (i = 0; i < PLAYER_COUNT; ++i) {
            if (game.players[i].hp <= 0 || (!automatic && i == HUMAN_PLAYER)) continue;
            snprintf(log_path, sizeof(log_path), "%s\\player%d_diagnostics.log", run_dir, i);
            commands[i] = worker_command(i, snapshots[i], log_path);
        }
        game_resolve(&game, commands);
        for (i = 0; i < PLAYER_COUNT; ++i)
            if (game.last[i] >= COMMAND_ATTACK_UP && game.last[i] <= COMMAND_ATTACK_RIGHT && !game.cancelled[i] &&
                (game.players[i].hp > 0 || game.damaged[i])) attacks = 1;
        /* A normal turn is shown for two seconds: attack overlay for the first one. */
        if (attacks) {
            display_game(&game, "공격 범위 ** / 노란 배경, 피격 빨간 배경 / 사망 !!", 1);
            if (delay) Sleep(1000);
        }
        display_game(&game, "동시 턴 판정 완료", 0);
        if (delay) Sleep((DWORD)(attacks ? delay - 1000 : delay));
    }
    if (quit) puts("QUIT: 사용자 입력으로 종료했습니다.");
    else if (game.winner >= 0) printf("WINNER: %c%d\n", game.winner == HUMAN_PLAYER ? 'H' : 'P', game.winner == HUMAN_PLAYER ? 0 : game.winner);
    else puts("DRAW: 최대 턴에 도달했습니다.");
done:
    printf("Snapshots / diagnostics: %s\n", run_dir);
    if (diagnostics_end()) result = 1;
    return result;
}
