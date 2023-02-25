#include "../../../include/game.h"
#include "../../../include/mob.h"

extern struct SurvivalMapConfig MapConfig;


struct SurvivalSpecialRoundParam specialRoundParams[] = {
	// ROUND 5
	{
		.MaxSpawnedAtOnce = MAX_MOBS_SPAWNED,
		.SpawnParamCount = 2,
		.SpawnParamIds = {
			MOB_SPAWN_PARAM_RUNNER,
			MOB_SPAWN_PARAM_GHOST,
			-1, -1
		},
		.Name = "Ghost Runner Round"
	},
	// ROUND 10
	{
		.MaxSpawnedAtOnce = MAX_MOBS_SPAWNED,
		.SpawnParamCount = 3,
		.SpawnParamIds = {
			MOB_SPAWN_PARAM_EXPLOSION,
			MOB_SPAWN_PARAM_ACID,
			MOB_SPAWN_PARAM_FREEZE,
			-1
		},
		.Name = "Elemental Round"
	},
	// ROUND 15
	{
		.MaxSpawnedAtOnce = MAX_MOBS_SPAWNED,
		.SpawnParamCount = 2,
		.SpawnParamIds = {
			MOB_SPAWN_PARAM_RUNNER,
			MOB_SPAWN_PARAM_FREEZE,
			-1
		},
		.Name = "Freeze Runner Round"
	},
	// ROUND 20
	{
		.MaxSpawnedAtOnce = MAX_MOBS_SPAWNED,
		.SpawnParamCount = 3,
		.SpawnParamIds = {
			MOB_SPAWN_PARAM_GHOST,
			MOB_SPAWN_PARAM_EXPLOSION,
			MOB_SPAWN_PARAM_RUNNER,
			-1
		},
		.Name = "Evade Round"
	},
	// ROUND 25
	{
		.MaxSpawnedAtOnce = 10,
		.SpawnParamCount = 1,
		.SpawnParamIds = {
			MOB_SPAWN_PARAM_TITAN,
			-1
		},
		.Name = "Titan Round"
	},
};

const int specialRoundParamsCount = sizeof(specialRoundParams) / sizeof(struct SurvivalSpecialRoundParam);

// NOTE
// These must be ordered from least probable to most probable
struct MobSpawnParams defaultSpawnParams[] = {
	// executioner
	[MOB_SPAWN_PARAM_TITAN]
	{
		.Cost = 1000,
		.MinRound = 10,
		.CooldownTicks = TPS * 10,
		.Probability = 0.01,
		.SpawnType = SPAWN_TYPE_DEFAULT_RANDOM,
		.Name = "Titan",
		.Config = {
			.MobType = MOB_TANK,
			.Xp = 250,
			.Bangles = EXECUTIONER_BANGLE_LEFT_CHEST_PLATE | EXECUTIONER_BANGLE_RIGHT_CHEST_PLATE
                | EXECUTIONER_BANGLE_LEFT_COLLAR_BONE | EXECUTIONER_BANGLE_RIGHT_COLLAR_BONE
                | EXECUTIONER_BANGLE_HELMET | EXECUTIONER_BANGLE_BRAIN,
			.Damage = MOB_BASE_DAMAGE * 2.5,
			.MaxDamage = MOB_BASE_DAMAGE * 5,
			.Speed = MOB_BASE_SPEED * 0.85,
			.MaxSpeed = MOB_BASE_SPEED * 1.0,
			.Health = MOB_BASE_HEALTH * 50.0,
			.MaxHealth = 0,
			.Bolts = MOB_BASE_BOLTS * 10,
			.AttackRadius = EXECUTIONER_MELEE_ATTACK_RADIUS * 2.0,
			.HitRadius = EXECUTIONER_MELEE_HIT_RADIUS * 1.5,
      .CollRadius = EXECUTIONER_BASE_COLL_RADIUS * 4,
			.ReactionTickCount = EXECUTIONER_BASE_REACTION_TICKS * 0.35,
			.AttackCooldownTickCount = EXECUTIONER_BASE_ATTACK_COOLDOWN_TICKS * 1.5,
			.MaxCostMutation = 10,
			.MobSpecialMutation = 0,
		}
	},
	// ghost zombie
	[MOB_SPAWN_PARAM_GHOST]
	{
		.Cost = 10,
		.MinRound = 4,
		.CooldownTicks = TPS * 1,
		.Probability = 0.05,
		.SpawnType = SPAWN_TYPE_DEFAULT_RANDOM | SPAWN_TYPE_SEMI_NEAR_PLAYER | SPAWN_TYPE_NEAR_HEALTHBOX,
		.Name = "Ghost",
		.Config = {
			.MobType = MOB_GHOST,
			.Xp = 25,
			.Bangles = ZOMBIE_BANGLE_HEAD_2 | ZOMBIE_BANGLE_TORSO_2,
			.Damage = MOB_BASE_DAMAGE * 1.0,
			.MaxDamage = MOB_BASE_DAMAGE * 1.3,
			.Speed = MOB_BASE_SPEED * 1.0,
			.MaxSpeed = MOB_BASE_SPEED * 2.0,
			.Health = MOB_BASE_HEALTH * 1.0,
			.MaxHealth = 0,
			.Bolts = MOB_BASE_BOLTS * 1.5,
			.AttackRadius = ZOMBIE_MELEE_ATTACK_RADIUS,
			.HitRadius = ZOMBIE_MELEE_HIT_RADIUS,
      .CollRadius = ZOMBIE_BASE_COLL_RADIUS * 1.0,
			.ReactionTickCount = ZOMBIE_BASE_REACTION_TICKS,
			.AttackCooldownTickCount = ZOMBIE_BASE_ATTACK_COOLDOWN_TICKS,
			.MaxCostMutation = 2,
			.MobSpecialMutation = 0,
		}
	},
	// explode zombie
	[MOB_SPAWN_PARAM_EXPLOSION]
	{
		.Cost = 10,
		.MinRound = 6,
		.CooldownTicks = TPS * 2,
		.Probability = 0.08,
		.SpawnType = SPAWN_TYPE_SEMI_NEAR_PLAYER | SPAWN_TYPE_NEAR_PLAYER | SPAWN_TYPE_ON_PLAYER,
		.Name = "Explosion",
		.Config = {
			.MobType = MOB_EXPLODE,
			.Xp = 40,
			.Bangles = ZOMBIE_BANGLE_HEAD_1 | ZOMBIE_BANGLE_TORSO_1,
			.Damage = MOB_BASE_DAMAGE * 2.0,
			.MaxDamage = MOB_BASE_DAMAGE * 10.0,
			.Speed = MOB_BASE_SPEED * 1.0,
			.MaxSpeed = MOB_BASE_SPEED * 1.5,
			.Health = MOB_BASE_HEALTH * 2.0,
			.MaxHealth = 0,
			.Bolts = MOB_BASE_BOLTS * 1.0,
			.AttackRadius = ZOMBIE_MELEE_ATTACK_RADIUS,
			.HitRadius = ZOMBIE_EXPLODE_HIT_RADIUS,
      .CollRadius = ZOMBIE_BASE_COLL_RADIUS * 1.0,
			.ReactionTickCount = ZOMBIE_BASE_REACTION_TICKS,
			.AttackCooldownTickCount = ZOMBIE_BASE_ATTACK_COOLDOWN_TICKS,
			.MaxCostMutation = 2,
			.MobSpecialMutation = 0,
		}
	},
	// acid zombie
	[MOB_SPAWN_PARAM_ACID]
	{
		.Cost = 20,
		.MinRound = 10,
		.Probability = 0.09,
		.CooldownTicks = TPS * 1,
		.SpawnType = SPAWN_TYPE_SEMI_NEAR_PLAYER | SPAWN_TYPE_NEAR_PLAYER | SPAWN_TYPE_NEAR_HEALTHBOX,
		.Name = "Acid",
		.Config = {
			.MobType = MOB_ACID,
			.Xp = 50,
			.Bangles = ZOMBIE_BANGLE_HEAD_4 | ZOMBIE_BANGLE_TORSO_4,
			.Damage = MOB_BASE_DAMAGE * 1.0,
			.MaxDamage = MOB_BASE_DAMAGE * 1.0,
			.Speed = MOB_BASE_SPEED * 1.0,
			.MaxSpeed = MOB_BASE_SPEED * 1.8,
			.Health = MOB_BASE_HEALTH * 1.0,
			.Bolts = MOB_BASE_BOLTS * 1.2,
			.MaxHealth = 0,
			.AttackRadius = ZOMBIE_MELEE_ATTACK_RADIUS,
			.HitRadius = ZOMBIE_MELEE_HIT_RADIUS,
      .CollRadius = ZOMBIE_BASE_COLL_RADIUS * 1.0,
			.ReactionTickCount = ZOMBIE_BASE_REACTION_TICKS,
			.AttackCooldownTickCount = ZOMBIE_BASE_ATTACK_COOLDOWN_TICKS,
			.MaxCostMutation = 2,
			.MobSpecialMutation = 0,
		}
	},
	// freeze zombie
	[MOB_SPAWN_PARAM_FREEZE]
	{
		.Cost = 20,
		.MinRound = 8,
		.CooldownTicks = TPS * 1,
		.Probability = 0.1,
		.SpawnType = SPAWN_TYPE_SEMI_NEAR_PLAYER | SPAWN_TYPE_NEAR_PLAYER,
		.Name = "Freeze",
		.Config = {
			.MobType = MOB_FREEZE,
			.Xp = 50,
			.Bangles = ZOMBIE_BANGLE_HEAD_1 | ZOMBIE_BANGLE_TORSO_1,
			.Damage = MOB_BASE_DAMAGE * 1.0,
			.MaxDamage = MOB_BASE_DAMAGE * 1.6,
			.Speed = MOB_BASE_SPEED * 0.8,
			.MaxSpeed = MOB_BASE_SPEED * 1.5,
			.Health = MOB_BASE_HEALTH * 1.2,
			.MaxHealth = 0,
			.Bolts = MOB_BASE_BOLTS * 1.2,
			.AttackRadius = ZOMBIE_MELEE_ATTACK_RADIUS,
			.HitRadius = ZOMBIE_MELEE_HIT_RADIUS,
      .CollRadius = ZOMBIE_BASE_COLL_RADIUS * 1.0,
			.ReactionTickCount = ZOMBIE_BASE_REACTION_TICKS,
			.AttackCooldownTickCount = ZOMBIE_BASE_ATTACK_COOLDOWN_TICKS,
			.MaxCostMutation = 2,
			.MobSpecialMutation = 0,
		}
	},
	// runner zombie
	[MOB_SPAWN_PARAM_RUNNER]
	{
		.Cost = 10,
		.MinRound = 0,
		.CooldownTicks = 0,
		.Probability = 0.1,
		.SpawnType = SPAWN_TYPE_SEMI_NEAR_PLAYER | SPAWN_TYPE_NEAR_PLAYER,
		.Name = "Runner",
		.Config = {
			.MobType = MOB_RUNNER,
			.Xp = 15,
			.Bangles = TREMOR_BANGLE_HEAD | TREMOR_BANGLE_CHEST | TREMOR_BANGLE_LEFT_ARM,
			.Damage = MOB_BASE_DAMAGE * 0.4,
			.MaxDamage = MOB_BASE_DAMAGE * 0.8,
			.Speed = MOB_BASE_SPEED * 2.0,
			.MaxSpeed = MOB_BASE_SPEED * 3.0,
			.Health = MOB_BASE_HEALTH * 0.6,
			.MaxHealth = 0,
			.Bolts = MOB_BASE_BOLTS * 1.0,
			.AttackRadius = TREMOR_MELEE_ATTACK_RADIUS,
			.HitRadius = TREMOR_MELEE_HIT_RADIUS,
      .CollRadius = TREMOR_BASE_COLL_RADIUS * 1.0,
			.ReactionTickCount = TREMOR_BASE_REACTION_TICKS,
			.AttackCooldownTickCount = TREMOR_BASE_ATTACK_COOLDOWN_TICKS,
			.MaxCostMutation = 2,
			.MobSpecialMutation = 0,
		}
	},
	// normal zombie
	[MOB_SPAWN_PARAM_NORMAL]
	{
		.Cost = 5,
		.MinRound = 0,
		.CooldownTicks = 0,
		.Probability = 1.0,
		.SpawnType = SPAWN_TYPE_SEMI_NEAR_PLAYER | SPAWN_TYPE_NEAR_PLAYER,
		.Name = "Zombie",
		.Config = {
			.MobType = MOB_NORMAL,
			.Xp = 10,
			.Bangles = ZOMBIE_BANGLE_HEAD_1 | ZOMBIE_BANGLE_TORSO_1,
			.Damage = MOB_BASE_DAMAGE * 1.0,
			.MaxDamage = MOB_BASE_DAMAGE * 1.3,
			.Speed = MOB_BASE_SPEED * 1.0,
			.MaxSpeed = MOB_BASE_SPEED * 2.0,
			.Health = MOB_BASE_HEALTH * 1.0,
			.MaxHealth = 0,
			.Bolts = MOB_BASE_BOLTS * 1.0,
			.AttackRadius = ZOMBIE_MELEE_ATTACK_RADIUS,
			.HitRadius = ZOMBIE_MELEE_HIT_RADIUS,
      .CollRadius = ZOMBIE_BASE_COLL_RADIUS * 1.0,
			.ReactionTickCount = ZOMBIE_BASE_REACTION_TICKS,
			.AttackCooldownTickCount = ZOMBIE_BASE_ATTACK_COOLDOWN_TICKS,
			.MaxCostMutation = 2,
			.MobSpecialMutation = 0,
		}
	},
};

const int defaultSpawnParamsCount = sizeof(defaultSpawnParams) / sizeof(struct MobSpawnParams);


u32 MobPrimaryColors[] = {
	[MOB_NORMAL] 	0x00464443,
	[MOB_RUNNER] 	0x00464443,
	[MOB_FREEZE] 	0x00804000,
	[MOB_ACID] 		0x00464443,
	[MOB_EXPLODE] 0x00202080,
	[MOB_GHOST] 	0x00464443,
	[MOB_TANK]		0x00464443,
};

u32 MobSecondaryColors[] = {
	[MOB_NORMAL] 	0x80202020,
	[MOB_RUNNER] 	0x80202020,
	[MOB_FREEZE] 	0x80FF2000,
	[MOB_ACID] 		0x8000FF00,
	[MOB_EXPLODE] 0x000040C0,
	[MOB_GHOST] 	0x80202020,
	[MOB_TANK]		0x80FF2020,
};

u32 MobSpecialMutationColors[] = {
	[MOB_SPECIAL_MUTATION_FREEZE] 0xFFFF2000,
	[MOB_SPECIAL_MUTATION_ACID] 	0xFF00FF00,
};

u32 MobLODColors[] = {
	[MOB_NORMAL] 	0x00808080,
	[MOB_RUNNER] 	0x00808080,
	[MOB_FREEZE] 	0x00F08000,
	[MOB_ACID] 		0x0000F000,
	[MOB_EXPLODE] 0x004040F0,
	[MOB_GHOST] 	0x00464443,
	[MOB_TANK]		0x00808080,
};


void configInit(void)
{
  MapConfig.DefaultSpawnParams = defaultSpawnParams;
  MapConfig.DefaultSpawnParamsCount = defaultSpawnParamsCount;
  MapConfig.SpecialRoundParams = specialRoundParams;
  MapConfig.SpecialRoundParamsCount = specialRoundParamsCount;
}
