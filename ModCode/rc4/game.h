#ifndef GAME_H
#define GAME_H

#include <libdl/game.h>
#include <libdl/player.h>

//--------------------------------------------------------------------------
struct GameState
{
  int SpawnTumblerTicker;
};

//--------------------------------------------------------------------------
extern struct GameState State;

#endif // GAME_H
