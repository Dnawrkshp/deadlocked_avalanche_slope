#include <libdl/stdio.h>
#include <libdl/random.h>
#include <libdl/net.h>
#include <libdl/spawnpoint.h>
#include <libdl/area.h>
#include <libdl/time.h>
#include "game.h"
#include "tumbler.h"
#include "cgm.h"
#include "cgm_score.h"
#include "cgm_netmsg.h"

//--------------------------------------------------------------------------
struct GameState State = {};

//--------------------------------------------------------------------------
void checkForPlayerHitTumbler(void)
{
  int i;
  for (i = 0; i < GAME_MAX_LOCALS; ++i)
  {
    Player *p = playerGetFromSlot(i);
    if (!playerIsValid(p)) continue;
    if (!mobyIsTumbler(p->Coll.pContactMoby)) continue;

    // die
    playerSetHealth(p, 0);
    playerGetVTable(p)->UpdateState(p, PLAYER_STATE_GET_FLATTENED, 1, 0, 0);
  }
}

//--------------------------------------------------------------------------
void updatePlayerScores(void)
{
  int i;
  for (i = 0; i < GAME_MAX_PLAYERS; ++i)
  {
    Player *p = playerGetFromIndex(i);
    if (!playerIsValid(p)) continue;
    if (p->Health <= 0) continue;

    // relative to spawn point height
    float score = p->PlayerPosition[2] - spawnPointGet(1)->M0[14];
    float currScore = cgmScoreGetCustomPlayerFloatStat(i, 0);
    cgmScoreSetCustomPlayerFloatStat(i, 0, maxf(score, currScore));
  }
}

//--------------------------------------------------------------------------
void spawnRandomTumbler(int areaIdx)
{
  Area_t area;
  if (!areaGetArea(areaIdx, &area))
    return;

  if (area.CuboidCount <= 0)
    return;

  int cuboidIdx = rand(area.CuboidCount);
  SpawnPoint* cuboid = spawnPointGet(area.Cuboids[cuboidIdx]);
  
  float size = randRange(1, 2);
  float restitution = 1.0f;
  float bounceImpulseScale = 1.5f;
  float mass = 1.0f; //randRange(0.5f, 10.0f);

  // get random start point in cuboid
  VECTOR pos;
  vector_write(pos, randVectorRange(-1, 1));
  vector_apply(pos, pos, cuboid->M0);

  // use cuboid forward for start direction
  VECTOR vel;
  vector_copy(vel, &cuboid->M0[4]);
  vector_scale(vel, vel, randRange(4, 25));

  // compute random angular velocity
  VECTOR angVel;
  vector_write(angVel, 0);

  // compute random rotation
  VECTOR rot;
  vector_write(rot, randVector(1));

  // spawn
  tumblerSpawn(MOBY_ID_BETA_BOX, pos, rot, size, mass, restitution, bounceImpulseScale, vel, angVel);
}

//--------------------------------------------------------------------------
void frameUpdate(void)
{
}

//--------------------------------------------------------------------------
void gameUpdate(void)
{
  // spawn periodically
  if (State.SpawnTumblerTicker == 0)
  {
    spawnRandomTumbler(0);
    State.SpawnTumblerTicker = randRangeInt(5, 60);
  }
  else
  {
    --State.SpawnTumblerTicker;
  }

  checkForPlayerHitTumbler();
  updatePlayerScores();
}

//--------------------------------------------------------------------------
void gameInit(void)
{
}
