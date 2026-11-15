#include "sample_ai.h"

/* P1 - Attacker: bounded hunts, but always collect key and escape. */
Command player1_choose_command(const char *state_path)
{
    return sample_ai_choose(state_path, SAMPLE_ATTACK);
}
