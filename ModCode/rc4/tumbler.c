#include <libdl/utils.h>
#include <libdl/stdio.h>
#include <libdl/collision.h>
#include <libdl/time.h>
#include <libdl/math3d.h>
#include <libdl/math.h>
#include <libdl/graphics.h>
#include "tumbler.h"

#define TUMBLER_GRAVITY                 (22.0f)
#define TUMBLER_LIFETIME_TICKS          (60 * 60)

#define TUMBLER_MIN_SCALE               (0.1f)
#define TUMBLER_MAX_SCALE               (40.0f)

#define TUMBLER_CONTACT_SKIN            (0.08f)
#define TUMBLER_CONTACT_BIAS            (0.03f)
#define TUMBLER_FRICTION                (0.55f)
#define TUMBLER_RESTING_NORMAL_SPEED    (0.65f)
#define TUMBLER_MIN_IMPULSE_DENOM       (0.0001f)
#define TUMBLER_MIN_BOUNCE_IMPULSE_SCALE (0.0f)
#define TUMBLER_MAX_BOUNCE_IMPULSE_SCALE (8.0f)

#define TUMBLER_LINEAR_DAMPING          (0.18f)
#define TUMBLER_ANGULAR_DAMPING         (0.28f)
#define TUMBLER_MAX_LINEAR_SPEED        (40.0f)
#define TUMBLER_MAX_ANGULAR_SPEED       (18.0f)
#define TUMBLER_REST_PROBE_LENGTH       (0.05f)
#define TUMBLER_MIN_SWEEP_LENGTH        (0.02f)

struct TumblerPVar
{
	VECTOR Velocity;
	VECTOR AngularVelocity;
	VECTOR Orientation;
	VECTOR ColliderHalfExtents;
	VECTOR ColliderOffset;
	float Mass;
	float InverseMass;
	float InverseInertiaBody[3];
	float Restitution;
	float BounceImpulseScale;
	int Lifeticks;
};

struct TumblerDefinition
{
	int OClass;
	VECTOR ColliderOffset;
	VECTOR ColliderHalfExtents;
};

struct TumblerContact
{
	VECTOR HitPosition;
	VECTOR HitNormal;
	VECTOR ProbePoint;
};

static struct TumblerDefinition TumblerDefinitions[] =
{
	{
		MOBY_ID_BETA_BOX,
		{ 0.0f, 0.0f, 0.5f, 0.0f },
		{ 0.5f, 0.5f, 0.5f, 0.0f }
	}
};

#define TUMBLER_DEFINITION_COUNT        (sizeof(TumblerDefinitions) / sizeof(TumblerDefinitions[0]))

//--------------------------------------------------------------------------
static void tumblerVectorZero(VECTOR v)
{
	vector_write(v, 0);
}

//--------------------------------------------------------------------------
static void tumblerVectorSet(VECTOR v, float x, float y, float z)
{
	v[0] = x;
	v[1] = y;
	v[2] = z;
	v[3] = 0.0f;
}

//--------------------------------------------------------------------------
static struct TumblerDefinition *tumblerGetDefinition(int oClass)
{
	u32 i;

	for (i = 0; i < TUMBLER_DEFINITION_COUNT; ++i)
	{
		if (TumblerDefinitions[i].OClass == oClass)
			return &TumblerDefinitions[i];
	}

	return &TumblerDefinitions[0];
}

//--------------------------------------------------------------------------
static void tumblerClampVectorLength(VECTOR v, float maxLength)
{
	float length = vector_length(v);
	if (length > maxLength && length > 0.0f)
	{
		vector_scale(v, v, maxLength / length);
		v[3] = 0.0f;
	}
}

//--------------------------------------------------------------------------
static void tumblerApplyDamping(VECTOR v, float damping)
{
	float scale = 1.0f - (damping * MATH_DT);
	if (scale < 0.0f)
		scale = 0.0f;

	vector_scale(v, v, scale);
	v[3] = 0.0f;
}

//--------------------------------------------------------------------------
static void tumblerQuatSetIdentity(VECTOR q)
{
	q[0] = 0.0f;
	q[1] = 0.0f;
	q[2] = 0.0f;
	q[3] = 1.0f;
}

//--------------------------------------------------------------------------
static void tumblerQuatMultiply(VECTOR out, VECTOR a, VECTOR b)
{
	VECTOR result;

	result[0] = (a[3] * b[0]) + (a[0] * b[3]) + (a[1] * b[2]) - (a[2] * b[1]);
	result[1] = (a[3] * b[1]) - (a[0] * b[2]) + (a[1] * b[3]) + (a[2] * b[0]);
	result[2] = (a[3] * b[2]) + (a[0] * b[1]) - (a[1] * b[0]) + (a[2] * b[3]);
	result[3] = (a[3] * b[3]) - (a[0] * b[0]) - (a[1] * b[1]) - (a[2] * b[2]);

	quat_normalize(out, result);
}

//--------------------------------------------------------------------------
static void tumblerQuatToMatrix(VECTOR q, MATRIX out)
{
	VECTOR row0;
	VECTOR row1;
	VECTOR row2;
	VECTOR row3;
	float x = q[0];
	float y = q[1];
	float z = q[2];
	float w = q[3];
	float xx = x * x;
	float yy = y * y;
	float zz = z * z;
	float xy = x * y;
	float xz = x * z;
	float yz = y * z;
	float wx = w * x;
	float wy = w * y;
	float wz = w * z;

	tumblerVectorSet(row0, 1.0f - (2.0f * (yy + zz)), 2.0f * (xy + wz), 2.0f * (xz - wy));
	tumblerVectorSet(row1, 2.0f * (xy - wz), 1.0f - (2.0f * (xx + zz)), 2.0f * (yz + wx));
	tumblerVectorSet(row2, 2.0f * (xz + wy), 2.0f * (yz - wx), 1.0f - (2.0f * (xx + yy)));
	tumblerVectorSet(row3, 0.0f, 0.0f, 0.0f);
	row3[3] = 1.0f;
	matrix_fromrows(out, row0, row1, row2, row3);
}

//--------------------------------------------------------------------------
static void tumblerBuildEulerRotationMatrix(VECTOR rotation, MATRIX out)
{
	matrix_unit(out);
	matrix_rotate_x(out, out, rotation[0]);
	matrix_rotate_y(out, out, rotation[1]);
	matrix_rotate_z(out, out, rotation[2]);
}

//--------------------------------------------------------------------------
static void tumblerBuildRotationMatrix(Moby *moby, MATRIX out)
{
	struct TumblerPVar *pvars = (struct TumblerPVar *)moby->PVar;

	tumblerQuatToMatrix(pvars->Orientation, out);
}

//--------------------------------------------------------------------------
static void tumblerSyncMobyRotation(Moby *moby)
{
	MATRIX rot;

	tumblerBuildRotationMatrix(moby, rot);
	matrix_toeuler(rot, moby->Rotation);
	vector_clampeuler(moby->Rotation, moby->Rotation);
	moby->Rotation[3] = 0.0f;
}

//--------------------------------------------------------------------------
static void tumblerIntegrateOrientation(Moby *moby)
{
	struct TumblerPVar *pvars = (struct TumblerPVar *)moby->PVar;
	VECTOR axis;
	VECTOR delta;
	VECTOR next;
	float angularSpeed = vector_length(pvars->AngularVelocity);

	if (angularSpeed <= 0.0001f)
		return;

	vector_scale(axis, pvars->AngularVelocity, 1.0f / angularSpeed);
	axis[3] = 0.0f;
	quat_fromangleaxis(delta, axis, angularSpeed * MATH_DT);
	quat_normalize(delta, delta);
	tumblerQuatMultiply(next, delta, pvars->Orientation);
	vector_copy(pvars->Orientation, next);
	tumblerSyncMobyRotation(moby);
}

//--------------------------------------------------------------------------
static void tumblerGetWorldColliderOffset(Moby *moby, VECTOR out)
{
	struct TumblerPVar *pvars = (struct TumblerPVar *)moby->PVar;
	MATRIX rot;

	tumblerBuildRotationMatrix(moby, rot);
	vector_apply(out, pvars->ColliderOffset, rot);
	out[3] = 0.0f;
}

//--------------------------------------------------------------------------
void tumblerGetPosition(Moby *moby, VECTOR out)
{
	VECTOR worldOffset;

	tumblerGetWorldColliderOffset(moby, worldOffset);
	vector_add(out, moby->Position, worldOffset);
	out[3] = 0.0f;
}

//--------------------------------------------------------------------------
void tumblerSetPosition(Moby *moby, VECTOR position)
{
	VECTOR worldOffset;

	tumblerGetWorldColliderOffset(moby, worldOffset);
	vector_subtract(moby->Position, position, worldOffset);
	moby->Position[3] = 0.0f;
}

//--------------------------------------------------------------------------
static int tumblerRaycast(Moby* moby, VECTOR from, VECTOR to, struct TumblerContact *out, VECTOR probePoint)
{
	int hit = CollLine_Fix(from, to, COLLISION_FLAG_IGNORE_DYNAMIC, moby, NULL);

	if (out && hit)
	{
		vector_copy(out->HitPosition, CollLine_Fix_GetHitPosition());
		vector_normalize(out->HitNormal, CollLine_Fix_GetHitNormal());
		vector_copy(out->ProbePoint, probePoint);
		out->HitPosition[3] = 0.0f;
		out->HitNormal[3] = 0.0f;
		out->ProbePoint[3] = 0.0f;
	}

	return hit;
}

//--------------------------------------------------------------------------
static void tumblerApplyInverseInertiaWorld(Moby *moby, VECTOR worldVec, VECTOR out)
{
	struct TumblerPVar *pvars = (struct TumblerPVar *)moby->PVar;
	MATRIX rot;
	MATRIX invRot;
	VECTOR localVec;
	VECTOR localResult;

	tumblerBuildRotationMatrix(moby, rot);
	matrix_transpose(invRot, rot);

	vector_apply(localVec, worldVec, invRot);
	localVec[3] = 0.0f;

	localResult[0] = localVec[0] * pvars->InverseInertiaBody[0];
	localResult[1] = localVec[1] * pvars->InverseInertiaBody[1];
	localResult[2] = localVec[2] * pvars->InverseInertiaBody[2];
	localResult[3] = 0.0f;

	vector_apply(out, localResult, rot);
	out[3] = 0.0f;
}

//--------------------------------------------------------------------------
static void tumblerGetWorldCorners(Moby *moby, VECTOR outCorners[8])
{
	struct TumblerPVar *pvars = (struct TumblerPVar *)moby->PVar;
	MATRIX rot;
	VECTOR center;
	VECTOR local;
	VECTOR worldOffset;
	int i;

	tumblerBuildRotationMatrix(moby, rot);
	tumblerGetPosition(moby, center);

	for (i = 0; i < 8; ++i)
	{
		local[0] = (i & 1) ? pvars->ColliderHalfExtents[0] : -pvars->ColliderHalfExtents[0];
		local[1] = (i & 2) ? pvars->ColliderHalfExtents[1] : -pvars->ColliderHalfExtents[1];
		local[2] = (i & 4) ? pvars->ColliderHalfExtents[2] : -pvars->ColliderHalfExtents[2];
		local[3] = 0.0f;

		vector_apply(worldOffset, local, rot);
		worldOffset[3] = 0.0f;
		vector_add(outCorners[i], center, worldOffset);
		outCorners[i][3] = 0.0f;
	}
}

//--------------------------------------------------------------------------
static void tumblerApplyImpulse(Moby *moby, VECTOR r, VECTOR impulse)
{
	struct TumblerPVar *pvars = (struct TumblerPVar *)moby->PVar;
	VECTOR linearChange;
	VECTOR angularImpulse;
	VECTOR angularChange;

	vector_scale(linearChange, impulse, pvars->InverseMass);
	vector_add(pvars->Velocity, pvars->Velocity, linearChange);
	pvars->Velocity[3] = 0.0f;

	vector_outerproduct(angularImpulse, r, impulse);
	angularImpulse[3] = 0.0f;
	tumblerApplyInverseInertiaWorld(moby, angularImpulse, angularChange);
	vector_add(pvars->AngularVelocity, pvars->AngularVelocity, angularChange);
	pvars->AngularVelocity[3] = 0.0f;
}

//--------------------------------------------------------------------------
static float tumblerComputeImpulseDenom(Moby *moby, VECTOR r, VECTOR normal)
{
	struct TumblerPVar *pvars = (struct TumblerPVar *)moby->PVar;
	VECTOR rCrossN;
	VECTOR invInertiaRCrossN;
	VECTOR angularTerm;
	float denom;

	vector_outerproduct(rCrossN, r, normal);
	rCrossN[3] = 0.0f;
	tumblerApplyInverseInertiaWorld(moby, rCrossN, invInertiaRCrossN);
	vector_outerproduct(angularTerm, invInertiaRCrossN, r);
	angularTerm[3] = 0.0f;

	denom = pvars->InverseMass + vector_innerproduct_unscaled(normal, angularTerm);
	if (denom < TUMBLER_MIN_IMPULSE_DENOM)
		denom = TUMBLER_MIN_IMPULSE_DENOM;

	return denom;
}

//--------------------------------------------------------------------------
static void tumblerResolveContact(Moby *moby, struct TumblerContact *contact)
{
	struct TumblerPVar *pvars = (struct TumblerPVar *)moby->PVar;
	VECTOR center;
	VECTOR r;
	VECTOR angularVelocityAtPoint;
	VECTOR contactVelocity;
	VECTOR normalVelocity;
	VECTOR tangentVelocity;
	VECTOR tangent;
	VECTOR impulse;
	float velocityAlongNormal;
	float restitution;
	float normalDenom;
	float normalImpulseMagnitude;
	float tangentSpeed;

	tumblerGetPosition(moby, center);
	vector_subtract(r, contact->HitPosition, center);
	r[3] = 0.0f;

	vector_outerproduct(angularVelocityAtPoint, pvars->AngularVelocity, r);
	angularVelocityAtPoint[3] = 0.0f;
	vector_add(contactVelocity, pvars->Velocity, angularVelocityAtPoint);
	contactVelocity[3] = 0.0f;

	velocityAlongNormal = vector_innerproduct_unscaled(contactVelocity, contact->HitNormal);
	if (velocityAlongNormal >= 0.0f)
		return;

	restitution = pvars->Restitution;
	if (velocityAlongNormal > -TUMBLER_RESTING_NORMAL_SPEED)
		restitution = 0.0f;

	normalDenom = tumblerComputeImpulseDenom(moby, r, contact->HitNormal);
	normalImpulseMagnitude = (-(1.0f + restitution) * velocityAlongNormal) / normalDenom;
	normalImpulseMagnitude *= pvars->BounceImpulseScale;

	vector_scale(impulse, contact->HitNormal, normalImpulseMagnitude);
	impulse[3] = 0.0f;
	tumblerApplyImpulse(moby, r, impulse);

	vector_outerproduct(angularVelocityAtPoint, pvars->AngularVelocity, r);
	angularVelocityAtPoint[3] = 0.0f;
	vector_add(contactVelocity, pvars->Velocity, angularVelocityAtPoint);
	contactVelocity[3] = 0.0f;

	vector_scale(normalVelocity, contact->HitNormal, vector_innerproduct_unscaled(contactVelocity, contact->HitNormal));
	vector_subtract(tangentVelocity, contactVelocity, normalVelocity);
	tangentVelocity[3] = 0.0f;

	tangentSpeed = vector_length(tangentVelocity);
	if (tangentSpeed > 0.001f)
	{
		float frictionDenom;
		float frictionImpulseMagnitude;
		float maxFrictionImpulse;

		vector_scale(tangent, tangentVelocity, 1.0f / tangentSpeed);
		tangent[3] = 0.0f;

		frictionDenom = tumblerComputeImpulseDenom(moby, r, tangent);
		frictionImpulseMagnitude = -tangentSpeed / frictionDenom;
		maxFrictionImpulse = normalImpulseMagnitude * TUMBLER_FRICTION;
		frictionImpulseMagnitude = clamp(frictionImpulseMagnitude, -maxFrictionImpulse, maxFrictionImpulse);

		vector_scale(impulse, tangent, frictionImpulseMagnitude);
		impulse[3] = 0.0f;
		tumblerApplyImpulse(moby, r, impulse);
	}
}

//--------------------------------------------------------------------------
static void tumblerCorrectContacts(Moby *moby, struct TumblerContact *contacts, int contactCount)
{
	VECTOR center;
	VECTOR toProbe;
	VECTOR correction;
	VECTOR correctionNormal;
	float separation;
	float correctionDistance;
	float maxCorrectionDistance = 0.0f;
	int i;

	for (i = 0; i < contactCount; ++i)
	{
		vector_subtract(toProbe, contacts[i].ProbePoint, contacts[i].HitPosition);
		toProbe[3] = 0.0f;

		separation = vector_innerproduct_unscaled(toProbe, contacts[i].HitNormal);
		if (separation >= TUMBLER_CONTACT_SKIN)
			continue;

		correctionDistance = (TUMBLER_CONTACT_SKIN - separation) + TUMBLER_CONTACT_BIAS;
		if (correctionDistance > maxCorrectionDistance)
		{
			maxCorrectionDistance = correctionDistance;
			vector_copy(correctionNormal, contacts[i].HitNormal);
			correctionNormal[3] = 0.0f;
		}
	}

	if (maxCorrectionDistance <= 0.0f)
		return;

	vector_scale(correction, correctionNormal, maxCorrectionDistance);
	correction[3] = 0.0f;

	tumblerGetPosition(moby, center);
	vector_add(center, center, correction);
	tumblerSetPosition(moby, center);
}

//--------------------------------------------------------------------------
static void tumblerRemoveRestingNormalVelocity(Moby *moby, VECTOR normal)
{
	struct TumblerPVar *pvars = (struct TumblerPVar *)moby->PVar;
	VECTOR normalVelocity;
	float normalSpeed = vector_innerproduct_unscaled(pvars->Velocity, normal);

	if (normalSpeed < 0.0f && normalSpeed > -TUMBLER_RESTING_NORMAL_SPEED)
	{
		vector_scale(normalVelocity, normal, normalSpeed);
		vector_subtract(pvars->Velocity, pvars->Velocity, normalVelocity);
		pvars->Velocity[3] = 0.0f;
	}
}

//--------------------------------------------------------------------------
static int tumblerCollectContacts(Moby *moby, struct TumblerContact *contacts, int maxContacts, VECTOR outAverageNormal)
{
	struct TumblerPVar *pvars = (struct TumblerPVar *)moby->PVar;
	VECTOR corners[8];
	VECTOR sweep;
	VECTOR from;
	VECTOR to;
	float sweepLength;
	int contactCount = 0;
	int i;

	tumblerGetWorldCorners(moby, corners);
	vector_scale(sweep, pvars->Velocity, MATH_DT);
	if (sweep[2] < 0.0f)
		sweep[2] -= TUMBLER_CONTACT_SKIN;
	else
		sweep[2] -= TUMBLER_MIN_SWEEP_LENGTH;
	sweep[3] = 0.0f;

	sweepLength = vector_length(sweep);
	if (sweepLength < TUMBLER_MIN_SWEEP_LENGTH)
	{
		tumblerVectorSet(sweep, 0.0f, 0.0f, -TUMBLER_MIN_SWEEP_LENGTH);
	}

	tumblerVectorZero(outAverageNormal);

	for (i = 0; i < 8 && contactCount < maxContacts; ++i)
	{
		vector_copy(from, corners[i]);
		vector_add(to, corners[i], sweep);
		to[3] = 0.0f;

		if (tumblerRaycast(moby, from, to, &contacts[contactCount], corners[i]))
		{
			vector_add(outAverageNormal, outAverageNormal, contacts[contactCount].HitNormal);
			outAverageNormal[3] = 0.0f;
			++contactCount;
		}
	}

	for (i = 0; i < 8 && contactCount < maxContacts; ++i)
	{
		vector_copy(from, corners[i]);
		vector_copy(to, corners[i]);
		to[2] -= TUMBLER_REST_PROBE_LENGTH;
		to[3] = 0.0f;

		if (tumblerRaycast(moby, from, to, &contacts[contactCount], corners[i]))
		{
			vector_add(outAverageNormal, outAverageNormal, contacts[contactCount].HitNormal);
			outAverageNormal[3] = 0.0f;
			++contactCount;
		}
	}

	if (contactCount > 0)
	{
		vector_scale(outAverageNormal, outAverageNormal, 1.0f / (float)contactCount);
		if (vector_length(outAverageNormal) > 0.001f)
			vector_normalize(outAverageNormal, outAverageNormal);
		else
			tumblerVectorSet(outAverageNormal, 0.0f, 0.0f, 1.0f);
	}

	return contactCount;
}

//--------------------------------------------------------------------------
void tumblerDraw(Moby *moby)
{
	VECTOR center;
	VECTOR corners[8];
	char cornerLabels[] = "01234567";
	int x;
	int y;
	int i;

	tumblerGetPosition(moby, center);
	if (gfxWorldSpaceToScreenSpace(center, &x, &y))
		gfxScreenSpaceText(x, y, 1, 1, 0x80FFFFFF, "+", -1, TEXT_ALIGN_MIDDLECENTER);

	tumblerGetWorldCorners(moby, corners);
	for (i = 0; i < 8; ++i)
	{
		if (gfxWorldSpaceToScreenSpace(corners[i], &x, &y))
			gfxScreenSpaceText(x, y, 1, 1, 0x80FFAA40, &cornerLabels[i], 1, TEXT_ALIGN_MIDDLECENTER);
	}
}

//--------------------------------------------------------------------------
void tumblerUpdate(Moby *moby)
{
	struct TumblerPVar *pvars = (struct TumblerPVar *)moby->PVar;
	struct TumblerContact contacts[16];
	VECTOR gravityStep;
	VECTOR averageNormal;
	VECTOR center;
	int contactCount;
	int i;

	--pvars->Lifeticks;
	if (pvars->Lifeticks <= 0 || moby->Position[2] <= (gameGetDeathHeight() + 5))
	{
		mobyDestroy(moby);
		return;
	}

#if DEBUG
	gfxRegisterDrawFunction((void**)0x0022251C, (gfxDrawFuncDef*)&tumblerDraw, moby);
#endif

	tumblerVectorSet(gravityStep, 0.0f, 0.0f, -TUMBLER_GRAVITY * MATH_DT);
	vector_add(pvars->Velocity, pvars->Velocity, gravityStep);
	pvars->Velocity[3] = 0.0f;

	contactCount = tumblerCollectContacts(moby, contacts, 16, averageNormal);
	for (i = 0; i < contactCount; ++i)
		tumblerResolveContact(moby, &contacts[i]);

	tumblerCorrectContacts(moby, contacts, contactCount);

	if (contactCount > 0)
		tumblerRemoveRestingNormalVelocity(moby, averageNormal);

	tumblerApplyDamping(pvars->Velocity, TUMBLER_LINEAR_DAMPING);
	tumblerApplyDamping(pvars->AngularVelocity, TUMBLER_ANGULAR_DAMPING);

	tumblerClampVectorLength(pvars->Velocity, TUMBLER_MAX_LINEAR_SPEED);
	tumblerClampVectorLength(pvars->AngularVelocity, TUMBLER_MAX_ANGULAR_SPEED);

	tumblerGetPosition(moby, center);
	vector_scale(gravityStep, pvars->Velocity, MATH_DT);
	vector_add(center, center, gravityStep);
	tumblerSetPosition(moby, center);

	tumblerIntegrateOrientation(moby);
	tumblerSetPosition(moby, center);

	mobyUpdateTransform(moby);
}

//--------------------------------------------------------------------------
static void tumblerInitInertia(struct TumblerPVar *pvars)
{
	float width = pvars->ColliderHalfExtents[0] * 2.0f;
	float height = pvars->ColliderHalfExtents[1] * 2.0f;
	float depth = pvars->ColliderHalfExtents[2] * 2.0f;
	float ix = (1.0f / 12.0f) * pvars->Mass * ((height * height) + (depth * depth));
	float iy = (1.0f / 12.0f) * pvars->Mass * ((width * width) + (depth * depth));
	float iz = (1.0f / 12.0f) * pvars->Mass * ((width * width) + (height * height));

	pvars->InverseInertiaBody[0] = ix > 0.0f ? 1.0f / ix : 0.0f;
	pvars->InverseInertiaBody[1] = iy > 0.0f ? 1.0f / iy : 0.0f;
	pvars->InverseInertiaBody[2] = iz > 0.0f ? 1.0f / iz : 0.0f;
}

//--------------------------------------------------------------------------
void tumblerSpawn(int oClass, VECTOR position, VECTOR rotation, float size, float mass, float restitution, float bounceImpulseScale, VECTOR velocity, VECTOR angularVelocity)
{
	struct TumblerDefinition *definition = tumblerGetDefinition(oClass);
	Moby *moby = mobySpawn(oClass, sizeof(struct TumblerPVar));
	struct TumblerPVar *pvars;
	MATRIX rot;
	float scale = clamp(size, TUMBLER_MIN_SCALE, TUMBLER_MAX_SCALE);

	if (!moby)
		return;

	pvars = (struct TumblerPVar *)moby->PVar;
	pvars->Lifeticks = TUMBLER_LIFETIME_TICKS;

	vector_copy(pvars->Velocity, velocity);
	pvars->Velocity[3] = 0.0f;
	vector_copy(pvars->AngularVelocity, angularVelocity);
	pvars->AngularVelocity[3] = 0.0f;
	tumblerVectorSet(
		pvars->ColliderHalfExtents,
		definition->ColliderHalfExtents[0] * scale,
		definition->ColliderHalfExtents[1] * scale,
		definition->ColliderHalfExtents[2] * scale);
	tumblerVectorSet(
		pvars->ColliderOffset,
		definition->ColliderOffset[0] * scale,
		definition->ColliderOffset[1] * scale,
		definition->ColliderOffset[2] * scale);

	pvars->Mass = mass;
	pvars->InverseMass = pvars->Mass > 0.0f ? 1.0f / pvars->Mass : 0.0f;
	pvars->Restitution = clamp(restitution, 0.0f, 1.0f);
	pvars->BounceImpulseScale = clamp(bounceImpulseScale, TUMBLER_MIN_BOUNCE_IMPULSE_SCALE, TUMBLER_MAX_BOUNCE_IMPULSE_SCALE);
	tumblerInitInertia(pvars);

	moby->DrawDist = 255;
	moby->UpdateDist = 255;
	moby->ModeBits = MOBY_MODE_BIT_HIDE_BACKFACES;
	moby->Scale *= scale;
  moby->Bolts = -123;

	vector_copy(moby->Rotation, rotation);
	moby->Rotation[3] = 0.0f;
	tumblerBuildEulerRotationMatrix(moby->Rotation, rot);
	quat_from_matrix(pvars->Orientation, rot);
	quat_normalize(pvars->Orientation, pvars->Orientation);
	if (vector_length(pvars->Orientation) <= 0.0001f)
		tumblerQuatSetIdentity(pvars->Orientation);
	tumblerSyncMobyRotation(moby);
	tumblerSetPosition(moby, position);

	moby->PUpdate = &tumblerUpdate;
	mobyUpdateTransform(moby);

	// printf("TUMBLER SPAWNED AT %08X\n", (u32)moby);
}

//--------------------------------------------------------------------------
int mobyIsTumbler(Moby* moby)
{
  return moby && moby->Bolts == -123;
}
