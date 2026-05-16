#ifndef GAME_H
#define GAME_H

#include <libdl/game.h>
#include <libdl/player.h>

//--------------------------------------------------------------------------
enum GameCustomStatIds
{
  CSTAT_DISTANCE_UP,
  CSTAT_COMPLETE_TIME,
  CSTAT_DISTANCE_FORWARD,
};

//--------------------------------------------------------------------------
struct GameState
{
  int SpawnTumblerTicker;
  int GameTicks;
  Moby* HillMoby;
};

//--------------------------------------------------------------------------
extern struct GameState State;

//--------------------------------------------------------------------------
int gameIsPointInTumbleZone(VECTOR point);

#endif // GAME_H
