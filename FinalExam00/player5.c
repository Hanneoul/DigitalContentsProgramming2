#include "sample_ai.h"

/* P5 - Balanced: objective-first with occasional opportunistic combat. */
Command player5_choose_command(const char *state_path)
{
    return sample_ai_choose(state_path, SAMPLE_BALANCED);
}
