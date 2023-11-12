#ifndef SURVIVAL_MOB_SWARMER_H
#define SURVIVAL_MOB_SWARMER_H

#define SWARMER_BASE_REACTION_TICKS						(0.25 * TPS)
#define SWARMER_BASE_ATTACK_COOLDOWN_TICKS			(2 * TPS)
#define SWARMER_BASE_EXPLODE_RADIUS						(5)
#define SWARMER_MELEE_HIT_RADIUS								(1.75)
#define SWARMER_EXPLODE_HIT_RADIUS							(5)
#define SWARMER_MELEE_ATTACK_RADIUS						(5)

#define SWARMER_TARGET_KEEP_CURRENT_FACTOR     (10.0)

#define SWARMER_MAX_WALKABLE_SLOPE             (40 * MATH_DEG2RAD)
#define SWARMER_BASE_STEP_HEIGHT								(2)
#define SWARMER_TURN_RADIANS_PER_SEC           (45 * MATH_DEG2RAD)
#define SWARMER_TURN_AIR_RADIANS_PER_SEC       (15 * MATH_DEG2RAD)
#define SWARMER_MOVE_ACCELERATION              (25)
#define SWARMER_MOVE_AIR_ACCELERATION          (5)

#define SWARMER_ANIM_ATTACK_TICKS							(30)
#define SWARMER_TIMEBOMB_TICKS									(60 * 2)
#define SWARMER_FLINCH_COOLDOWN_TICKS					(60 * 7)
#define SWARMER_ACTION_COOLDOWN_TICKS					(30)
#define SWARMER_RESPAWN_AFTER_TICKS						(60 * 30)
#define SWARMER_BASE_COLL_RADIUS								(0.5)
#define SWARMER_MAX_COLL_RADIUS								(4)
#define SWARMER_AMBSND_MIN_COOLDOWN_TICKS    	(60 * 2)
#define SWARMER_AMBSND_MAX_COOLDOWN_TICKS    	(60 * 3)
#define SWARMER_FLINCH_PROBABILITY             (1.0)

enum SwarmerAnimId
{
	SWARMER_ANIM_IDLE,
	SWARMER_ANIM_IDLE_LOOK_LEFT_RIGHT,
	SWARMER_ANIM_IDLE_JUMP,
	SWARMER_ANIM_IDLE_JUMP2,
	SWARMER_ANIM_WALK,
	SWARMER_ANIM_WADDLE,
	SWARMER_ANIM_BITE,
	SWARMER_ANIM_JUMP_FORWARD_BITE,
	SWARMER_ANIM_JUMP_BACK,
	SWARMER_ANIM_ALERT,
	SWARMER_ANIM_JUMP,
	SWARMER_ANIM_SIDE_FLIP,
	SWARMER_ANIM_ROAR,
	SWARMER_ANIM_JUMP_AND_FALL,
	SWARMER_ANIM_FLINCH_BACKFLIP_AND_STAND,
	SWARMER_ANIM_FLINCH_SPIN_AND_STAND,
	SWARMER_ANIM_FLINCH_SPIN_AND_STAND2
};

enum SwarmerAction
{
	SWARMER_ACTION_SPAWN,
	SWARMER_ACTION_IDLE,
	SWARMER_ACTION_JUMP,
	SWARMER_ACTION_WALK,
	SWARMER_ACTION_FLINCH,
	SWARMER_ACTION_BIG_FLINCH,
	SWARMER_ACTION_LOOK_AT_TARGET,
  SWARMER_ACTION_DIE,
	SWARMER_ACTION_ATTACK,
	SWARMER_ACTION_TIME_BOMB,
  SWARMER_ACTION_TIME_BOMB_EXPLODE
};

#endif // SURVIVAL_MOB_SWARMER_H
