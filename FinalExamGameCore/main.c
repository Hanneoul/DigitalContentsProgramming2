#include <windows.h>
#include <errno.h>
#include "game.h"

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

int main(int argc, char **argv)
{
    Game game;
    FILE *map;
    char base[MAX_PATH], default_map[MAX_PATH], run_dir[MAX_PATH], error[256], message[256];
    char snapshot[MAX_PATH], log_path[MAX_PATH];
    const char *map_path;
    char *slash;
    DWORD length;
    int i, rounds = 200, delay = 2000, plain = 0, result = 0;
    if (argc == 5 && strcmp(argv[1], "--worker") == 0) {
        int player;
        if (!number(argv[2], 0, PLAYER_COUNT - 1, &player)) return 90;
        return worker_main(player, argv[3], argv[4]);
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
        else if (strcmp(argv[i], "--map") == 0 && i + 1 < argc) map_path = argv[++i];
        else if (strcmp(argv[i], "--max-rounds") == 0 && i + 1 < argc && number(argv[i + 1], 1, 10000, &rounds)) ++i;
        else {
            fputs("Usage: FinalExamGameCore [--map path] [--fast] [--plain] [--max-rounds 1..10000] [--self-test]\n", stderr);
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
    display_game(&game, "게임 시작: 각 NPC 행동 사이에 2초 간격 (--fast로 생략)");
    result = 0;
    for (game.round = 1; game.round <= rounds && game.winner < 0; ++game.round) {
        for (i = 0; i < PLAYER_COUNT && game.winner < 0; ++i) {
            int player = (game.round - 1 + i) % PLAYER_COUNT;
            Command command;
            ++game.turn;
            snprintf(snapshot, sizeof(snapshot), "%s\\player%d_state.dat", run_dir, player);
            snprintf(log_path, sizeof(log_path), "%s\\player%d_diagnostics.log", run_dir, player);
            if (!snapshot_write(&game, player, snapshot)) {
                fprintf(stderr, "Snapshot write failed: %s\n", snapshot);
                result = 1;
                goto done;
            }
            if (delay) Sleep((DWORD)delay);
            command = worker_command(player, snapshot, log_path);
            snprintf(message, sizeof(message), "P%d command=%d : %s", player, (int)command,
                game_move(&game, player, command) ? "이동" : "이동 불가 / 무효 명령 (로그 확인)");
            display_game(&game, message);
        }
    }
    if (game.winner >= 0) printf("WINNER: P%d\n", game.winner);
    else puts("DRAW: 최대 라운드에 도달했습니다.");
done:
    printf("Snapshots / diagnostics: %s\n", run_dir);
    if (diagnostics_end()) result = 1;
    return result;
}
