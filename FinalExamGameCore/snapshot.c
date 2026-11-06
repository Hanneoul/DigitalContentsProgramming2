#include "game.h"

int snapshot_write(const Game *game, int player, const char *path)
{
    int row, col, i, ok;
    FILE *file = fopen(path, "w");
    if (!file) return 0;
    fprintf(file, "FGC_STATE 1\n%d %d %d %d %d %d %d %d %d\n",
        game->size, game->round, game->turn, player, game->key_owner,
        game->key.row, game->key.col, game->exit.row, game->exit.col);
    for (i = 0; i < PLAYER_COUNT; ++i)
        fprintf(file, "%d %d %d\n", i, game->players[i].row, game->players[i].col);
    for (row = 0; row < game->size; ++row) {
        for (col = 0; col < game->size; ++col) {
            const char *token = "  ";
            if (game->terrain[row][col] == '#') token = "##";
            else if (game->terrain[row][col] == 'E') token = "()";
            else if (game->key_owner < 0 && game->key.row == row && game->key.col == col) token = "@-";
            fputs(token, file);
        }
        fputc('\n', file);
    }
    ok = !ferror(file);
    if (fclose(file) != 0) ok = 0;
    return ok;
}
