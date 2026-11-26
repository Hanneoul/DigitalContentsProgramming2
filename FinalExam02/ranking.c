#include <windows.h>
#include "game.h"

/* CREATE_NEW reserves a free slot atomically: existing rankings are never overwritten. */
int rank_save(const Game *g, const char *directory, char *path, size_t capacity)
{
    char text[2048], candidate[MAX_PATH];
    int n, i, length, written;
    DWORD bytes;
    HANDLE file;
    if (!g->game_over || (!g->draw && g->finished_count != PLAYER_COUNT) || !capacity) return -1;
    path[0] = '\0';
    length = snprintf(text, sizeof(text), "FinalExam02 - FINAL RANKING\r\nTurns: %d\r\n\r\n", g->turn);
    if (g->draw) length += snprintf(text + length, sizeof(text) - (size_t)length, "DRAW - no escape within 1000 turns\r\n");
    for (i = 0; i < PLAYER_COUNT; ++i) {
        if (g->draw) {
            length += snprintf(text + length, sizeof(text) - (size_t)length, "P%d - DRAW (TIED)\r\n", i);
            continue;
        }
        written = snprintf(text + length, sizeof(text) - (size_t)length,
            "%d. P%d - %s", i + 1, g->finish_order[i], i == PLAYER_COUNT - 1 ? "LAST REMAINING\r\n" : "ESCAPED");
        if (written < 0 || (size_t)written >= sizeof(text) - (size_t)length) return -1;
        length += written;
        if (i < PLAYER_COUNT - 1) length += snprintf(text + length, sizeof(text) - (size_t)length, " (turn %d)\r\n", g->finish_turn[i]);
    }
    for (n = 0; n < 100; ++n) {
        written = snprintf(candidate, sizeof(candidate), "%s\\rank%02d.txt", directory, n);
        if (written < 0 || written >= (int)sizeof(candidate) || (size_t)written >= capacity) return -1;
        file = CreateFileA(candidate, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file == INVALID_HANDLE_VALUE) {
            DWORD error = GetLastError();
            if (error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS) continue;
            return -1;
        }
        written = WriteFile(file, text, (DWORD)length, &bytes, NULL) && bytes == (DWORD)length;
        if (!CloseHandle(file)) written = 0;
        if (!written) { DeleteFileA(candidate); return -1; }
        strcpy_s(path, capacity, candidate);
        return 1;
    }
    return 0;
}
