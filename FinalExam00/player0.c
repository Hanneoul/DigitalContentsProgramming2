#include "sample_ai.h"

/* P0 - Evader: avoid danger while seeking key and exit. */
Command player0_choose_command(const char *state_path)
{
    return sample_ai_choose(state_path, SAMPLE_EVADE);
}
