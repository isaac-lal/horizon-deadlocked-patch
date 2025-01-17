#include "include/game.h"
#include <libdl/math3d.h>
#include <libdl/utils.h>

const int UPGRADE_COST[] = {
	8000,			// v2
	12000,		// v3
	20000,		// v4
	40000,		// v5
	60000,		// v6
	90000,		// v7
	150000,		// v8
	220000,		// v9
	350000,		// v10
};

const float BOLT_TAX[] = {
  1.00,     // 0 players
	1.00,			// 1 player
	0.90,			// 2 players
	0.80,			// 3 players
	0.70,			// 4 players
	0.60,			// 5 players
	0.55,			// 6 players
	0.50,			// 7 players
	0.45,			// 8 players
	0.40,			// 9 players
	0.40,			// 10 players
};

// vanilla is 30 for each
const short WEAPON_PICKUP_BASE_RESPAWN_TIMES[] = {
	30, // VIPER
	30, // MAGMA
	30, // ARBITER
	30, // FUSION
	30, // MINES
	30, // B6
	30, // FLAIL
	30, // SHIELD
};

const short WEAPON_PICKUP_PLAYER_RESPAWN_TIME_OFFSETS[] = {
	0, // 1 player
	0, // 2 players
	5, // 3 players
	10, // 4 players
	12, // 5 players
	14, // 6 players
	16, // 7 players
	18, // 8 players
	21, // 9 players
	24, // 10 players
};

const int ENABLED_ALPHA_MODS[] = {
  ALPHA_MOD_SPEED,
  ALPHA_MOD_AMMO,
  ALPHA_MOD_IMPACT,
  ALPHA_MOD_AREA,
  ALPHA_MOD_JACKPOT,
  ALPHA_MOD_XP
};

const int ENABLED_ALPHA_MODS_COUNT = COUNT_OF(ENABLED_ALPHA_MODS);
