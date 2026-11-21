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

/* Wait only for the unspent frame budget; slow AI must not add a fixed sleep. */
unsigned int frame_wait_ms(uint64_t previous_output, uint64_t now, unsigned int interval)
{
    uint64_t elapsed = now - previous_output;
    return elapsed < interval ? interval - (unsigned int)elapsed : 0;
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
    ULONGLONG previous_output;
    int i, max_turns = 0, delay = 500, plain = 0, automatic = 0, result = 0;
    if ((argc == 5 || argc == 6) && strcmp(argv[1], "--worker") == 0) {
        int player;
        if (!number(argv[2], 0, PLAYER_COUNT - 1, &player)) return 90;
        return worker_main(player, argv[3], argv[4], argc == 6 ? argv[5] : NULL);
    }
    diagnostics_begin();
    if (argc == 2 && strcmp(argv[1], "--feature-test") == 0) {
        result = feature_test();
        return diagnostics_end() ? 1 : result;
    }
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
            fputs("Usage: FinalExam01 [--map path] [--auto] [--fast] [--plain] [--max-turns 1..10000] [--self-test | --feature-test]\n", stderr);
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
    if (!automatic) {
        display_game(&game, "Press 's' Key", 0);
        input_init();
        if (!input_wait_start()) { puts("START CANCELLED: EOF"); return diagnostics_end() ? 1 : 0; }
    }
    result = 0;
    previous_output = GetTickCount64();
    display_game(&game, "게임 시작 | 기본 화면 갱신 간격 500ms", 0);
    while (!game.game_over && (!max_turns || game.turn < max_turns)) {
        Command commands[PLAYER_COUNT];
        const char *message;
        unsigned int wait;
        int attacks = 0;
        game_begin_turn(&game);
        /* Write EVERY snapshot before executing ANY submitted code or updating state. */
        for (i = 0; i < PLAYER_COUNT; ++i) {
            commands[i] = (Command)-1;
            snprintf(snapshots[i], sizeof(snapshots[i]), "%s\\player%d_state.dat", run_dir, i);
            if (!snapshot_write(&game, i, snapshots[i])) { fputs("Snapshot write failed.\n", stderr); result = 1; goto done; }
        }
        for (i = 0; i < PLAYER_COUNT; ++i) {
            if (game.players[i].hp <= 0 || game.players[i].rank) continue;
            snprintf(log_path, sizeof(log_path), "%s\\player%d_diagnostics.log", run_dir, i);
            commands[i] = worker_command(i, snapshots[i], log_path);
        }
        game_resolve(&game, commands);
        for (i = 0; i < PLAYER_COUNT; ++i)
            if (game.last[i] >= COMMAND_ATTACK_UP && game.last[i] <= COMMAND_ATTACK_RIGHT && !game.cancelled[i] &&
                (game.players[i].hp > 0 || game.damaged[i])) attacks = 1;
        message = game.draw ? "GAME OVER - 1000턴 탈출자 없음: 전원 무승부" :
            game.game_over ? "GAME OVER - 최종 순위 확정" :
            max_turns && game.turn >= max_turns ? "턴 제한으로 중단 - 순위 미확정" :
            attacks ? "동시 턴 완료: 공격 범위 ** / 피격 빨간 배경" : "동시 턴 판정 완료";
        /* Previous rendering, snapshots and ALL AI work consume the same budget.
           Sleep BEFORE printf, then anchor the next frame to this actual output. */
        wait = frame_wait_ms(previous_output, GetTickCount64(), (unsigned int)delay);
        if (wait) Sleep((DWORD)wait);
        previous_output = GetTickCount64();
        display_game(&game, message, attacks);
    }
    if (game.game_over) {
        char rank_path[MAX_PATH];
        int saved;
        puts("GAME OVER");
        if (game.draw) {
            puts("DRAW: 1000턴까지 탈출자가 없어 남은 전원 공동 무승부입니다.");
            for (i = 0; i < PLAYER_COUNT; ++i) if (!game.players[i].rank) printf("DRAW: P%d\n", i);
        }
        for (i = 0; i < game.finished_count; ++i)
            printf("RANK %d: P%d%s\n", i + 1, game.finish_order[i], i == PLAYER_COUNT - 1 ? " (LAST)" : " (ESCAPED)");
        saved = rank_save(&game, base, rank_path, sizeof(rank_path));
        if (saved == 1) printf("순위 저장: %s\n", rank_path);
        else if (saved == 0) puts("파일 부족: rank00.txt~rank99.txt가 모두 존재합니다. 저장하지 않습니다.");
        else { fputs("순위 파일 저장 오류\n", stderr); result = 1; }
    } else puts("STOPPED: 최대 턴에 도달했습니다. 미확정 순위는 저장하지 않습니다.");
done:
    printf("Snapshots / diagnostics: %s\n", run_dir);
    if (diagnostics_end()) result = 1;
    return result;
}
