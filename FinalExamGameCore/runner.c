#include <windows.h>
#include "game.h"

/* Instructor-only declarations: students do not get the dispatcher or other sources. */
Command player0_choose_command(const char *path);
Command player1_choose_command(const char *path);
Command player2_choose_command(const char *path);
Command player3_choose_command(const char *path);
Command player4_choose_command(const char *path);
Command player5_choose_command(const char *path);

int worker_main(int player, const char *snapshot, const char *log_path)
{
    Command (*const choose[PLAYER_COUNT])(const char *) = {
        player0_choose_command, player1_choose_command, player2_choose_command,
        player3_choose_command, player4_choose_command, player5_choose_command
    };
    Command result;
    if (player < 0 || player >= PLAYER_COUNT) return 90;
    if (!freopen(log_path, "a", stderr)) return 90;
    if (!freopen("NUL", "w", stdout)) return 90;
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    diagnostics_begin();
    result = choose[player](snapshot);
    if (diagnostics_end()) fprintf(stderr, "P%d: resource errors above; snapshot=%s\n", player, snapshot);
    if ((int)result < 0 || result > COMMAND_RIGHT) return 90;
    return 20 + (int)result;
}

Command worker_command(int player, const char *snapshot, const char *log_path)
{
    char exe[MAX_PATH], command[MAX_PATH * 4];
    STARTUPINFOA startup = {0};
    PROCESS_INFORMATION process = {0};
    DWORD status = 90, wait_result, length;
    FILE *log;
    startup.cb = sizeof(startup);
    length = GetModuleFileNameA(NULL, exe, sizeof(exe));
    if (!length || length >= sizeof(exe)) return (Command)-1;
    snprintf(command, sizeof(command), "\"%s\" --worker %d \"%s\" \"%s\"", exe, player, snapshot, log_path);
    if (!CreateProcessA(exe, command, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &startup, &process)) {
        fprintf(stderr, "P%d: CreateProcess failed (%lu)\n", player, GetLastError());
        return (Command)-1;
    }
    wait_result = WaitForSingleObject(process.hProcess, 1500);
    if (wait_result == WAIT_OBJECT_0) GetExitCodeProcess(process.hProcess, &status);
    else {
        TerminateProcess(process.hProcess, 91);
        WaitForSingleObject(process.hProcess, 1000);
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    if (status >= 20 && status <= 23) return (Command)(status - 20);
    log = fopen(log_path, "a");
    if (log) {
        fprintf(log, "P%d: %s; exit=%lu; turn skipped. snapshot=%s\n", player,
            wait_result == WAIT_OBJECT_0 ? "invalid command or worker crash" : "AI timeout (1500ms) / wait failure", status, snapshot);
        fclose(log);
    }
    return (Command)-1;
}
