#ifndef PLAYER_API_H
#define PLAYER_API_H

/* Only this header and your own playerN.c belong in a student's handout. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "diagnostics.h"

#define MAP_MAX 32
#define PLAYER_COUNT 6
typedef enum Command {
    COMMAND_UP = 0,
    COMMAND_DOWN = 1,
    COMMAND_LEFT = 2,
    COMMAND_RIGHT = 3
} Command;
#endif
