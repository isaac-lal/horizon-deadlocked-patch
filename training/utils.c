#include "include/utils.h"
#include "common.h"
#include <string.h>
#include <libdl/stdio.h>
#include <libdl/game.h>
#include <libdl/collision.h>
#include <libdl/moby.h>
#include <libdl/sound.h>
#include <libdl/random.h>
#include <libdl/spawnpoint.h>
#include <libdl/graphics.h>

/* 
 * Explosion sound def
 */
SoundDef ExplosionSoundDef =
{
	1000.0,		// MinRange
	1000.0,		// MaxRange
	2000,		// MinVolume
	2000,		// MaxVolume
	0,			// MinPitch
	0,			// MaxPitch
	0,			// Loop
	0x10,		// Flags
	0xF4,		// Index
	3			// Bank
};

Moby * spawnExplosion(VECTOR position, float size)
{
	// SpawnMoby_5025
	Moby * moby = ((Moby* (*)(u128, float, int, int, int, int, int, short, short, short, short, short, short,
				short, short, float, float, float, int, Moby *, int, int, int, int, int, int, int, int,
				int, short, Moby *, Moby *, u128)) (0x003c3b38))
				(vector_read(position), size / 2.5, 0x2, 0x14, 0x10, 0x10, 0x10, 0x10, 0x2, 0, 1, 0, 0,
				0, 0, 0, 0, 2, 0x00080800, 0, 0x00388EF7, 0x000063F7, 0x00407FFFF, 0x000020FF, 0x00008FFF, 0x003064FF, 0x7F60A0FF, 0x280000FF,
				0x003064FF, 0, 0, 0, 0);
				
	soundPlay(&ExplosionSoundDef, 0, moby, 0, 0x400);

	return moby;
}

void playTimerTickSound()
{
	((void (*)(Player*, int, int))0x005eb280)((Player*)0x347AA0, 0x3C, 0);
}

void patchVoidFallCameraBug(Player* player)
{
	// Fixes void fall bug
	*((u8*)0x00171DE0 + player->PlayerId) = 1;
	*(u32*)0x004DB88C = 0;
	player->CameraType2 = 2;
}

void unpatchVoidFallCameraBug(Player* player)
{
	*((u8*)0x00171DE0 + player->PlayerId) = 0;
	*(u32*)0x004DB88C = 0xA48200E0;
}

u8 decTimerU8(u8* timeValue)
{
	int value = *timeValue;
	if (value == 0)
		return 0;

	*timeValue = --value;
	return value;
}

u16 decTimerU16(u16* timeValue)
{
	int value = *timeValue;
	if (value == 0)
		return 0;

	*timeValue = --value;
	return value;
}

u32 decTimerU32(u32* timeValue)
{
	long value = *timeValue;
	if (value == 0)
		return 0;

	*timeValue = --value;
	return value;
}

//--------------------------------------------------------------------------
Player * playerGetRandom(void)
{
	int r = rand(GAME_MAX_PLAYERS);
	int i = 0, c = 0;
	Player ** players = playerGetAll();

	do {
		if (players[i] && !playerIsDead(players[i]))
			++c;
		
		++i;
		if (i == GAME_MAX_PLAYERS) {
			if (c == 0)
				return NULL;
			i = 0;
		}
	} while (c < r);

	return players[i-1];
}

//--------------------------------------------------------------------------
int spawnPointGetNearestTo(VECTOR point, VECTOR out, float minDist)
{
	VECTOR t;
	int spCount = spawnPointGetCount();
	int i;
	float bestPointDist = 100000;
	float minDistSqr = minDist * minDist;

	for (i = 0; i < spCount; ++i) {
		SpawnPoint* sp = spawnPointGet(i);
		vector_subtract(t, (float*)&sp->M0[12], point);
		float d = vector_sqrmag(t);
		if (d >= minDistSqr) {
			// randomize order a little
			d += randRange(0, 15 * 15);
			if (d < bestPointDist) {
				vector_copy(out, (float*)&sp->M0[12]);
				vector_fromyaw(t, randRadian());
				vector_scale(t, t, 3);
				vector_add(out, out, t);
				bestPointDist = d;
			}
		}
	}

	return spCount > 0;
}

//--------------------------------------------------------------------------
int spawnGetRandomPoint(VECTOR out) {
	// spawn randomly
	SpawnPoint* sp = spawnPointGet(rand(spawnPointGetCount()));
	if (sp) {
		vector_copy(out, &sp->M0[12]);
		return 1;
	}

	return 0;
}

//--------------------------------------------------------------------------
int getGadgetIdFromMoby(Moby* moby) {
	if (!moby)
		return -1;

	switch (moby->OClass)
	{
		case MOBY_ID_DUAL_VIPER_SHOT: return WEAPON_ID_VIPERS; break;
		case MOBY_ID_MAGMA_CANNON: return WEAPON_ID_MAGMA_CANNON; break;
		case MOBY_ID_ARBITER_ROCKET0: return WEAPON_ID_ARBITER; break;
		case MOBY_ID_FUSION_RIFLE:
		case MOBY_ID_FUSION_SHOT: return WEAPON_ID_FUSION_RIFLE; break;
		case MOBY_ID_MINE_LAUNCHER_MINE: return WEAPON_ID_MINE_LAUNCHER; break;
		case MOBY_ID_B6_BOMB_EXPLOSION: return WEAPON_ID_B6; break;
		case MOBY_ID_FLAIL: return WEAPON_ID_FLAIL; break;
		case MOBY_ID_HOLOSHIELD_LAUNCHER: return WEAPON_ID_OMNI_SHIELD; break;
		case MOBY_ID_WRENCH: return WEAPON_ID_WRENCH; break;
		default: return -1;
	}
}
