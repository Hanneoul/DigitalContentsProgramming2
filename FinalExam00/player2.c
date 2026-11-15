#include "sample_ai.h"

/* P2 - Blocker: temporary exit interception plus key/escape objective. */
Command player2_choose_command(const char *state_path)
{
    return sample_ai_choose(state_path, SAMPLE_BLOCK);
}
