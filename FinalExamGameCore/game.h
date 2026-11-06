#ifndef GAME_H
#define GAME_H
#include "player_api.h"

typedef struct Position { int row, col; } Position;
typedef struct Game {
    int size, round, turn, key_owner, winner;
    char terrain[MAP_MAX][MAP_MAX]; /* '#' wall, ' ' floor, 'E' exit */
    Position players[PLAYER_COUNT], key, exit;
} Game;

int game_load(Game *game, FILE *file, char *error, size_t capacity);
int game_move(Game *game, int player, Command command);
int snapshot_write(const Game *game, int player, const char *path);
void display_init(int plain);
void display_game(const Game *game, const char *message);
Command worker_command(int player, const char *snapshot, const char *log_path);
int worker_main(int player, const char *snapshot, const char *log_path);
int self_test(void);
#endif
