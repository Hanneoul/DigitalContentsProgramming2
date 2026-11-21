#include "game.h"

extern char player0_name[PLAYER_NAME_BYTES];
extern char player1_name[PLAYER_NAME_BYTES];
extern char player2_name[PLAYER_NAME_BYTES];
extern char player3_name[PLAYER_NAME_BYTES];
extern char player4_name[PLAYER_NAME_BYTES];
extern char player5_name[PLAYER_NAME_BYTES];

/* The renderer reads the student's variable without calling their decision function. */
const char *player_name(int player)
{
    static const char *const names[PLAYER_COUNT] = {
        player0_name, player1_name, player2_name, player3_name, player4_name, player5_name
    };
    return player >= 0 && player < PLAYER_COUNT && names[player][0] ? names[player] : "Player";
}
