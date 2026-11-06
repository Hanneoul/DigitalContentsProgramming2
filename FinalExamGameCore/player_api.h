#ifndef PLAYER_API_H
#define PLAYER_API_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "diagnostics.h"

#define MAP_MAX 32
#define NPC_COUNT 6
#define PLAYER_COUNT 7
#define HUMAN_PLAYER 6
#define MAX_HP 2
/* Each group of four uses the order up, down, left, right. */
typedef enum Command {
    COMMAND_UP, COMMAND_DOWN, COMMAND_LEFT, COMMAND_RIGHT,
    COMMAND_ATTACK_UP, COMMAND_ATTACK_DOWN, COMMAND_ATTACK_LEFT, COMMAND_ATTACK_RIGHT,
    COMMAND_PICK_UP, COMMAND_PICK_DOWN, COMMAND_PICK_LEFT, COMMAND_PICK_RIGHT,
    COMMAND_DROP_UP, COMMAND_DROP_DOWN, COMMAND_DROP_LEFT, COMMAND_DROP_RIGHT,
    COMMAND_COUNT
} Command;

/* x is the cell column, y is the row; one x occupies TWO console characters. */
typedef struct Position { int x, y; } Position;
typedef struct PlayerState {
    Position pos, spawn;
    int hp;
    unsigned int deaths;
    uint64_t rest;
} PlayerState;

/* Public, file-only information; no pointer to the live core is exposed to AI. */
typedef struct Snapshot {
    int size, turn, self, key_owner;
    Position key, key_start, exit;
    PlayerState players[PLAYER_COUNT];
    char terrain[MAP_MAX][MAP_MAX];
} Snapshot;

/* Parse an already-open state file. The caller retains ownership and must fclose. */
int snapshot_read(FILE *file, Snapshot *state);
#endif
