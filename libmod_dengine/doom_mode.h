// Modos de juego Doom - Simplificado
#ifndef __D_MODE_H
#define __D_MODE_H

typedef enum
{
    doom,
    heretic,
    hexen,
    strife,
    indetermined
} GameMission_t;

typedef enum
{
    shareware,
    registered,
    commercial,
    retail,
    indetermined_mode
} GameMode_t;

const char *D_GameMissionString(GameMission_t mission);
const char *D_SuggestGameName(GameMission_t mission, GameMode_t mode);

#endif
