#ifndef GAME_H
#define GAME_H
#include "player_api.h"

/* Only the core owns mutable game state and this turn's visual effects. */
typedef struct Game {
    int size, turn, key_owner, winner;
    char terrain[MAP_MAX][MAP_MAX];
    PlayerState players[PLAYER_COUNT];
    Position key, key_start, exit;
    unsigned char attack[MAP_MAX][MAP_MAX];
    int damaged[PLAYER_COUNT], cancelled[PLAYER_COUNT];
    Command last[PLAYER_COUNT];
} Game;

int game_load(Game *game, FILE *file, char *error, size_t capacity);
void game_begin_turn(Game *game);
void game_resolve(Game *game, const Command commands[PLAYER_COUNT]);
int snapshot_write(const Game *game, int player, const char *path);
void display_init(int plain);
void display_game(const Game *game, const char *message, int effects);
void input_init(void);
int input_decode(int ch, int has_key, Command *command);
int input_command(int has_key, Command *command);
Command worker_command(int player, const char *snapshot, const char *log_path);
int worker_main(int player, const char *snapshot, const char *log_path, const char *ready_event);
int self_test(void);
#endif
