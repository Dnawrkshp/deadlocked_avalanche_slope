#ifndef TUMBLER_H
#define TUMBLER_H

#include <libdl/math3d.h>
#include <libdl/game.h>
#include <libdl/moby.h>

int mobyIsTumbler(Moby* moby);
void tumblerSpawn(int oClass, VECTOR position, VECTOR rotation, float size, float mass, float restitution, float bounceImpulseScale, VECTOR velocity, VECTOR angularVelocity);
void tumblerGetPosition(Moby *moby, VECTOR out);
void tumblerSetPosition(Moby *moby, VECTOR position);

#endif // TUMBLER_H
