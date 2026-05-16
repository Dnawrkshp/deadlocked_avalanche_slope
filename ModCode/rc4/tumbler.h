#ifndef TUMBLER_H
#define TUMBLER_H

#include <libdl/math3d.h>
#include <libdl/game.h>
#include <libdl/moby.h>

//--------------------------------------------------------------------------
struct TumblerSpawnArgs
{
	int OClass;
	VECTOR Position;
	VECTOR Rotation;
	VECTOR Velocity;
	VECTOR AngularVelocity;
	VECTOR Force;
	float Size;
	float Mass;
	float Restitution;
	float BounceImpulseScale;
};

//--------------------------------------------------------------------------
int mobyIsTumbler(Moby* moby);
int tumblerGetDefinitionCount(void);
int tumblerGetDefinitionOClass(int index);
int tumblerGetRandomOClass(void);
void tumblerInit(void);
void tumblerSpawn(struct TumblerSpawnArgs *args);
void tumblerSpawnDrawDebug(struct TumblerSpawnArgs *args);
void tumblerGetPosition(Moby *moby, VECTOR out);
void tumblerSetPosition(Moby *moby, VECTOR position);

#endif // TUMBLER_H
