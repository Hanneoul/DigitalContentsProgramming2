#include "sample_ai.h"

/* P3 - Survivor: avoid risky paths and retreat when wounded. */
Command player3_choose_command(const char *state_path)
{
    return sample_ai_choose(state_path, SAMPLE_SURVIVE);
}
