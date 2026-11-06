#include "player_api.h"

/* Edit only this function. All working state is reconstructed from the snapshot.
   Starter strategy: breadth-first search to the key, then to the exit. */
Command player4_choose_command(const char *state_path)
{
    FILE *file = fopen(state_path, "r");
    char magic[32], line[MAP_MAX * 2 + 4];
    char walls[MAP_MAX][MAP_MAX];
    int version, n, round, turn, self, owner, kr, kc, er, ec;
    int rows[PLAYER_COUNT], cols[PLAYER_COUNT];
    int distance[MAP_MAX][MAP_MAX], first[MAP_MAX][MAP_MAX];
    int qr[MAP_MAX * MAP_MAX], qc[MAP_MAX * MAP_MAX];
    const int dr[4] = {-1, 1, 0, 0}, dc[4] = {0, 0, -1, 1};
    int r, c, i, id, target_r, target_c, head = 0, tail = 0;
    if (!file) return COMMAND_UP;
    if (fscanf(file, "%31s %d", magic, &version) != 2 || strcmp(magic, "FGC_STATE") || version != 1 ||
        fscanf(file, "%d%d%d%d%d%d%d%d%d", &n, &round, &turn, &self, &owner, &kr, &kc, &er, &ec) != 9 ||
        n < 3 || n > MAP_MAX || self < 0 || self >= PLAYER_COUNT) {
        fclose(file);
        return COMMAND_UP;
    }
    for (i = 0; i < PLAYER_COUNT; ++i) {
        if (fscanf(file, "%d%d%d", &id, &rows[i], &cols[i]) != 3 || id != i ||
            rows[i] < 0 || rows[i] >= n || cols[i] < 0 || cols[i] >= n) {
            fclose(file);
            return COMMAND_UP;
        }
    }
    /* Consume the newline after the last numeric record. */
    if (!fgets(line, sizeof(line), file)) { fclose(file); return COMMAND_UP; }
    for (r = 0; r < n; ++r) {
        if (!fgets(line, sizeof(line), file) || strlen(line) < (size_t)n * 2) {
            fclose(file);
            return COMMAND_UP;
        }
        for (c = 0; c < n; ++c) {
            walls[r][c] = line[c * 2];
            distance[r][c] = -1;
            first[r][c] = -1;
        }
    }
    fclose(file);
    target_r = owner < 0 ? kr : er;
    target_c = owner < 0 ? kc : ec;
    r = rows[self]; c = cols[self];
    qr[tail] = r; qc[tail++] = c;
    distance[r][c] = 0;
    while (head < tail) {
        r = qr[head]; c = qc[head++];
        if (r == target_r && c == target_c && first[r][c] >= 0)
            return (Command)first[r][c];
        for (i = 0; i < 4; ++i) {
            int nr = r + dr[i], nc = c + dc[i];
            if (nr < 0 || nc < 0 || nr >= n || nc >= n || walls[nr][nc] == '#' || distance[nr][nc] >= 0) continue;
            distance[nr][nc] = distance[r][c] + 1;
            first[nr][nc] = first[r][c] < 0 ? i : first[r][c];
            qr[tail] = nr; qc[tail++] = nc;
        }
    }
    return COMMAND_UP;
}

