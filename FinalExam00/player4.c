#include "sample_ai.h"

/* P4 - Snatcher: shortest escape focus; fight chiefly for the key. */
Command player4_choose_command(const char *state_path)
{
    return sample_ai_choose(state_path, SAMPLE_SNATCH);
}
