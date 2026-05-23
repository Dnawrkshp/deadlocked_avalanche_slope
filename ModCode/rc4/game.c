#include <libdl/stdio.h>
#include <libdl/random.h>
#include <libdl/net.h>
#include <libdl/spawnpoint.h>
#include <libdl/area.h>
#include <libdl/collision.h>
#include <libdl/time.h>
#include "game.h"
#include "tumbler.h"
#include "cgm.h"
#include "cgm_score.h"
#include "cgm_netmsg.h"

#ifndef SPAWN_TUMBLERS_AREA_IDX
#define SPAWN_TUMBLERS_AREA_IDX (0)
#endif

#ifndef PLAYER_SPAWN_CUBOID_IDX
#define PLAYER_SPAWN_CUBOID_IDX (1)
#endif

#ifndef SLOPE_BASE_CUBOID_IDX
#define SLOPE_BASE_CUBOID_IDX (2)
#endif

#define TUMBLER_DEBUG_LINE_SPACING      (5.0f)

//--------------------------------------------------------------------------
struct GameState State = {};

//--------------------------------------------------------------------------
void setHillPosition(float y, float z, int team)
{
  if (!State.HillMoby)
    return;

  int hillCuboidIdx = *(int*)(State.HillMoby->PVar + 0x80);
  *(short*)(State.HillMoby->PVar + 0x7c) = team;
  *(short*)(State.HillMoby->PVar + 0x7e) = 0; // square
  SpawnPoint* hillCuboid = spawnPointGet(hillCuboidIdx);

  hillCuboid->M0[13] = y;
  hillCuboid->M0[14] = z + 1;
  hillCuboid->M0[15] = 0.01f;
  
  // enlarge hill marker on dzo
  float* cuboidVertical = &hillCuboid->M0[8];
  vector_normalize(cuboidVertical, cuboidVertical);
  vector_scale(cuboidVertical, cuboidVertical, 3);

  // move hill moby to cuboid
  vector_copy(State.HillMoby->Position, &hillCuboid->M0[12]);
}

//--------------------------------------------------------------------------
int gameIsPointInTumbleZone(VECTOR point)
{
  if (point[1] < 564)
    return 0;

  return 1;
}

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
  if (gameHasEnded())
    return;
    
  SpawnPoint* baseCuboid = spawnPointGet(SLOPE_BASE_CUBOID_IDX);
  float baseForward = baseCuboid->M0[13];
  float baseHeight = baseCuboid->M0[14];
  int i;
  for (i = 0; i < GAME_MAX_PLAYERS; ++i)
  {
    Player *p = playerGetFromIndex(i);
    if (!playerIsValid(p)) continue;
    if (p->Health <= 0) continue;
    if (!p->Ground.onGood) continue;

    // relative to spawn point height
    float forward = p->PlayerPosition[1] - baseForward;
    float score = p->PlayerPosition[2] - baseHeight;
    float currScore = cgmScoreGetCustomPlayerFloatStat(i, CSTAT_DISTANCE_UP);
    float newScore = clamp(maxf(score, currScore), 0, cgmScoreTarget.CustomTarget / SCORE_FLOAT_PRECISION);
    cgmScoreSetCustomPlayerFloatStat(i, CSTAT_DISTANCE_UP, newScore, 0);
    cgmScoreSetCustomPlayerFloatStat(i, CSTAT_DISTANCE_FORWARD, maxf(forward, cgmScoreGetCustomPlayerFloatStat(i, CSTAT_DISTANCE_FORWARD)), 0);

    // check if reached end
    if (cgmScoreGetCustomPlayerIntStat(i, CSTAT_DISTANCE_UP) >= cgmScoreGetTargetScore())
    {
      cgmScoreSetCustomPlayerIntStat(i, CSTAT_COMPLETE_TIME, State.GameTicks / 60.0f, 0);
    }
  }

  float bestScore = 0;
  float bestForward = 0;
  int bestScoreIdx = 10;
  for (i = 0; i < GAME_MAX_PLAYERS; ++i)
  {
    float score = cgmScoreGetCustomPlayerFloatStat(i, CSTAT_DISTANCE_UP);
    if (score > bestScore)
    {
      bestForward = cgmScoreGetCustomPlayerFloatStat(i, CSTAT_DISTANCE_FORWARD);
      bestScore = score;
      bestScoreIdx = i;
    }
  }

  // slope is y=1/3x
  setHillPosition(bestForward + baseForward, bestScore + baseHeight, bestScoreIdx);
}

//--------------------------------------------------------------------------
void onRecvSpawnTumbler(int fromClientId, struct SpawnTumblerMessage *msg)
{
  if (!isInGame())
    return;

  tumblerSpawn(&msg->SpawnArgs);
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
  struct SpawnTumblerMessage msg;
  float forceMagnitude = randRange(5, 25);

  // get random start point in cuboid
  vector_write(msg.SpawnArgs.Position, randVectorRange(-1, 1));
  vector_apply(msg.SpawnArgs.Position, msg.SpawnArgs.Position, cuboid->M0);

  // use cuboid forward for start direction
  vector_copy(msg.SpawnArgs.Velocity, &cuboid->M0[4]);
  vector_scale(msg.SpawnArgs.Velocity, msg.SpawnArgs.Velocity, randRange(4, 25));

  // keep pushing downhill so tumblers do not stall on shallow/rough sections
  vector_copy(msg.SpawnArgs.Force, &cuboid->M0[4]);
  vector_scale(msg.SpawnArgs.Force, msg.SpawnArgs.Force, forceMagnitude);

  // compute random angular velocity
  vector_write(msg.SpawnArgs.AngularVelocity, 0);

  // compute random rotation
  vector_write(msg.SpawnArgs.Rotation, randVector(1));

  msg.SpawnArgs.OClass = tumblerGetRandomOClass();
  msg.SpawnArgs.Size = randRange(0.5, 1);
  msg.SpawnArgs.Mass = 1.0f; //randRange(0.5f, 10.0f);
  msg.SpawnArgs.Restitution = 1.0f;
  msg.SpawnArgs.BounceImpulseScale = 1.0f;

  // broadcast spawn
  cgmNetMsgBroadcast_SpawnTumbler(&msg);
}

//--------------------------------------------------------------------------
void resetTumblerSpawnTicker(void)
{
  State.SpawnTumblerTicker = randRangeInt(2, 15);
}

//--------------------------------------------------------------------------
void spawnTumblerDefinitionDebugLine(void)
{
  SpawnPoint* cuboid = spawnPointGet(PLAYER_SPAWN_CUBOID_IDX);
  int count = tumblerGetDefinitionCount();
  VECTOR lineRight;
  VECTOR offset;
  struct TumblerSpawnArgs args;
  int i;

  if (!cuboid || count <= 0)
    return;

  vector_copy(lineRight, &cuboid->M0[0]);
  if (vector_length(lineRight) > 0.001f)
    vector_normalize(lineRight, lineRight);
  else
  {
    vector_write(lineRight, 0);
    lineRight[0] = 1.0f;
  }
  lineRight[3] = 0.0f;

  vector_write(args.Rotation, 0);
  vector_write(args.Velocity, 0);
  vector_write(args.AngularVelocity, 0);
  vector_write(args.Force, 0);
  args.Size = 1.0f;
  args.Mass = 1.0f;
  args.Restitution = 1.0f;
  args.BounceImpulseScale = 1.0f;

  for (i = 0; i < count; ++i)
  {
    args.OClass = tumblerGetDefinitionOClass(i);
    vector_scale(offset, lineRight, ((float)i - ((float)(count - 1) * 0.5f)) * TUMBLER_DEBUG_LINE_SPACING);
    vector_add(args.Position, &cuboid->M0[12], offset);
    args.Position[3] = 0.0f;
    tumblerSpawnDrawDebug(&args);
  }
}

//--------------------------------------------------------------------------
void modDraw(void)
{
  
}

//--------------------------------------------------------------------------
void modUpdate(void)
{
  // spawn periodically
  if (State.SpawnTumblerTicker == 0)
  {
#if !DEBUG
    spawnRandomTumbler(SPAWN_TUMBLERS_AREA_IDX);
#endif
    resetTumblerSpawnTicker();
  }
  else
  {
    --State.SpawnTumblerTicker;
  }

#if !DEBUG
  checkForPlayerHitTumbler();
#endif
  updatePlayerScores();
  ++State.GameTicks;
}

//--------------------------------------------------------------------------
void modCleanup(void)
{
  
}

//--------------------------------------------------------------------------
void modInit(void)
{
  State.GameTicks = 0;
  State.HillMoby = mobyFindNextByOClass(mobyListGetStart(), 0x2604);
  tumblerInit();
  resetTumblerSpawnTicker();

#if DEBUG
  spawnTumblerDefinitionDebugLine();
#endif

  POKE_U32(0x004443CC, 0); // disable hill radar blip
  //POKE_U32(0x004443B0, 0); // disable hill renderer
  POKE_U32(0x00444E1C, 0); // disable hill check for player in hill
  POKE_U32(0x00444E58, 0x8682007C); // lh v0,0x7C(s4)
  POKE_U32(0x00444E5C, 0x1451000D); // bne v0,s1,0x00444E94
  POKE_U32(0x00444E78, 0x4600A306); // mov.s f12,f20
  POKE_U32(0x004441CC, 0); // disable forced white hill team ownership
  POKE_U32(0x00444E8C, 0); // disable write hill team ownership
}
