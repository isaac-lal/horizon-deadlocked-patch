#include <tamtypes.h>
#include <libdl/dl.h>
#include <libdl/player.h>
#include <libdl/pad.h>
#include <libdl/time.h>
#include <libdl/net.h>
#include <libdl/game.h>
#include <libdl/string.h>
#include <libdl/math.h>
#include <libdl/random.h>
#include <libdl/math3d.h>
#include <libdl/stdio.h>
#include <libdl/gamesettings.h>
#include <libdl/dialog.h>
#include <libdl/sound.h>
#include <libdl/patch.h>
#include <libdl/ui.h>
#include <libdl/graphics.h>
#include <libdl/color.h>
#include <libdl/collision.h>
#include <libdl/utils.h>
#include "../../../include/game.h"
#include "../../../include/mob.h"
#include "../include/gate.h"
#include "../include/maputils.h"
#include "../include/shared.h"
#include "../include/pathfind.h"
#include "messageid.h"
#include "module.h"

void mobForceIntoMapBounds(Moby* moby);

#if GATE
void gateSetCollision(int collActive);
#endif

extern int aaa;

#if MOB_ZOMBIE
#include "zombie.c"
#endif

#if MOB_EXECUTIONER
#include "executioner.c"
#endif

#if MOB_TREMOR
#include "tremor.c"
#endif

#if MOB_SWARMER
#include "swarmer.c"
#endif

#if MOB_REAPER
#include "reaper.c"
#endif

#if MOB_REACTOR
#include "trailshot.c"
#include "reactor.c"
#endif

#if DEBUGMOVE
VECTOR MoveCheckHit;
VECTOR MoveCheckFinal;
VECTOR MoveCheckUp;
VECTOR MoveCheckDown;
VECTOR MoveNextPos;
#endif

//--------------------------------------------------------------------------
int mobAmIOwner(Moby* moby)
{
  if (!mobyIsMob(moby)) return 0;
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	return gameGetMyClientId() == pvars->MobVars.Owner;
}

//--------------------------------------------------------------------------
int mobIsFrozen(Moby* moby)
{
  if (!moby || !moby->PVar || !MapConfig.State)
    return 0;

	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  return MapConfig.State->Freeze && pvars->MobVars.Config.MobAttribute != MOB_ATTRIBUTE_FREEZE && pvars->MobVars.Health > 0;
}

//--------------------------------------------------------------------------
void mobSpawnCorn(Moby* moby, int bangle)
{
#if MOB_CORN
	mobyBlowCorn(
			moby
		, bangle
		, 0
		, 3.0
		, 6.0
		, 3.0
		, 6.0
		, -1
		, -1.0
		, -1.0
		, 255
		, 1
		, 0
		, 1
		, 1.0
		, 0x23
		, 3
		, 1.0
		, NULL
		, 0
		);
#endif
}

//--------------------------------------------------------------------------
int mobDoDamageTryHit(Moby* moby, Moby* hitMoby, VECTOR jointPosition, float sqrHitRadius, int damageFlags, float amount)
{
  VECTOR mobToHitMoby, mobToJoint, jointToHitMoby;
	MobyColDamageIn in;

  vector_subtract(mobToHitMoby, hitMoby->Position, moby->Position);
  vector_subtract(mobToJoint, jointPosition, moby->Position);
  vector_subtract(jointToHitMoby, hitMoby->Position, jointPosition);

  // ignore if hit behind
  if (vector_innerproduct(mobToHitMoby, mobToJoint) < 0)
    return 0;

  // ignore if past attack radius
  if (vector_innerproduct(mobToHitMoby, jointToHitMoby) > 0 && vector_sqrmag(jointToHitMoby) > sqrHitRadius)
    return 0;

  vector_write(in.Momentum, 0);
  in.Damager = moby;
  in.DamageFlags = damageFlags;
  in.DamageClass = 0;
  in.DamageStrength = 1;
  in.DamageIndex = moby->OClass;
  in.Flags = 1;
  in.DamageHp = amount;

  mobyCollDamageDirect(hitMoby, &in);
  return 1;
}

//--------------------------------------------------------------------------
int mobDoDamage(Moby* moby, float radius, float amount, int damageFlags, int friendlyFire, int jointId)
{
	VECTOR p, delta;
	MATRIX jointMtx;
  Player** players = playerGetAll();
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  int i;
  int result = 0;
  float sqrRadius = radius * radius;
  float firstPassSqrRadius = powf(5 + radius, 2);

  // get position of right spike joint
  mobyGetJointMatrix(moby, jointId, jointMtx);
  vector_copy(p, &jointMtx[12]);

  // if no friendly fire just check hit on players
  // otherwise check all mobys
  if (!friendlyFire) {
    for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
      Player* player = players[i];
      if (!player || !player->SkinMoby || playerIsDead(player))
        continue;

      vector_subtract(delta, player->PlayerPosition, p);
      if (vector_sqrmag(delta) > firstPassSqrRadius)
        continue;

      if (mobDoDamageTryHit(moby, player->PlayerMoby, p, sqrRadius, damageFlags, amount)) {
        result |= MOB_DO_DAMAGE_HIT_FLAG_HIT_PLAYER;
        if (player->PlayerMoby == pvars->MobVars.Target) result |= MOB_DO_DAMAGE_HIT_FLAG_HIT_TARGET;
      }
    }
  }
  else if (CollMobysSphere_Fix(p, COLLISION_FLAG_IGNORE_DYNAMIC, moby, NULL, 5 + radius) > 0) {
    Moby** hitMobies = CollMobysSphere_Fix_GetHitMobies();
    Moby* hitMoby;
    while ((hitMoby = *hitMobies++)) {
      if (mobDoDamageTryHit(moby, hitMoby, p, sqrRadius, damageFlags, amount)) {
        if (guberMobyGetPlayerDamager(hitMoby)) result |= MOB_DO_DAMAGE_HIT_FLAG_HIT_PLAYER;
        if (hitMoby == pvars->MobVars.Target) result |= MOB_DO_DAMAGE_HIT_FLAG_HIT_TARGET;
        if (mobyIsMob(hitMoby)) result |= MOB_DO_DAMAGE_HIT_FLAG_HIT_MOB;
      }
    }
  }

  return result;
}

//--------------------------------------------------------------------------
void mobSetAction(Moby* moby, int action)
{
	struct MobActionUpdateEventArgs args;
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;

	// don't set if already action
	if (pvars->MobVars.Action == action)
		return;

	GuberEvent* event = guberCreateEvent(moby, MOB_EVENT_STATE_UPDATE);
	if (event) {
		args.Action = action;
		args.ActionId = ++pvars->MobVars.ActionId;
		guberEventWrite(event, &args, sizeof(struct MobActionUpdateEventArgs));
	}
}

//--------------------------------------------------------------------------
void mobTransAnimLerp(Moby* moby, int animId, int lerpFrames, float startOff)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	if (moby->AnimSeqId != animId) {
		mobyAnimTransition(moby, animId, lerpFrames, startOff);

		pvars->MobVars.AnimationReset = 1;
		pvars->MobVars.AnimationLooped = 0;
	} else {
		
		// get current t
		// if our stored start is uninitialized, then set current t as start
		float t = moby->AnimSeqT;
		float end = *(u8*)((u32)moby->AnimSeq + 0x10) - (float)(lerpFrames * 0.5);
		if (t >= end && pvars->MobVars.AnimationReset) {
			pvars->MobVars.AnimationLooped++;
			pvars->MobVars.AnimationReset = 0;
		} else if (t < end) {
			pvars->MobVars.AnimationReset = 1;
		}
	}
}

//--------------------------------------------------------------------------
void mobTransAnim(Moby* moby, int animId, float startOff)
{
	mobTransAnimLerp(moby, animId, 10, startOff);
}

//--------------------------------------------------------------------------
int mobIsProjectileComing(Moby* moby)
{
  VECTOR t;
  Moby* m = NULL;
  Moby** mobys = (Moby**)0x0026BDA0;

  while ((m = *mobys++)) {
    if (!mobyIsDestroyed(m)) {
      switch (m->OClass)
      {
        case MOBY_ID_B6_BALL0:
        case MOBY_ID_ARBITER_ROCKET0:
        case MOBY_ID_DUAL_VIPER_SHOT:
        case MOBY_ID_MINE_LAUNCHER_MINE:
        {
          // projectile is within 5 units
          vector_subtract(t, moby->Position, m->Position);
          if (vector_sqrmag(t) < (7*7)) return 1;
          break;
        }
      }
    }
  }

  return 0;
}

//--------------------------------------------------------------------------
int mobHasVelocity(struct MobPVar* pvars)
{
	VECTOR t;
	vector_projectonhorizontal(t, pvars->MobVars.MoveVars.Velocity);
	return vector_sqrmag(t) >= 0.0001;
}

//--------------------------------------------------------------------------
void mobStand(Moby* moby)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  if (!pvars)
    return;

  // remove horizontal velocity
  vector_projectonvertical(pvars->MobVars.MoveVars.Velocity, pvars->MobVars.MoveVars.Velocity);
}

//--------------------------------------------------------------------------
int mobMoveCheck(Moby* moby, VECTOR outputPos, VECTOR from, VECTOR to)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  VECTOR delta, horizontalDelta, reflectedDelta;
  VECTOR hitTo, hitFrom;
  VECTOR hitToEx, hitNormal;
  VECTOR up = {0,0,0,0};
  float collRadius = pvars->MobVars.Config.CollRadius;
  if (!pvars)
    return 0;

  up[2] = collRadius; // 0.5;

  // offset hit scan to center of mob
  //vector_add(hitFrom, from, up);
  //vector_add(hitTo, to, up);
  
  // get horizontal delta between to and from
  vector_subtract(delta, to, from);
  vector_projectonhorizontal(horizontalDelta, delta);

  // offset hit scan to center of mob
  vector_add(hitFrom, from, up);
  vector_add(hitTo, hitFrom, horizontalDelta);
  
  vector_normalize(horizontalDelta, horizontalDelta);

  // move to further out to factor in the radius of the mob
  vector_normalize(hitToEx, delta);
  vector_scale(hitToEx, hitToEx, pvars->MobVars.Config.CollRadius);
  vector_add(hitTo, hitTo, hitToEx);
  vector_subtract(hitFrom, hitFrom, hitToEx);

  // check if we hit something
  if (CollLine_Fix(hitFrom, hitTo, COLLISION_FLAG_IGNORE_DYNAMIC, moby, NULL)) {

    vector_normalize(hitNormal, CollLine_Fix_GetHitNormal());

    // compute wall slope
    pvars->MobVars.MoveVars.WallSlope = getSignedSlope(horizontalDelta, hitNormal); //atan2f(1, vector_innerproduct(horizontalDelta, hitNormal)) - MATH_PI/2;
    pvars->MobVars.MoveVars.HitWall = 1;

    // check if we hit another mob
    Moby* hitMoby = pvars->MobVars.MoveVars.HitWallMoby = CollLine_Fix_GetHitMoby();
    if (hitMoby && mobyIsMob(hitMoby)) {
      pvars->MobVars.MoveVars.HitWall = 0;
      pvars->MobVars.MoveVars.HitWallMoby = NULL;
    }

#if DEBUGMOVE
    vector_copy(MoveCheckHit, CollLine_Fix_GetHitPosition());
#endif

    // if the hit point is before to
    // then we want to snap back before to
    vector_subtract(to, CollLine_Fix_GetHitPosition(), up);
    
    //vector_projectonhorizontal(hitToEx, hitToEx);
    vector_reflect(reflectedDelta, hitToEx, hitNormal);
    //if (reflectedDelta[2] > delta[2])
    //  reflectedDelta[2] = delta[2];

    vector_add(outputPos, to, reflectedDelta);

#if DEBUGMOVE
    vector_copy(MoveCheckFinal, outputPos);
#endif
    return 1;

    // reflect velocity on hit normal and add back to starting position
    vector_reflect(reflectedDelta, delta, hitNormal);
    //vectorProjectOnHorizontal(delta, delta);
    if (reflectedDelta[2] > delta[2])
      reflectedDelta[2] = delta[2];

    vector_add(outputPos, to, reflectedDelta);
    return 1;
  }

  vector_copy(outputPos, to);
  return 0;
}

//--------------------------------------------------------------------------
void mobMove(Moby* moby)
{
  VECTOR targetVelocity;
  VECTOR normalizedVelocity;
  VECTOR nextPos;
  VECTOR temp;
  VECTOR groundCheckFrom, groundCheckTo;
  VECTOR up = {0,0,1,0};
  int isMovingDown = 0;
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  int isOwner = mobAmIOwner(moby);

  u8 stuckCheckTicks = decTimerU8(&pvars->MobVars.MoveVars.StuckCheckTicks);
  u8 ungroundedTicks = decTimerU8(&pvars->MobVars.MoveVars.UngroundedTicks);
  u8 moveSkipTicks = decTimerU8(&pvars->MobVars.MoveVars.MoveSkipTicks);

  if (moveSkipTicks == 0) {

#if GATE
    gateSetCollision(0);
#endif

    // move next position to last position
    vector_copy(moby->Position, pvars->MobVars.MoveVars.NextPosition);
    vector_copy(pvars->MobVars.MoveVars.LastPosition, pvars->MobVars.MoveVars.NextPosition);

    // reset state
    pvars->MobVars.MoveVars.Grounded = 0;
    pvars->MobVars.MoveVars.WallSlope = 0;
    pvars->MobVars.MoveVars.HitWall = 0;
    pvars->MobVars.MoveVars.HitWallMoby = NULL;

#if DEBUGMOVE
    vector_write(MoveCheckHit, 0);
    vector_write(MoveCheckFinal, 0);
    vector_write(MoveCheckUp, 0);
    vector_write(MoveCheckDown, 0);
    vector_write(MoveNextPos, 0);
#endif

    if (1)
    {
      // add additive velocity
      vector_add(pvars->MobVars.MoveVars.Velocity, pvars->MobVars.MoveVars.Velocity, pvars->MobVars.MoveVars.AddVelocity);

      // compute simulated velocity by multiplying velocity by number of ticks to simulate
      vector_scale(targetVelocity, pvars->MobVars.MoveVars.Velocity, (float)(pvars->MobVars.MoveVars.MoveStep + 1));

      // get horizontal normalized velocity
      vector_normalize(normalizedVelocity, targetVelocity);
      normalizedVelocity[2] = 0;

      // compute next position
      vector_add(nextPos, moby->Position, targetVelocity);

      // move physics check twice to prevent clipping walls
      if (mobMoveCheck(moby, nextPos, moby->Position, nextPos)) {
        mobMoveCheck(moby, nextPos, moby->Position, nextPos);
      }

      // check ground or ceiling
      isMovingDown = vector_innerproduct(targetVelocity, up) < 0;
      if (isMovingDown) {
        vector_copy(groundCheckFrom, nextPos);
        groundCheckFrom[2] = maxf(moby->Position[2], nextPos[2]) + ZOMBIE_BASE_STEP_HEIGHT;
        vector_copy(groundCheckTo, nextPos);
        groundCheckTo[2] -= 0.5;
        if (CollLine_Fix(groundCheckFrom, groundCheckTo, COLLISION_FLAG_IGNORE_DYNAMIC, moby, NULL)) {
          // mark grounded this frame
          pvars->MobVars.MoveVars.Grounded = 1;

          // check if we've hit death barrier
          if (isOwner) {
            int hitId = CollLine_Fix_GetHitCollisionId() & 0x0F;
            if (hitId == 0x4 || hitId == 0xb || hitId == 0x0d) {
              pvars->MobVars.Respawn = 1;
            }
          }

          // force position to above ground
          vector_copy(nextPos, CollLine_Fix_GetHitPosition());
          nextPos[2] += 0.01;

#if DEBUGMOVE
          vector_copy(MoveCheckDown, CollLine_Fix_GetHitPosition());
#endif

          // remove vertical velocity from velocity
          vector_projectonhorizontal(pvars->MobVars.MoveVars.Velocity, pvars->MobVars.MoveVars.Velocity);
        }
      } else {
        vector_copy(groundCheckFrom, nextPos);
        groundCheckFrom[2] = moby->Position[2];
        vector_copy(groundCheckTo, nextPos);
        groundCheckTo[2] += 3;
        //groundCheckTo[2] += ZOMBIE_BASE_STEP_HEIGHT;
        if (CollLine_Fix(groundCheckFrom, groundCheckTo, COLLISION_FLAG_IGNORE_DYNAMIC, moby, NULL)) {
          // force position to below ceiling
          //vector_copy(nextPos, CollLine_Fix_GetHitPosition());
          //nextPos[2] -= 0.01;

#if DEBUGMOVE
          vector_copy(MoveCheckUp, CollLine_Fix_GetHitPosition());
#endif

          vector_copy(nextPos, CollLine_Fix_GetHitPosition());
          nextPos[2] = maxf(moby->Position[2], groundCheckTo[2] - 3);

          //vector_copy(nextPos, moby->Position);

          // remove vertical velocity from velocity
          //vectorProjectOnHorizontal(pvars->MobVars.MoveVars.Velocity, pvars->MobVars.MoveVars.Velocity);
        }
      }

#if DEBUGMOVE
      vector_copy(MoveNextPos, nextPos);
#endif

      // set position
      vector_copy(pvars->MobVars.MoveVars.NextPosition, nextPos);
    }

    // add gravity to velocity with clamp on downwards speed
    pvars->MobVars.MoveVars.Velocity[2] -= GRAVITY_MAGNITUDE * MATH_DT * (float)(pvars->MobVars.MoveVars.MoveStep + 1);
    if (pvars->MobVars.MoveVars.Velocity[2] < -10 * MATH_DT)
      pvars->MobVars.MoveVars.Velocity[2] = -10 * MATH_DT;

    // check if stuck by seeing if the sum horizontal delta position over the last second
    // is less than 1 in magnitude
    if (!stuckCheckTicks) {
      pvars->MobVars.MoveVars.StuckCheckTicks = 60;
      pvars->MobVars.MoveVars.IsStuck = pvars->MobVars.MoveVars.HitWall && vector_sqrmag(pvars->MobVars.MoveVars.SumPositionDelta) < (pvars->MobVars.MoveVars.SumSpeedOver * TPS * 0.25);
      if (!pvars->MobVars.MoveVars.IsStuck) {
        pvars->MobVars.MoveVars.StuckJumpCount = 0;
        pvars->MobVars.MoveVars.StuckTicks = 0;
      }

      // reset counters
      vector_write(pvars->MobVars.MoveVars.SumPositionDelta, 0);
      pvars->MobVars.MoveVars.SumSpeedOver = 0;
    }

    if (pvars->MobVars.MoveVars.IsStuck) {
      pvars->MobVars.MoveVars.StuckTicks++;
    }

    // add horizontal delta position to sum
    vector_subtract(temp, pvars->MobVars.MoveVars.NextPosition, pvars->MobVars.MoveVars.LastPosition);
    vector_projectonhorizontal(temp, temp);
    vector_add(pvars->MobVars.MoveVars.SumPositionDelta, pvars->MobVars.MoveVars.SumPositionDelta, temp);
    pvars->MobVars.MoveVars.SumSpeedOver += vector_sqrmag(temp);

    vector_write(pvars->MobVars.MoveVars.AddVelocity, 0);
    pvars->MobVars.MoveVars.MoveSkipTicks = pvars->MobVars.MoveVars.MoveStep;
  }

  float t = clamp(1 - (pvars->MobVars.MoveVars.MoveSkipTicks / (float)(pvars->MobVars.MoveVars.MoveStep + 1)), 0, 1);
  vector_lerp(moby->Position, pvars->MobVars.MoveVars.LastPosition, pvars->MobVars.MoveVars.NextPosition, t);

  mobForceIntoMapBounds(moby);
}

//--------------------------------------------------------------------------
void mobTurnTowards(Moby* moby, VECTOR towards, float turnSpeed)
{
  VECTOR delta;
  
  if (!moby || !moby->PVar)
    return;

	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  //float turnSpeed = pvars->MobVars.MoveVars.Grounded ? ZOMBIE_TURN_RADIANS_PER_SEC : ZOMBIE_TURN_AIR_RADIANS_PER_SEC;
  float radians = turnSpeed * pvars->MobVars.Config.Speed * MATH_DT;

  vector_subtract(delta, towards, moby->Position);
  float targetYaw = atan2f(delta[1], delta[0]);
  float yawDelta = clampAngle(targetYaw - moby->Rotation[2]);

  moby->Rotation[2] = clampAngle(moby->Rotation[2] + clamp(yawDelta, -radians, radians));
}

//--------------------------------------------------------------------------
void mobGetVelocityToTarget(Moby* moby, VECTOR velocity, VECTOR from, VECTOR to, float speed, float acceleration)
{
  VECTOR targetVelocity;
  VECTOR fromToTarget;
  VECTOR next, nextToTarget;
  VECTOR temp;
  float targetSpeed = speed * MATH_DT;

	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  if (!pvars)
    return;

  float collRadius = pvars->MobVars.Config.CollRadius + 0.5;
  
  // target velocity from rotation
  vector_fromyaw(targetVelocity, moby->Rotation[2]);

  // acclerate velocity towards target velocity
  //vector_normalize(targetVelocity, targetVelocity);
  vector_scale(targetVelocity, targetVelocity, targetSpeed);
  vector_subtract(temp, targetVelocity, velocity);
  vector_projectonhorizontal(temp, temp);
  vector_scale(temp, temp, acceleration * MATH_DT);
  vector_add(velocity, velocity, temp);
  
  // stop when at target
  if (pvars->MobVars.Target && targetSpeed > 0) {
    vector_subtract(fromToTarget, pvars->MobVars.Target->Position, from);
    float distToTarget = vector_length(fromToTarget);
    vector_add(next, from, velocity);
    vector_subtract(nextToTarget, pvars->MobVars.Target->Position, next);
    float distNextToTarget = vector_length(nextToTarget);
    
    float min = collRadius + PLAYER_COLL_RADIUS;
    float max = min + (targetSpeed * 0.2);

    // if too close to target, stop
    if (distNextToTarget < collRadius && vector_innerproduct_unscaled(fromToTarget, velocity) > 0) {
      vector_normalize(velocity, velocity);
      vector_scale(velocity, velocity, distToTarget - collRadius);
      return;
    }
  }

  if (targetSpeed <= 0) {
    vector_projectonvertical(velocity, velocity);
    return;
  }
}

//--------------------------------------------------------------------------
void mobPostDrawQuad(Moby* moby, int texId, u32 color, int jointId)
{
	struct QuadDef quad;
	float size = moby->Scale * 2;
	MATRIX m2;
	VECTOR pTL = {0,size,size,1};
	VECTOR pTR = {0,-size,size,1};
	VECTOR pBL = {0,size,-size,1};
	VECTOR pBR = {0,-size,-size,1};
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	if (!pvars)
		return;

	//u32 color = MobLODColors[pvars->MobVars.Config.MobType] | (moby->Opacity << 24);

	// set draw args
	matrix_unit(m2);

	// init
  gfxResetQuad(&quad);

	// color of each corner?
	vector_copy(quad.VertexPositions[0], pTL);
	vector_copy(quad.VertexPositions[1], pTR);
	vector_copy(quad.VertexPositions[2], pBL);
	vector_copy(quad.VertexPositions[3], pBR);
	quad.VertexColors[0] = quad.VertexColors[1] = quad.VertexColors[2] = quad.VertexColors[3] = color;
  quad.VertexUVs[0] = (struct UV){0,0};
  quad.VertexUVs[1] = (struct UV){1,0};
  quad.VertexUVs[2] = (struct UV){0,1};
  quad.VertexUVs[3] = (struct UV){1,1};
	quad.Clamp = 1;
	quad.Tex0 = gfxGetFrameTex(texId);
	quad.Tex1 = 0xFF9000000260;
	quad.Alpha = 0x8000000044;

	GameCamera* camera = cameraGetGameCamera(0);
	if (!camera)
		return;
	
	// set world matrix by joint
	mobyGetJointMatrix(moby, jointId, m2);

	// draw
	gfxDrawQuad((void*)0x00222590, &quad, m2, 1);
}

#if DEBUG

//--------------------------------------------------------------------------
void mobPostDrawDebug(Moby* moby)
{
#if PRINT_JOINTS
  MATRIX jointMtx;
  int i = 0;
  char buf[32];
  int animJointCount = 0;

  // get anim joint count
  void* pclass = moby->PClass;
  if (pclass) {
    animJointCount = **(u32**)((u32)pclass + 0x1C);
  }

  for (i = 0; i < animJointCount; ++i) {
    snprintf(buf, sizeof(buf), "%d", i);
    mobyGetJointMatrix(moby, i, jointMtx);
    draw3DMarker(&jointMtx[12], 0.5, 0x80FFFFFF, buf);
  }
#endif

#if DEBUGMOVE
  draw3DMarker(MoveCheckHit, 1, 0x80FFFFFF, "-");
  draw3DMarker(MoveCheckFinal, 1, 0x80FFFFFF, "+");
  draw3DMarker(MoveCheckUp, 1, 0x80FFFFFF, "^");
  draw3DMarker(MoveCheckDown, 1, 0x80FFFFFF, "v");
  draw3DMarker(MoveNextPos, 1, 0x80FFFFFF, "o");
#endif
}
#endif

//--------------------------------------------------------------------------
void mobPreUpdate(Moby* moby)
{
#if DEBUG
	gfxRegisterDrawFunction((void**)0x0022251C, (gfxDrawFuncDef*)&mobPostDrawDebug, moby);
#endif
}

//--------------------------------------------------------------------------
void mobOnSpawned(Moby* moby)
{
  if (!moby || !moby->PVar)
    return;
  
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  switch (moby->OClass)
  {
#if MOB_ZOMBIE
    case ZOMBIE_MOBY_OCLASS:
    {
      pvars->VTable = &ZombieVTable;
      break;
    }
#endif
#if MOB_EXECUTIONER
    case EXECUTIONER_MOBY_OCLASS:
    {
      pvars->VTable = &ExecutionerVTable;
      break;
    }
#endif
#if MOB_TREMOR
    case TREMOR_MOBY_OCLASS:
    {
      pvars->VTable = &TremorVTable;
      break;
    }
#endif
#if MOB_SWARMER
    case SWARMER_MOBY_OCLASS:
    {
      pvars->VTable = &SwarmerVTable;
      break;
    }
#endif
#if MOB_REACTOR
    case REACTOR_MOBY_OCLASS:
    {
      pvars->VTable = &ReactorVTable;
      break;
    }
#endif
#if MOB_REAPER
    case REAPER_MOBY_OCLASS:
    {
      pvars->VTable = &ReaperVTable;
      break;
    }
#endif
    default:
    {
      DPRINTF("unhandled mob spawned oclass:%04X\n", moby->OClass);
      break;
    }
  }
}

//--------------------------------------------------------------------------
void mobOnStateUpdate(Moby* moby, struct MobStateUpdateEventArgs* e)
{
  // update pathfinding state
  pathSetPath(moby, e->PathStartNodeIdx, e->PathEndNodeIdx, e->PathCurrentEdgeIdx, e->PathHasReachedStart, e->PathHasReachedEnd);
}

//--------------------------------------------------------------------------
void mobInit(void)
{
  MapConfig.OnMobSpawnedFunc = &mobOnSpawned;
}
