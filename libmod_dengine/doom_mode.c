#include "doom_mode.h"
#include <string.h>

const char *D_GameMissionString(GameMission_t mission)
{
    switch (mission)
    {
        case doom: return "doom";
        case heretic: return "heretic";
        case hexen: return "hexen";
        case strife: return "strife";
        default: return "unknown";
    }
}

const char *D_SuggestGameName(GameMission_t mission, GameMode_t mode)
{
    switch (mission)
    {
        case doom: return "Doom";
        case heretic: return "Heretic";
        case hexen: return "Hexen";
        case strife: return "Strife";
        default: return "Unknown";
    }
}
