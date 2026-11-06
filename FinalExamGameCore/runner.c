#include <windows.h>
#include "game.h"

/* Instructor-only declarations: students do not get the dispatcher or other sources. */
Command player0_choose_command(const char *path);
Command player1_choose_command(const char *path);
Command player2_choose_command(const char *path);
Command player3_choose_command(const char *path);
Command player4_choose_command(const char *path);
Command player5_choose_command(const char *path);

/* Execute one AI in a disposable child; report resources before returning its command. */
int worker_main(int player, const char *snapshot, const char *log_path, const char *ready_event)
{
    Command (*const choose[PLAYER_COUNT])(const char *) = {
        player0_choose_command, player1_choose_command, player2_choose_command,
        player3_choose_command, player4_choose_command, player5_choose_command, player0_choose_command
    };
    Command result;
    if (player < 0 || player >= PLAYER_COUNT) return 90;
    if (!freopen(log_path, "a", stderr)) return 90;
    if (!freopen("NUL", "w", stdout)) return 90;
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    diagnostics_begin();
    /* Exclude OS/CRT loading time from the student's computation budget. */
    if (ready_event) {
        HANDLE ready = OpenEventA(EVENT_MODIFY_STATE, FALSE, ready_event);
        if (!ready) return 90;
        SetEvent(ready);
        CloseHandle(ready);
    }
    result = choose[player](snapshot);
    if (diagnostics_end()) fprintf(stderr, "P%d: resource errors above; snapshot=%s\n", player, snapshot);
    if ((int)result < 0 || result >= COMMAND_COUNT) return 90;
    return 20 + (int)result;
}

/* Allow up to 10 seconds for process startup, then 1.5 seconds for AI execution. */
Command worker_command(int player, const char *snapshot, const char *log_path)
{
    char exe[MAX_PATH], command[MAX_PATH * 4], event_name[128];
    STARTUPINFOA startup = {0};
    PROCESS_INFORMATION process = {0};
    DWORD status = 90, wait_result, length;
    HANDLE ready, handles[2];
    int started = 0;
    FILE *log;
    startup.cb = sizeof(startup);
    length = GetModuleFileNameA(NULL, exe, sizeof(exe));
    if (!length || length >= sizeof(exe)) return (Command)-1;
    snprintf(event_name, sizeof(event_name), "Local\\FGC-%lu-%llu-%d", GetCurrentProcessId(), GetTickCount64(), player);
    ready = CreateEventA(NULL, TRUE, FALSE, event_name);
    if (!ready) return (Command)-1;
    snprintf(command, sizeof(command), "\"%s\" --worker %d \"%s\" \"%s\" \"%s\"", exe, player, snapshot, log_path, event_name);
    if (!CreateProcessA(exe, command, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &startup, &process)) {
        fprintf(stderr, "P%d: CreateProcess failed (%lu)\n", player, GetLastError());
        CloseHandle(ready);
        return (Command)-1;
    }
    handles[0] = process.hProcess; handles[1] = ready;
    wait_result = WaitForMultipleObjects(2, handles, FALSE, 10000);
    if (wait_result == WAIT_OBJECT_0 + 1) {
        started = 1;
        wait_result = WaitForSingleObject(process.hProcess, 1500);
    }
    if (wait_result == WAIT_OBJECT_0) GetExitCodeProcess(process.hProcess, &status);
    else {
        TerminateProcess(process.hProcess, 91);
        WaitForSingleObject(process.hProcess, 1000);
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    CloseHandle(ready);
    if (status >= 20 && status < 20 + COMMAND_COUNT) return (Command)(status - 20);
    log = fopen(log_path, "a");
    if (log) {
        fprintf(log, "P%d: %s; exit=%lu; command cancelled (1 damage). snapshot=%s\n", player,
            wait_result == WAIT_OBJECT_0 ? "invalid command or worker crash" :
            started ? "AI timeout (1500ms) / wait failure" : "worker startup timeout (10000ms) / wait failure", status, snapshot);
        fclose(log);
    }
    return (Command)-1;
}
