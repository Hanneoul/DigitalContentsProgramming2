#include <inttypes.h>
#include "game.h"
#include <errno.h>

/* Publish the same turn-start state for every player, differing only in self ID. */
int snapshot_write(const Game *g, int player, const char *path)
{
    int y, x, i, ok;
    FILE *file = fopen(path, "w");
    if (!file) return 0;
    fprintf(file, "FGC_STATE 3\n%d %d %d %d %d %d %d %d %d %d\n",
        g->size, g->turn, player, g->key_owner, g->key.x, g->key.y,
        g->key_start.x, g->key_start.y, g->exit.x, g->exit.y);
    for (i = 0; i < PLAYER_COUNT; ++i) {
        const PlayerState *p = &g->players[i];
        fprintf(file, "%d %d %d %d %d %d %u %" PRIu64 " %d\n", i, p->pos.x, p->pos.y,
            p->spawn.x, p->spawn.y, p->hp, p->deaths, p->rest, p->rank);
    }
    for (y = 0; y < g->size; ++y) {
        for (x = 0; x < g->size; ++x) {
            const char *token = "  ";
            if (g->terrain[y][x] == '#') token = "##";
            else if (g->terrain[y][x] == 'E') token = "()";
            else if (g->key_owner < 0 && g->key.x == x && g->key.y == y) token = "@-";
            fputs(token, file);
        }
        fputc('\n', file);
    }
    /* Optional trailing extension keeps the v3 header/map readable by old AIs. */
    fprintf(file, "KEY_STATUS %d\n", game_key_remaining(g));
    fputs("ATTACK_STATUS", file);
    for (i = 0; i < PLAYER_COUNT; ++i) fprintf(file, " %d", g->players[i].attack_charge);
    fputc('\n', file);
    ok = !ferror(file);
    if (fclose(file) != 0) ok = 0;
    return ok;
}

/* Read public v3 ranks; legacy v2 files have every player still racing. */
int snapshot_read(FILE *file, Snapshot *s)
{
    char magic[32], line[MAP_MAX * 2 + 4];
    int version, i, id, x, y;
    if (!file || !s) return 0;
    memset(s, 0, sizeof(*s));
    if (fscanf_s(file, "%31s%d", magic, (unsigned int)sizeof(magic), &version) != 2 || strcmp(magic, "FGC_STATE") || (version != 2 && version != 3)) return 0;
    if (fscanf_s(file, "%d%d%d%d%d%d%d%d%d%d", &s->size, &s->turn, &s->self, &s->key_owner,
        &s->key.x, &s->key.y, &s->key_start.x, &s->key_start.y, &s->exit.x, &s->exit.y) != 10) return 0;
    if (s->size < 3 || s->size > MAP_MAX || s->self < 0 || s->self >= PLAYER_COUNT ||
        s->key_owner < -1 || s->key_owner >= PLAYER_COUNT) return 0;
    for (i = 0; i < PLAYER_COUNT; ++i) {
        PlayerState *p = &s->players[i];
        if (fscanf_s(file, "%d%d%d%d%d%d%u%" SCNu64, &id, &p->pos.x, &p->pos.y,
            &p->spawn.x, &p->spawn.y, &p->hp, &p->deaths, &p->rest) != 8 || id != i) return 0;
        if (version == 3 && (fscanf_s(file, "%d", &p->rank) != 1 || p->rank < 0 || p->rank > PLAYER_COUNT)) return 0;
        if (p->rank && (p->hp != 0 || p->rest != 0)) return 0;
        p->attack_charge = MAX_ATTACK_CHARGE; /* Legacy snapshots predate the limit. */
        if (p->pos.x < 0 || p->pos.y < 0 || p->pos.x >= s->size || p->pos.y >= s->size || p->hp < 0 || p->hp > MAX_HP) return 0;
    }
    if (!fgets(line, sizeof(line), file)) return 0; /* Numeric record's newline. */
    for (y = 0; y < s->size; ++y) {
        if (!fgets(line, sizeof(line), file) || strcspn(line, "\r\n") != (size_t)s->size * 2) return 0;
        for (x = 0; x < s->size; ++x) {
            char a = line[x * 2], b = line[x * 2 + 1];
            if (a == '#' && b == '#') s->terrain[y][x] = '#';
            else if (a == '(' && b == ')') s->terrain[y][x] = 'E';
            else if ((a == ' ' && b == ' ') || (a == '@' && b == '-')) s->terrain[y][x] = ' ';
            else return 0;
        }
    }
    s->key_respawn_remaining = s->key_owner >= 0 ? 0 : KEY_RESPAWN_TURNS;
    /* Legacy v2/v3 files end after the map; new ones expose the reset countdown. */
    while (fgets(line, sizeof(line), file)) {
        if (!strncmp(line, "KEY_STATUS ", 11)) {
            int remaining;
            if (sscanf_s(line, "KEY_STATUS %d", &remaining) != 1 || remaining < 0 || remaining > KEY_RESPAWN_TURNS ||
                (s->key_owner >= 0 ? remaining != 0 : remaining == 0)) return 0;
            s->key_respawn_remaining = remaining;
        } else if (!strncmp(line, "ATTACK_STATUS ", 14)) {
            const char *cursor = line + 14;
            for (i = 0; i < PLAYER_COUNT; ++i) {
                char *end;
                long charge;
                errno = 0; charge = strtol(cursor, &end, 10);
                if (errno || end == cursor || charge < 0 || charge > MAX_ATTACK_CHARGE) return 0;
                s->players[i].attack_charge = (int)charge;
                cursor = end;
            }
            if (cursor[strspn(cursor, " \t\r\n")]) return 0;
        } else return 0;
    }
    return !ferror(file);
}
