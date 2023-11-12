#ifndef SURVIVAL_MOB_TREMOR_H
#define SURVIVAL_MOB_TREMOR_H

#define TREMOR_BASE_REACTION_TICKS						(0.25 * TPS)
#define TREMOR_BASE_ATTACK_COOLDOWN_TICKS			(1 * TPS)
#define TREMOR_BASE_EXPLODE_RADIUS						(5)
#define TREMOR_MELEE_HIT_RADIUS								(1.75)
#define TREMOR_MELEE_ATTACK_RADIUS						(5)

#define TREMOR_TARGET_KEEP_CURRENT_FACTOR     (10)

#define TREMOR_MAX_WALKABLE_SLOPE             (50 * MATH_DEG2RAD)
#define TREMOR_BASE_STEP_HEIGHT								(3)
#define TREMOR_TURN_RADIANS_PER_SEC           (90 * MATH_DEG2RAD)
#define TREMOR_TURN_AIR_RADIANS_PER_SEC       (15 * MATH_DEG2RAD)
#define TREMOR_MOVE_ACCELERATION              (40)
#define TREMOR_MOVE_AIR_ACCELERATION          (20)

#define TREMOR_ANIM_ATTACK_TICKS							(30)
#define TREMOR_FLINCH_COOLDOWN_TICKS					(60 * 7)
#define TREMOR_ACTION_COOLDOWN_TICKS					(30)
#define TREMOR_RESPAWN_AFTER_TICKS						(60 * 30)
#define TREMOR_BASE_COLL_RADIUS								(0.5)
#define TREMOR_MAX_COLL_RADIUS								(4)
#define TREMOR_AMBSND_MIN_COOLDOWN_TICKS    	(60 * 2)
#define TREMOR_AMBSND_MAX_COOLDOWN_TICKS    	(60 * 3)
#define TREMOR_FLINCH_PROBABILITY             (1.0)

enum TremorAnimId
{
	TREMOR_ANIM_IDLE,
	TREMOR_ANIM_IDLE_CLEAN_SPIKE,
	TREMOR_ANIM_IDLE_LOOK_AROUND,
	TREMOR_ANIM_IDLE_READY,
	TREMOR_ANIM_LAUGHING,
	TREMOR_ANIM_RUN,
	TREMOR_ANIM_WALK,
	TREMOR_ANIM_JUMP,
	TREMOR_ANIM_SIDE_FLIP_RIGHT,
	TREMOR_ANIM_SIDE_FLIP_LEFT,
	TREMOR_ANIM_BACK_FLIP,
	TREMOR_ANIM_SWING,
	TREMOR_ANIM_JUMP_AND_STAB_DOWN,
	TREMOR_ANIM_STAB_DOWN_POSITION_REPEAT,
	TREMOR_ANIM_DANCE_SPIN_THING,
	TREMOR_ANIM_ATTACK_COMBO_2,
	TREMOR_ANIM_THROW,
	TREMOR_ANIM_FLINCH,
	TREMOR_ANIM_SPAWN,
	TREMOR_ANIM_FLINCH_FALL_GET_UP,
	TREMOR_ANIM_FLINCH_BACK_FLIP_FALL,
};

enum TremorBangles {
	TREMOR_BANGLE_HEAD =  	          (1 << 0),
	TREMOR_BANGLE_HEAD_ARMOR =  	    (1 << 1),
	TREMOR_BANGLE_CHEST =  	          (1 << 2),
	TREMOR_BANGLE_CHEST_ARMOR =  	    (1 << 3),
	TREMOR_BANGLE_LEFT_ARM =  	      (1 << 4),
	TREMOR_BANGLE_LEFT_ARM_ARMOR =  	(1 << 5),
	TREMOR_BANGLE_HIDE_BODY =	        (1 << 15)
};

enum TremorAction
{
	TREMOR_ACTION_SPAWN,
	TREMOR_ACTION_IDLE,
	TREMOR_ACTION_JUMP,
	TREMOR_ACTION_WALK,
	TREMOR_ACTION_FLINCH,
	TREMOR_ACTION_BIG_FLINCH,
	TREMOR_ACTION_LOOK_AT_TARGET,
  TREMOR_ACTION_DIE,
	TREMOR_ACTION_ATTACK,
};

#endif // SURVIVAL_MOB_TREMOR_H
