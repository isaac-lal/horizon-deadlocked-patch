/***************************************************
 * FILENAME :		main.c
 * 
 * DESCRIPTION :
 * 		SURVIVAL.
 * 
 * NOTES :
 * 		Each offset is determined per app id.
 * 		This is to ensure compatibility between versions of Deadlocked/Gladiator.
 * 		
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */

#include <tamtypes.h>
#include <string.h>

#include <libdl/time.h>
#include <libdl/game.h>
#include <libdl/gamesettings.h>
#include <libdl/player.h>
#include <libdl/weapon.h>
#include <libdl/hud.h>
#include <libdl/cheats.h>
#include <libdl/sha1.h>
#include <libdl/dialog.h>
#include <libdl/ui.h>
#include <libdl/stdio.h>
#include <libdl/stdlib.h>
#include <libdl/graphics.h>
#include <libdl/spawnpoint.h>
#include <libdl/random.h>
#include <libdl/net.h>
#include <libdl/sound.h>
#include <libdl/dl.h>
#include <libdl/utils.h>
#include <libdl/collision.h>
#include <libdl/radar.h>
#include "module.h"
#include "messageid.h"
#include "config.h"
#include "include/mob.h"
#include "include/drop.h"
#include "include/bubble.h"
#include "include/game.h"
#include "include/utils.h"
#include "include/gate.h"

#define DIFFICULTY_FACTOR                   (State.MobStats.TotalSpawned / 100)
#define SPAWNPOINT_NEAR_BUFFER_SIZE         (3)

const char * SURVIVAL_ROUND_COMPLETE_MESSAGE = "Round %d Complete!";
const char * SURVIVAL_ROUND_START_MESSAGE = "Round %d";
const char * SURVIVAL_NEXT_ROUND_BEGIN_SKIP_MESSAGE = "\x1d   Start Round";
const char * SURVIVAL_NEXT_ROUND_WAIT_FOR_HOST_MESSAGE = "Waiting for Host";
const char * SURVIVAL_NEXT_ROUND_TIMER_MESSAGE = "Next Round";
const char * SURVIVAL_GAME_OVER = "GAME OVER";
const char * SURVIVAL_HEALTH_GUN = "Health Tornado";
const char * SURVIVAL_REVIVE_MESSAGE = "\x1c (DOWN) Revive %s";
const char * SURVIVAL_UPGRADE_MESSAGE = "\x11 Upgrade [\x0E%'d\x08]";
const char * SURVIVAL_OPEN_WEAPONS_MESSAGE = "\x12 Manage Mods";
const char * SURVIVAL_INTERACT_BANK_BALANCE_MESSAGE = "Balance \x0A%'d\x08";
const char * SURVIVAL_INTERACT_BANK_INTERACT_MESSAGE = "\x11 Deposit \x13 Withdraw";
const char * SURVIVAL_PRESTIGE_WEAPON_MESSAGE = "\x11 Prestige [\x0E%'d\x08]";
const char * SURVIVAL_PRESTIGE_WEAPON_NEED_V10_MESSAGE = "Your weapon is not powerful enough";
const char * SURVIVAL_PRESTIGE_WEAPON_MAXED_MESSAGE = "Your weapon is too powerful";
const char * SURVIVAL_MAXED_OUT_UPGRADE_MESSAGE = "Maxed Out";
const char * SURVIVAL_BUY_UPGRADE_MESSAGES[] = {
	[UPGRADE_HEALTH] "\x11 Health Upgrade (%d)",
	[UPGRADE_SPEED] "\x11 Speed Upgrade (%d)",
	[UPGRADE_DAMAGE] "\x11 Damage Upgrade (%d)",
	[UPGRADE_MEDIC] "\x11 Revive Discount (%d)",
	[UPGRADE_VENDOR] "\x11 Vendor Discount (%d)",
	[UPGRADE_PICKUPS] "\x11 Increase Powerup Duration (%d)",
  [UPGRADE_CRIT] "\x11 Critical Hit Upgrade (%d)",
};

const char * ALPHA_MODS[] = {
	"",
	"Speed Mod",
	"Ammo Mod",
	"Aiming Mod",
	"Impact Mod",
	"Area Mod",
	"XP Mod",
	"Jackpot Mod",
	"Nanoleech Mod"
};

const char WEAPON_PRESTIGE_PREFIX[WEAPON_PRESTIGE_MAX+1] = {
  [0] '\x08',
  [1] '\x09',
  [2] '\x0A',
  [3] '\x0B',
  [4] '\x0F',
  [5] '\x0E',
};

const int WEAPON_PRESTIGE_COST[WEAPON_PRESTIGE_MAX] = {
  [0] 100000,
  [1] 200000,
  [2] 400000,
  [3] 700000,
  [4] 1000000,
};

const u8 UPGRADEABLE_WEAPONS[] = {
	WEAPON_ID_VIPERS,
	WEAPON_ID_MAGMA_CANNON,
	WEAPON_ID_ARBITER,
	WEAPON_ID_FUSION_RIFLE,
	WEAPON_ID_MINE_LAUNCHER,
	WEAPON_ID_B6,
	WEAPON_ID_OMNI_SHIELD,
	WEAPON_ID_FLAIL
};

char UpgradesEnabled[] = {
  UPGRADE_HEALTH,
  UPGRADE_SPEED,
  UPGRADE_DAMAGE,
  UPGRADE_CRIT
};

int UpgradeMax[] = {
	[UPGRADE_HEALTH] 1000,
	[UPGRADE_SPEED] 40,
	[UPGRADE_DAMAGE] 1000,
  [UPGRADE_CRIT] 100,
	[UPGRADE_MEDIC] 16,
	[UPGRADE_VENDOR] 16,
	[UPGRADE_PICKUPS] 33,
};

#if FIXEDTARGET
Moby* FIXEDTARGETMOBY = NULL;
#endif

char LocalPlayerStrBuffer[2][64];

int BoltCounts[GAME_MAX_LOCALS] = {};

int Initialized = 0;
int FirstTimeInitialized = 0;

struct SurvivalState State;
struct SurvivalMapConfig* mapConfig = (struct SurvivalMapConfig*)0x01EF0010;

struct SurvivalSnackItem snackItems[SNACK_ITEM_MAX_COUNT] = {};
int snackItemsCount = 0;

int defaultSpawnParamsCooldowns[MAX_MOB_SPAWN_PARAMS] = {};
u8 mobPlaySoundCooldownTicks[MAX_MOB_SPAWN_PARAMS][MOBS_PLAY_SOUND_COOLDOWN_MAX_SOUNDIDS] = {};

PatchConfig_t* playerConfig = NULL;

int playerStates[GAME_MAX_PLAYERS] = {};
int playerStateTimers[GAME_MAX_PLAYERS] = {};

SoundDef TestSoundDef =
{
	0.0,	  // MinRange
	50.0,	  // MaxRange
	10,		  // MinVolume
	1000,		// MaxVolume
	0,			// MinPitch
	0,			// MaxPitch
	0,			// Loop
	0x10,		// Flags
	0xF5,		// Index
	3			  // Bank
};

struct CustomDzoCommandSurvivalDrawHud
{
  int Tokens;
  int EnemiesAlive;
  int CurrentRoundNumber;
  int HeldItem;
  char Blessings[4];
  int StartRoundTimer;
  float XpPercent;
  char HasRoundCompleteMessage;
  char HasDblPoints;
  char HasDblXp;
  char WaitingForHost;
  char RoundCompleteMessage[64];
  int Timer;
} dzoDrawHudCmd;

struct CustomDzoCommandSurvivalDrawReviveMsg
{
  VECTOR Position;
  int Seconds;
  int PlayerIdx;
};

void _getLocalBolts(void);
void dzoDrawReviveMsg(int playerId, VECTOR wsPosition, int seconds);

//--------------------------------------------------------------------------
void dzoDrawReviveMsg(int playerId, VECTOR wsPosition, int seconds)
{
  struct CustomDzoCommandSurvivalDrawReviveMsg cmd;
  if (!PATCH_DZO_INTEROP_FUNCS)
    return;

  vector_copy(cmd.Position, wsPosition);
  cmd.Seconds = seconds;
  cmd.PlayerIdx = playerId;
  PATCH_DZO_INTEROP_FUNCS->SendCustomCommandToClient(CUSTOM_DZO_CMD_ID_SURVIVAL_DRAW_REVIVE_MSG, sizeof(cmd), &cmd);
}

//--------------------------------------------------------------------------
void updateDzoHud(void)
{
  if (!PATCH_DZO_INTEROP_FUNCS)
    return;

  int gameTime = gameGetTime();

  dzoDrawHudCmd.Tokens = State.LocalPlayerState->State.CurrentTokens;
  dzoDrawHudCmd.HeldItem = State.LocalPlayerState->State.Item;
  memcpy(dzoDrawHudCmd.Blessings, State.LocalPlayerState->State.ItemBlessings, 4);
  dzoDrawHudCmd.CurrentRoundNumber = State.RoundNumber;
  dzoDrawHudCmd.EnemiesAlive = (State.RoundMaxMobCount - State.MobStats.TotalSpawnedThisRound) + State.MobStats.TotalAlive + State.MobStats.TotalSpawning; //State.MobStats.TotalAlive;
  dzoDrawHudCmd.StartRoundTimer = State.RoundEndTime - gameTime;
  dzoDrawHudCmd.WaitingForHost = State.RoundCompleteTime && State.RoundEndTime < 0;
  dzoDrawHudCmd.HasDblPoints = State.LocalPlayerState->IsDoublePoints;
  dzoDrawHudCmd.HasDblXp = State.LocalPlayerState->IsDoubleXP;
  dzoDrawHudCmd.XpPercent = State.LocalPlayerState->State.XP / (float)getXpForNextToken(State.LocalPlayerState->State.TotalTokens);
  dzoDrawHudCmd.Timer = gameTime - State.InitializedTime;
  PATCH_DZO_INTEROP_FUNCS->SendCustomCommandToClient(CUSTOM_DZO_CMD_ID_SURVIVAL_DRAW_HUD, sizeof(dzoDrawHudCmd), &dzoDrawHudCmd);

  // reset
  dzoDrawHudCmd.HasRoundCompleteMessage = 0;
}

//--------------------------------------------------------------------------
void setPlayerEXP(int localPlayerIndex, float expPercent)
{
	Player* player = playerGetFromSlot(localPlayerIndex);
	if (!player || !player->PlayerMoby)
		return;

	u32 canvasId = localPlayerIndex; //hudGetCurrentCanvas() + localPlayerIndex;
	void* canvas = hudGetCanvas(canvasId);
	if (!canvas)
		return;

	struct HUDWidgetRectangleObject* expBar = (struct HUDWidgetRectangleObject*)hudCanvasGetObject(canvas, 0xF000010A);
	if (!expBar) {
		//DPRINTF("no exp bar %d canvas:%d\n", localPlayerIndex, canvasId);
    return;
  }
	
	// set exp bar
	expBar->iFrame.ScaleX = 0.2275 * clamp(expPercent, 0, 1);
	expBar->iFrame.PositionX = 0.406 + (expBar->iFrame.ScaleX / 2);
	expBar->iFrame.PositionY = 0.0546875;
	expBar->iFrame.Color = hudGetTeamColor(player->Team, 2);
	expBar->Color1 = hudGetTeamColor(player->Team, 0);
	expBar->Color2 = hudGetTeamColor(player->Team, 2);
	expBar->Color3 = hudGetTeamColor(player->Team, 0);

	struct HUDWidgetTextObject* healthText = (struct HUDWidgetTextObject*)hudCanvasGetObject(canvas, 0xF000010E);
	if (!healthText) {
		//DPRINTF("no health text %d\n", localPlayerIndex);
    return;
  }
	
	// set health string
	snprintf(State.PlayerStates[player->PlayerId].HealthBarStrBuf, sizeof(State.PlayerStates[player->PlayerId].HealthBarStrBuf), "%d/%d", (int)player->Health, (int)player->MaxHealth);
	healthText->ExternalStringMemory = State.PlayerStates[player->PlayerId].HealthBarStrBuf;
}

//--------------------------------------------------------------------------
void setPlayerWeaponsMenu(int localPlayerIndex)
{
  static int has[GAME_MAX_LOCALS] = {0,0,0,0};
	Player* player = playerGetFromSlot(localPlayerIndex);
	if (!player || !player->PlayerMoby)
		return;

  u32 startCanvas = hudGetCurrentCanvas();
	u32 canvasId = localPlayerIndex; //hudGetCurrentCanvas() + localPlayerIndex;
	void* canvas = hudGetCanvas(canvasId);
	if (!canvas)
		return;

  int addrs[] = {
    0x00222C40,
    0x00222C48,
  };

  int isOpen = hudCanvasGetObject(canvas, hudPanelGetElement((void*)addrs[0], 0));
  if (!isOpen) {
    has[localPlayerIndex] = 0;
    return;
  } else if (has[localPlayerIndex]) {
    return;
  }

  hudSetCurrentCanvas(canvasId);

  int i,j;
  for (j = 0; j < 2; ++j) {
    int addr = addrs[j];
      
    for (i = -1; i < 256; ++i) {
      u32 id = hudPanelGetElement((void*)addr, i);
      struct HUDFrameObject* frame = (struct HUDFrameObject*)hudCanvasGetObject(canvas, id);

      if (frame) {
        
        float sx,sy,px,py;
        hudElementGetScale(id, &sx, &sy);
        hudElementGetPosition(id, &px, &py);

        // squish vertically
        sy *= 0.5;
        py *= 0.5;
        if (j == 1) {
          if (i == 0) {
            sy *= 2;
          }

          py += 0.11;
        }

        hudElementSetScale(id, sx, sy);
        hudElementSetPosition(id, px, py);
      }
    }
  }

  hudSetCurrentCanvas(startCanvas);
  has[localPlayerIndex] = 1;
}

//--------------------------------------------------------------------------
void uiShowLowerPopup(int localPlayerIdx, int msgStringId)
{
	((void (*)(int, int, int))0x0054ea30)(localPlayerIdx, msgStringId, 0);
}

//--------------------------------------------------------------------------
void popSnack(void)
{
  memmove(&snackItems[0], &snackItems[1], sizeof(struct SurvivalSnackItem) * (SNACK_ITEM_MAX_COUNT-1));
  memset(&snackItems[SNACK_ITEM_MAX_COUNT-1], 0, sizeof(struct SurvivalSnackItem));

  if (snackItemsCount)
    --snackItemsCount;
}

//--------------------------------------------------------------------------
void pushSnack(char * str, int ticksAlive, int localPlayerIdx)
{
  while (snackItemsCount >= SNACK_ITEM_MAX_COUNT) {
    popSnack();
  }

  // clamp
  if (snackItemsCount < 0)
    snackItemsCount = 0;

  strncpy(snackItems[snackItemsCount].Str, str, sizeof(snackItems[snackItemsCount].Str));
  snackItems[snackItemsCount].TicksAlive = ticksAlive;
  snackItems[snackItemsCount].DisplayForLocalPlayerIdx = localPlayerIdx;
  ++snackItemsCount;
}

//--------------------------------------------------------------------------
void drawSnack(void)
{
  if (snackItemsCount <= 0)
    return;

  // draw
  //uiShowPopup(0, snackItems[0].Str);
  char* a = uiMsgString(0x2400);
  strncpy(a, snackItems[0].Str, sizeof(snackItems[0].Str));

  if (snackItems[0].DisplayForLocalPlayerIdx <= 0)
    uiShowLowerPopup(0, 0x2400);
  if (snackItems[0].DisplayForLocalPlayerIdx < 0 || snackItems[0].DisplayForLocalPlayerIdx == 1)
    uiShowLowerPopup(1, 0x2400);

  snackItems[0].TicksAlive--;

  // remove when dead
  if (snackItems[0].TicksAlive <= 0)
    popSnack();
}

//--------------------------------------------------------------------------
struct GuberMoby* getGuber(Moby* moby)
{
  if (!moby) return NULL;

	if (moby->OClass == UPGRADE_MOBY_OCLASS && moby->PVar)
		return moby->GuberMoby;
	if (moby->OClass == DROP_MOBY_OCLASS && moby->PVar)
		return moby->GuberMoby;
	if (moby->OClass == DEMONBELL_MOBY_OCLASS && moby->PVar)
		return moby->GuberMoby;
  if (mobyIsMob(moby))
    return moby->GuberMoby;
	
	return 0;
}

//--------------------------------------------------------------------------
int handleEvent(Moby* moby, GuberEvent* event)
{
	if (!moby || !event || !isInGame())
		return 0;

  if (mobyIsMob(moby))
    return mobHandleEvent(moby, event);

	switch (moby->OClass)
	{
		case DROP_MOBY_OCLASS: if (!mapConfig || !mapConfig->OnMobDropEventFunc) { return 0; } return mapConfig->OnMobDropEventFunc(moby, event);
		case UPGRADE_MOBY_OCLASS: if (!mapConfig || !mapConfig->OnUpgradePickupEventFunc) { return 0; } return mapConfig->OnUpgradePickupEventFunc(moby, event);
    case DEMONBELL_MOBY_OCLASS: return demonbellHandleEvent(moby, event);
	}

	return 0;
}

//--------------------------------------------------------------------------
int shouldDrawHud(void)
{
  PlayerHUDFlags* hudFlags = hudGetPlayerFlags(0);
  return hudFlags && hudFlags->Flags.Raw != 0;
}

//--------------------------------------------------------------------------
void drawRoundMessage(const char * message, float scale, int yPixelsOffset)
{
	float x = 0.5;
	float y = 0.16;

  // move to dzo
  dzoDrawHudCmd.HasRoundCompleteMessage = 1;
  strncpy(dzoDrawHudCmd.RoundCompleteMessage, message, sizeof(dzoDrawHudCmd.RoundCompleteMessage));

	// draw message
	y *= SCREEN_HEIGHT;
	x *= SCREEN_WIDTH;
	gfxScreenSpaceText(x, y + 5 + yPixelsOffset, scale, scale * 1.5, 0x80FFFFFF, message, -1, 1);
}

//--------------------------------------------------------------------------
void drawTimer(int time)
{
  char buf[32];

  if (time < 0) time = 0;
  snprintf(buf, sizeof(buf), "%02d:%02d", time / TIME_MINUTE, (time % TIME_MINUTE) / TIME_SECOND);
  gfxScreenSpaceText(31, 211, 0.9, 0.9, 0x40000000, buf, -1, 1);
  gfxScreenSpaceText(30, 210, 0.9, 0.9, 0x80E0E0E0, buf, -1, 1);
}

//--------------------------------------------------------------------------
void openWeaponsMenu(int localPlayerIndex)
{
	((void (*)(int, int))0x00544748)(localPlayerIndex, 4);
	((void (*)(int, int))0x005415c8)(localPlayerIndex, 2);

	((void (*)(int))0x00543e10)(localPlayerIndex); // hide player
	int s = ((int (*)(int))0x00543648)(localPlayerIndex); // get hud enter state
	((void (*)(int))0x005c2370)(s); // swapto
}

//--------------------------------------------------------------------------
Moby * mobyGetRandomHealthbox(void)
{
	Moby* results[10];
	int count = 0;
	Moby* start = mobyListGetStart();
	Moby* end = mobyListGetEnd();

	while (start < end && count < 10) {
		if (start->OClass == MOBY_ID_HEALTH_BOX_MULT && !mobyIsDestroyed(start))
			results[count++] = start;

		++start;
	}

	if (count > 0)
		return results[rand(count)];

	return NULL;
}

//--------------------------------------------------------------------------
Player * playerGetRandom(void)
{
	int r = rand(GAME_MAX_PLAYERS);
	int i = 0, c = 0;
	Player ** players = playerGetAll();

	do {
		if (players[i] && !playerIsDead(players[i]) && players[i]->Health > 0)
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
void getResurrectPoint(Player* player, VECTOR outPos, VECTOR outRot, int firstRes)
{
	int i;
  VECTOR t;

  // spawn at player start
  SurvivalBakedConfig_t* bakedConfig = mapConfig->BakedConfig;
  if (bakedConfig) {
    for (i = 0; i < BAKED_SPAWNPOINT_COUNT; ++i) {
      if (bakedConfig->BakedSpawnPoints[i].Type == BAKED_SPAWNPOINT_PLAYER_START) {

        memcpy(outPos, bakedConfig->BakedSpawnPoints[i].Position, 12);
        memcpy(outRot, bakedConfig->BakedSpawnPoints[i].Rotation, 12);

        float theta = player->PlayerId / (float)GAME_MAX_PLAYERS;
        vector_fromyaw(t, theta * MATH_PI * 2);
        vector_scale(t, t, 2.5);
        vector_add(outPos, outPos, t);
        return;
      }
    }
  }

  // pass to base if we don't have a player start
  playerGetSpawnpoint(player, outPos, outRot, firstRes);
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
    if (!spawnPointIsPlayer(i))
      continue;
      
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
int spawnPointGetNearToPlayer(VECTOR out, float minDist)
{
	VECTOR t;
	int spCount = spawnPointGetCount();
	int i,j;
  int found = 0;
	float bestPointDistSqr[SPAWNPOINT_NEAR_BUFFER_SIZE] = {100000,100000,100000};
  int bestPoints[SPAWNPOINT_NEAR_BUFFER_SIZE] = {-1,-1,-1};
	float minDistSqr = minDist * minDist;
  float closestDistSqr = 0;
  Player** players = playerGetAll();

	for (i = 0; i < spCount; ++i) {
    if (!spawnPointIsPlayer(i))
      continue;

		SpawnPoint* sp = spawnPointGet(i);
    closestDistSqr = 1000000;

    // get closest sqr dist to any player
    for (j = 0; j < GAME_MAX_PLAYERS; ++j) {
      if (!players[j] || !players[j]->SkinMoby)
        continue;

      vector_subtract(t, (float*)&sp->M0[12], players[j]->PlayerPosition);
      float d = vector_sqrmag(t);

      // randomize order a little
      //d += randRange(0, 15 * 15);
      d += randRange(0, 0.5 * minDistSqr);

      if (d < closestDistSqr)
        closestDistSqr = d;
    }
    
    if (closestDistSqr >= minDistSqr) {
      for (j = found; j >= 0; --j) {
        if (j >= SPAWNPOINT_NEAR_BUFFER_SIZE) continue;

        if (closestDistSqr < bestPointDistSqr[j]) {
          
          if (found <= j) found = j+1;
          bestPoints[j] = i;
          bestPointDistSqr[j] = closestDistSqr;
          break;
        }
      }
    }
	}

  // pick random from best points
  if (found)
  {
    int pick = rand(found);
    int idx = bestPoints[pick];
    SpawnPoint* sp = spawnPointGet(idx);
    vector_copy(out, (float*)&sp->M0[12]);
    vector_fromyaw(t, randRadian());
    vector_scale(t, t, 3);
    vector_add(out, out, t);
  }

	return found;
}

//--------------------------------------------------------------------------
int spawnGetRandomPoint(VECTOR out, struct MobSpawnParams* mob) {
	// harder difficulty, better chance mob spawns near you
	float r = randRange(0, 1) / State.Difficulty;
  float demonBellFactor = 1;

  if (State.DemonBellCount > 0) {
    demonBellFactor = lerpf(1, 0.33, powf(State.RoundDemonBellCount / (float)State.DemonBellCount, 2));
  }

#if QUICK_SPAWN
	r = MOB_SPAWN_NEAR_PLAYER_PROBABILITY;
#endif

	// spawn on player
	if (r <= MOB_SPAWN_AT_PLAYER_PROBABILITY && (mob->SpawnType & SPAWN_TYPE_ON_PLAYER)) {
		Player * targetPlayer = playerGetRandom();
		if (targetPlayer) {
			vector_write(out, randVectorRange(-1, 1));
			out[2] = 0;
			vector_add(out, out, targetPlayer->PlayerPosition);
			return 1;
		}
	}

	// spawn near healthbox
	if (r <= MOB_SPAWN_NEAR_HEALTHBOX_PROBABILITY && (mob->SpawnType & SPAWN_TYPE_NEAR_HEALTHBOX)) {
		Moby* hb = mobyGetRandomHealthbox();
		if (hb)
			return spawnPointGetNearestTo(hb->Position, out, 10);
	}

	// spawn near player
	if (r <= MOB_SPAWN_NEAR_PLAYER_PROBABILITY && (mob->SpawnType & SPAWN_TYPE_NEAR_PLAYER)) {
		return spawnPointGetNearToPlayer(out, 30 * demonBellFactor * mapConfig->BakedConfig->SpawnDistanceFactor);
	}

	// spawn semi near player
	if (r <= MOB_SPAWN_SEMI_NEAR_PLAYER_PROBABILITY && (mob->SpawnType & SPAWN_TYPE_SEMI_NEAR_PLAYER)) {
		return spawnPointGetNearToPlayer(out, 60 * demonBellFactor * mapConfig->BakedConfig->SpawnDistanceFactor);
	}

	// spawn
	return spawnPointGetNearToPlayer(out, 100 * demonBellFactor * mapConfig->BakedConfig->SpawnDistanceFactor);
}

//--------------------------------------------------------------------------
int spawnCanSpawnMob(struct MobSpawnParams* mob, int spawnParamIdx)
{
	return (State.RoundNumber + 1) >= mob->MinRound
      && mob->Probability > 0
      && (!mob->SpecialRoundOnly || State.RoundIsSpecial)
      && (mob->MaxSpawnedAtOnce <= 0 || State.MobStats.NumAlive[spawnParamIdx] < mob->MaxSpawnedAtOnce)
      && (mob->MaxSpawnedPerRound <= 0 || State.MobStats.NumSpawnedThisRound[spawnParamIdx] < mob->MaxSpawnedPerRound)
      ;
}

//--------------------------------------------------------------------------
void populateSpawnArgsFromConfig(struct MobSpawnEventArgs* output, struct MobConfig* config, int spawnParamsIdx, int isBaseConfig, int spawnFlags)
{
  GameSettings* gs = gameGetSettings();
  if (!gs)
    return;

  float damage = config->Damage;
  float speed = config->Speed;
  float health = config->Health;
  float difficultyBump = 0.6 + powf(1.5, State.RoundNumber / 25) * 0.4;
  float difficulty = DIFFICULTY_FACTOR * difficultyBump * State.Difficulty;

  // scale config by round
  if (isBaseConfig) {
    //printf("1 %d damage:%f speed:%f health:%f\n", spawnParamsIdx, damage, speed, health);
    //damage = damage * powf(1 + (MOB_BASE_DAMAGE_SCALE * config->DamageScale * DIFFICULTY_FACTOR * State.Difficulty * randRange(0.5, 1.2)), 2);
    //speed = speed * powf(1 + (MOB_BASE_SPEED_SCALE * config->SpeedScale * DIFFICULTY_FACTOR * State.Difficulty * randRange(0.5, 1.2)), 2);
    
    damage = damage * (1 + (MOB_BASE_DAMAGE_SCALE * config->DamageScale * difficulty));
    speed = speed * (1 + (MOB_BASE_SPEED_SCALE * config->SpeedScale * difficulty));
    health = health * powf(1 + (MOB_BASE_HEALTH_SCALE * config->HealthScale * difficulty), 2);
    //printf("2 %d damage:%f speed:%f health:%f\n", spawnParamsIdx, damage, speed, health);
  }

  // enforce max values
  if (config->MaxDamage > 0 && damage > config->MaxDamage)
    damage = config->MaxDamage;
  if (config->MaxSpeed > 0 && speed > config->MaxSpeed)
    speed = config->MaxSpeed;
  if (config->MaxHealth > 0 && health > config->MaxHealth)
    health = config->MaxHealth;
  
  //printf("3 %d damage:%f speed:%f health:%f\n", spawnParamsIdx, damage, speed, health);

  output->SpawnParamsIdx = spawnParamsIdx;
  output->Bolts = (config->Bolts + randRangeInt(-50, 50)) * BOLT_TAX[(int)gs->PlayerCount];
  output->Xp = config->Xp;
  output->StartHealth = health;
  output->Bangles = (u16)config->Bangles;
  output->Damage = (u16)damage;
  output->AttackRadiusEighths = (u8)(config->AttackRadius * 8);
  output->HitRadiusEighths = (u8)(config->HitRadius * 8);
  output->CollRadiusEighths = (u8)(config->CollRadius * 8);
  output->SpeedEighths = (u16)(speed * 8);
  output->ReactionTickCount = (u8)config->ReactionTickCount;
  output->AttackCooldownTickCount = (u8)config->AttackCooldownTickCount;
  output->MobAttribute = config->MobAttribute;
}

//--------------------------------------------------------------------------
struct MobSpawnParams* spawnGetRandomMobParams(int * mobIdx)
{
	int i;

  if (!mapConfig->DefaultSpawnParams)
    return NULL;

	if (State.RoundIsSpecial && mapConfig->SpecialRoundParams) {
		struct SurvivalSpecialRoundParam* params = &mapConfig->SpecialRoundParams[State.RoundSpecialIdx];
    for (i = 0; i < params->SpawnParamCount; ++i) {
      int spawnParamIdx = params->SpawnParamIds[i];
      struct MobSpawnParams* mob = &mapConfig->DefaultSpawnParams[spawnParamIdx];
      if (spawnCanSpawnMob(mob, spawnParamIdx) && randRange(0,1) <= mob->Probability) {
        if (mobIdx)
          *mobIdx = spawnParamIdx;
        return mob;
      }
    }

    return NULL;
	}

	for (i = 0; i < mapConfig->DefaultSpawnParamsCount; ++i) {
		struct MobSpawnParams* mob = &mapConfig->DefaultSpawnParams[i];
		if (spawnCanSpawnMob(mob, i) && randRange(0,1) <= mob->Probability) {
			if (mobIdx)
				*mobIdx = i;
			return mob;
		}
	}

	for (i = 0; i < mapConfig->DefaultSpawnParamsCount; ++i) {
		struct MobSpawnParams* mob = &mapConfig->DefaultSpawnParams[i];
		//DPRINTF("CANT SPAWN round:%d mobIdx:%d mobMinRound:%d mobCost:%d mobProb:%f\n", State.RoundNumber, i, mob->MinRound, mob->Cost, mob->Probability);
	}
	
	return NULL;
}

//--------------------------------------------------------------------------
int spawnRandomMob(void) {
	VECTOR sp;
	int mobIdx = 0;
	struct MobSpawnParams* mob = spawnGetRandomMobParams(&mobIdx);
	if (mob) {
    int cooldownTicks = mob->CooldownTicks + (mob->CooldownOffsetPerRoundFactor * State.RoundNumber);
    if (cooldownTicks < 0) cooldownTicks = 0;

		// skip if cooldown not 0
    int cooldown = defaultSpawnParamsCooldowns[mobIdx];
    if (cooldown > 0) {
      return 0;
    }
    else if (State.RoundIsSpecial) {
      defaultSpawnParamsCooldowns[mobIdx] = lerpf(2 * TPS, cooldownTicks, mapConfig->SpecialRoundParams[State.RoundSpecialIdx].SpawnRateFactor);
      //DPRINTF("%d => %d\n", mobIdx, defaultSpawnParamsCooldowns[mobIdx]);
    }
    else {
      defaultSpawnParamsCooldowns[mobIdx] = cooldownTicks;
    }

		// try and spawn
		if (spawnGetRandomPoint(sp, mob)) {
			if (mobCreate(mobIdx, sp, 0, -1, 0, &mob->Config)) {
				return 1;
			} else { DPRINTF("failed to create mob\n"); }
		} else { DPRINTF("failed to get random spawn point\n"); }
	} else { DPRINTF("failed to get random mob params\n"); }

	// no more budget left
	return 0;
}

//--------------------------------------------------------------------------
void customBangelizeWeapons(Moby* weaponMoby, int weaponId, int weaponLevel)
{
	switch (weaponId)
	{
		case WEAPON_ID_VIPERS:
		{
			weaponMoby->Bangles = weaponLevel < VENDOR_MAX_WEAPON_LEVEL ? 0 : 1;
			break;
		}
		case WEAPON_ID_MAGMA_CANNON:
		{
			weaponMoby->Bangles = weaponLevel < VENDOR_MAX_WEAPON_LEVEL ? (weaponLevel ? 4 : 3) : 0x31;
			break;
		}
		case WEAPON_ID_ARBITER:
		{
			weaponMoby->Bangles = weaponLevel < VENDOR_MAX_WEAPON_LEVEL ? (weaponLevel ? 3 : 1) : 0xC;
			break;
		}
		case WEAPON_ID_FUSION_RIFLE:
		{
			weaponMoby->Bangles = weaponLevel < VENDOR_MAX_WEAPON_LEVEL ? (weaponLevel ? 2 : 1) : 6;
			break;
		}
		case WEAPON_ID_MINE_LAUNCHER:
		{
			weaponMoby->Bangles = weaponLevel < VENDOR_MAX_WEAPON_LEVEL ? (weaponLevel ? 0xC : 0) : 0xF;
			break;
		}
		case WEAPON_ID_B6:
		{
			weaponMoby->Bangles = weaponLevel < VENDOR_MAX_WEAPON_LEVEL ? (weaponLevel ? 6 : 4) : 7;
			break;
		}
		case WEAPON_ID_OMNI_SHIELD:
		{
			weaponMoby->Bangles = weaponLevel < VENDOR_MAX_WEAPON_LEVEL ? (weaponLevel ? 0xC : 1) : 0xF;
			break;
		}
		case WEAPON_ID_FLAIL:
		{
			if (weaponMoby->PVar) {
				weaponMoby = *(Moby**)((u32)weaponMoby->PVar + 0x33C);
				if (weaponMoby)
					weaponMoby->Bangles = weaponLevel < VENDOR_MAX_WEAPON_LEVEL ? (weaponLevel ? 3 : 1) : 0x1F;
			}
			break;
		}
	}
}

//--------------------------------------------------------------------------
// TODO: Move this into the code segment, overwriting GuiMain_GetGadgetVersionName at (0x00541850)
char * customGetGadgetVersionName(int localPlayerIndex, int weaponId, int showWeaponLevel, int capitalize, int minLevel)
{
	Player* p = playerGetFromSlot(localPlayerIndex);
	char* buf = (char*)(0x2F9D78 + localPlayerIndex*0x40);
	short* gadgetDef = ((short* (*)(int, int))0x00627f48)(weaponId, 0);
	int level = 0;
	int strIdOffset = capitalize ? 3 : 4;
	if (p && p->GadgetBox) {
		level = p->GadgetBox->Gadgets[weaponId].Level;
	}

	if (level >= 9)
		strIdOffset = capitalize ? 12 : 10;

	char* str = uiMsgString(gadgetDef[strIdOffset]);
  int prestige = State.PlayerStates[p->PlayerId].State.WeaponPrestige[weaponIdToSlot(weaponId)];

	if (level < 9 && level >= minLevel) {
		snprintf(buf, 0x40, "%c%s V%d", WEAPON_PRESTIGE_PREFIX[prestige], str, level+1);
		return buf;
	} else {
		snprintf(buf, 0x40, "%c%s", WEAPON_PRESTIGE_PREFIX[prestige], str);
		return buf;
	}
}

//--------------------------------------------------------------------------
void customMineMobyUpdate(Moby* moby)
{
	// handle auto destructing v10 child mines after N seconds
	// and auto destroy v10 child mines if they came from a remote client
	u32 pvar = (u32)moby->PVar;
	if (pvar) {
		int mLayer = *(int*)(pvar + 0x200);
		Player * p = playerGetAll()[*(short*)(pvar + 0xC0)];
		int createdLocally = p && p->IsLocal;
		int gameTime = gameGetTime();
		int timeCreated = *(int*)(&moby->Rotation[3]);

		// set initial time created
		if (timeCreated == 0) {
			timeCreated = gameTime;
			*(int*)(&moby->Rotation[3]) = timeCreated;
		}

		// don't spawn child mines if mine was created by remote client
		if (!createdLocally) {
			*(int*)(pvar + 0x200) = 2;
		} else if (p && mLayer > 0 && (gameTime - timeCreated) > (5 * TIME_SECOND)) {
			*(int*)(&moby->Rotation[3]) = gameTime;
			((void (*)(Moby*, Player*, int))0x003C90C0)(moby, p, 0);
			return;
		}
	}
	
	// call base
	((void (*)(Moby*))0x003C6C28)(moby);
}

//--------------------------------------------------------------------------
void onPlayerWithdrawnBankBox(int playerId, int bolts)
{
  if (!State.Bankbox) return;

  struct BankBoxPVar* pvars = (struct BankBoxPVar*)State.Bankbox->PVar;
  pvars->TotalBolts -= bolts;
  pvars->BoltsWithdrawnThisRound += bolts;
  State.PlayerStates[playerId].State.Bolts += bolts;
  playPaidSound(playerGetAll()[playerId]);
}

//--------------------------------------------------------------------------
int onPlayerWithdrawnBankBoxRemote(void * connection, void * data)
{
	SurvivalPlayerWithdrawnBankBoxMessage_t * message = (SurvivalPlayerWithdrawnBankBoxMessage_t*)data;
	onPlayerWithdrawnBankBox(message->PlayerId, message->Amount);

	return sizeof(SurvivalPlayerWithdrawnBankBoxMessage_t);
}

//--------------------------------------------------------------------------
void playerWithdrawnBankBox(int playerId, int bolts)
{
	SurvivalPlayerWithdrawnBankBoxMessage_t message;

	// send out
	message.PlayerId = playerId;
  message.Amount = bolts;
	netBroadcastCustomAppMessage(NET_DELIVERY_CRITICAL, netGetDmeServerConnection(), CUSTOM_MSG_WITHDRAWN_BANK_BOX, sizeof(SurvivalPlayerWithdrawnBankBoxMessage_t), &message);

	// set locally
	onPlayerWithdrawnBankBox(message.PlayerId, message.Amount);
}

//--------------------------------------------------------------------------
void onPlayerInteractBankBox(int playerId, int deposit, int amount)
{
  if (!State.Bankbox) return;

  struct BankBoxPVar* pvars = (struct BankBoxPVar*)State.Bankbox->PVar;
  
  if (deposit) {
    pvars->TotalBolts += amount;
    pvars->BoltsDepositThisRound += amount;
  } else if (gameAmIHost() && pvars->TotalBolts > 0) {
    int withdraw = pvars->TotalBolts;
    if (withdraw > BANK_BOX_AMOUNT)
      withdraw = BANK_BOX_AMOUNT;
    playerWithdrawnBankBox(playerId, withdraw);
  }
}

//--------------------------------------------------------------------------
int onPlayerInteractBankBoxRemote(void * connection, void * data)
{
	SurvivalPlayerInteractBankBoxMessage_t * message = (SurvivalPlayerInteractBankBoxMessage_t*)data;
	onPlayerInteractBankBox(message->PlayerId, message->Deposit, message->Amount);

	return sizeof(SurvivalPlayerInteractBankBoxMessage_t);
}

//--------------------------------------------------------------------------
void playerInteractBankBox(Player* player, int deposit)
{
	SurvivalPlayerInteractBankBoxMessage_t message;

  int amount = BANK_BOX_AMOUNT;
  if (deposit && State.PlayerStates[player->PlayerId].State.Bolts < amount) {
    amount = State.PlayerStates[player->PlayerId].State.Bolts;
  }

	// send out
	message.PlayerId = player->PlayerId;
  message.Deposit = deposit;
  message.Amount = amount;
  if (deposit) State.PlayerStates[player->PlayerId].State.Bolts -= amount;
	netBroadcastCustomAppMessage(NET_DELIVERY_CRITICAL, netGetDmeServerConnection(), CUSTOM_MSG_INTERACT_BANK_BOX, sizeof(SurvivalPlayerInteractBankBoxMessage_t), &message);

	// set locally
	onPlayerInteractBankBox(message.PlayerId, message.Deposit, message.Amount);
}

//--------------------------------------------------------------------------
void onPlayerPrestigeWeapon(int playerId, int weaponId, int prestigeId)
{
  char buf[64];
	Player* p = playerGetAll()[playerId];
	if (!p)
		return;

	GadgetBox* gBox = p->GadgetBox;
	gBox->Gadgets[weaponId].Level = -1;
	playerGiveWeapon(gBox, weaponId, 0, 1);
	if (p->Gadgets[0].pMoby && p->Gadgets[0].id == weaponId)
		customBangelizeWeapons(p->Gadgets[0].pMoby, weaponId, 0);

	// stats
	int wepSlotId = weaponIdToSlot(weaponId);
  State.PlayerStates[playerId].State.WeaponPrestige[wepSlotId] = prestigeId;

	// play upgrade sound
	playUpgradeSound(p);

#if LOG_STATS2
	DPRINTF("%d (%08X) weapon %d prestiged to %d\n", playerId, (u32)p, weaponId, prestigeId);
#endif
}

//--------------------------------------------------------------------------
int onPlayerPrestigeWeaponRemote(void * connection, void * data)
{
	SurvivalWeaponPrestigeMessage_t * message = (SurvivalWeaponPrestigeMessage_t*)data;
	onPlayerPrestigeWeapon(message->PlayerId, message->WeaponId, message->PrestigeId);

	return sizeof(SurvivalWeaponPrestigeMessage_t);
}

//--------------------------------------------------------------------------
int playerPrestigeWeapon(Player* player, int weaponId)
{
	SurvivalWeaponPrestigeMessage_t message;

  int slotId = weaponIdToSlot(weaponId);
  if (slotId <= 0) return 0;
  int nextPrestige = State.PlayerStates[player->PlayerId].State.WeaponPrestige[slotId] + 1;
  if (nextPrestige > WEAPON_PRESTIGE_MAX) return 0;
  if (player->GadgetBox->Gadgets[weaponId].Level != VENDOR_MAX_WEAPON_LEVEL) return 0;

	// send out
	message.PlayerId = player->PlayerId;
	message.WeaponId = weaponId;
	message.PrestigeId = nextPrestige;
	netBroadcastCustomAppMessage(NET_DELIVERY_CRITICAL, netGetDmeServerConnection(), CUSTOM_MSG_WEAPON_PRESTIGE, sizeof(SurvivalWeaponPrestigeMessage_t), &message);

	// set locally
	onPlayerPrestigeWeapon(message.PlayerId, message.WeaponId, message.PrestigeId);

  // some weapons stay in their v10 state until they are re-equipped
  player->ChangeWeaponHeldId = WEAPON_ID_WRENCH;
  return 1;
}

//--------------------------------------------------------------------------
void onPlayerUpgradeWeapon(int playerId, int weaponId, int level, int alphaMod)
{
  char buf[64];
	Player* p = playerGetAll()[playerId];
  if (alphaMod)
	  State.PlayerStates[playerId].State.AlphaMods[alphaMod]++;
	if (!p)
		return;

	GadgetBox* gBox = p->GadgetBox;
	gBox->Gadgets[weaponId].Level = -1;
	playerGiveWeapon(gBox, weaponId, level, 1);
	if (p->Gadgets[0].pMoby && p->Gadgets[0].id == weaponId)
		customBangelizeWeapons(p->Gadgets[0].pMoby, weaponId, level);

	if (p->IsLocal && alphaMod) {
		gBox->ModBasic[alphaMod-1]++;
		snprintf(buf, sizeof(buf), "Got %s", ALPHA_MODS[alphaMod]);
    pushSnack(buf, TPS * 1, p->LocalPlayerIndex);
    //State.PlayerStates[playerId].MessageCooldownTicks = 0;
	}
	
	// stats
	int wepSlotId = weaponIdToSlot(weaponId);
	if (level > State.PlayerStates[playerId].State.BestWeaponLevel[wepSlotId])
		State.PlayerStates[playerId].State.BestWeaponLevel[wepSlotId] = level;

	// play upgrade sound
	playUpgradeSound(p);

	// set exp bar to max
	if (level == VENDOR_MAX_WEAPON_LEVEL)
		gBox->Gadgets[weaponId].Experience = level;

#if LOG_STATS2
	DPRINTF("%d (%08X) weapon %d upgraded to %d\n", playerId, (u32)p, weaponId, level);
#endif
}

//--------------------------------------------------------------------------
int onPlayerUpgradeWeaponRemote(void * connection, void * data)
{
	SurvivalWeaponUpgradeMessage_t * message = (SurvivalWeaponUpgradeMessage_t*)data;
	onPlayerUpgradeWeapon(message->PlayerId, message->WeaponId, message->Level, message->Alphamod);

	return sizeof(SurvivalWeaponUpgradeMessage_t);
}

//--------------------------------------------------------------------------
void playerUpgradeWeapon(Player* player, int weaponId, int noAlphaMod)
{
	SurvivalWeaponUpgradeMessage_t message;
	GadgetBox* gBox = player->GadgetBox;

	// send out
	message.PlayerId = player->PlayerId;
	message.WeaponId = weaponId;
	message.Level = gBox->Gadgets[weaponId].Level + 1;
	message.Alphamod = ENABLED_ALPHA_MODS[rand(ENABLED_ALPHA_MODS_COUNT)];
  if (noAlphaMod)
    message.Alphamod = 0;
	netBroadcastCustomAppMessage(NET_DELIVERY_CRITICAL, netGetDmeServerConnection(), CUSTOM_MSG_WEAPON_UPGRADE, sizeof(SurvivalWeaponUpgradeMessage_t), &message);

	// set locally
	onPlayerUpgradeWeapon(message.PlayerId, message.WeaponId, message.Level, message.Alphamod);
}

//--------------------------------------------------------------------------
void mapUpgradePlayerWeaponHandler(int playerId, int weaponId, int giveAlphaMod)
{
  Player* player = playerGetAll()[playerId];
  if (!player || !player->PlayerMoby || !player->IsLocal)
    return;
  
  if (!player->GadgetBox || player->GadgetBox->Gadgets[weaponId].Level >= 9)
    return;

  playerUpgradeWeapon(player, weaponId, !giveAlphaMod);
}

//--------------------------------------------------------------------------
void onPlayerRevive(int playerId, int fromPlayerId)
{
  int useDeadPos = 0;

	if (fromPlayerId >= 0 && playerId != fromPlayerId)
		State.PlayerStates[fromPlayerId].State.Revives++;
	
	State.PlayerStates[playerId].IsDead = 0;
	State.PlayerStates[playerId].State.TimesRevived++;
	State.PlayerStates[playerId].State.TimesRevivedSinceRoundStart++;
	Player* player = playerGetAll()[playerId];
	if (!player)
		return;

	// backup current position/rotation
	VECTOR deadPos, deadPosDown, deadRot;
	vector_copy(deadPos, player->PlayerPosition);
	vector_copy(deadPosDown, player->PlayerPosition);
	vector_copy(deadRot, player->PlayerRotation);
  deadPos[2] += 0.5;
  deadPosDown[2] -= 1;

	// respawn
	PlayerVTable* vtable = playerGetVTable(player);
	if (vtable)
		vtable->UpdateState(player, PLAYER_STATE_IDLE, 1, 1, 1);
  getResurrectPoint(player, player->PlayerPosition, player->PlayerRotation, 0);
  playerSetPosRot(player, player->PlayerPosition, player->PlayerRotation);
	playerSetHealth(player, player->MaxHealth);

  // only reset back to dead pos if player died on ground
  if (CollLine_Fix(deadPos, deadPosDown, COLLISION_FLAG_IGNORE_DYNAMIC, player->PlayerMoby, NULL)) {
    int colId = CollLine_Fix_GetHitCollisionId() & 0xF;
    if (colId == 0xF || colId == 0x7 || colId == 0x9 || colId == 0xA)
      useDeadPos = 1;
  }

  if (useDeadPos) {
    playerSetPosRot(player, deadPos, deadRot);

    // spawn explosion to push zombies back
    spawnExplosion(deadPos, 5, 0x80008000);
    mobReactToExplosionAt(fromPlayerId, deadPos, 1, 8);
  }

	player->timers.acidTimer = 0;
	player->timers.collOff = 0;
	player->timers.freezeTimer = 1; // triggers the game to handle resetting movement speed on 0
	player->timers.postHitInvinc = TPS * 3;
}

//--------------------------------------------------------------------------
int onPlayerReviveRemote(void * connection, void * data)
{
	SurvivalReviveMessage_t * message = (SurvivalReviveMessage_t*)data;
	onPlayerRevive(message->PlayerId, message->FromPlayerId);

	return sizeof(SurvivalReviveMessage_t);
}

//--------------------------------------------------------------------------
void playerRevive(Player* player, int fromPlayerId)
{
	SurvivalReviveMessage_t message;

	// send out
	message.PlayerId = player->PlayerId;
	message.FromPlayerId = fromPlayerId;
	netSendCustomAppMessage(NET_DELIVERY_CRITICAL, netGetDmeServerConnection(), -1, CUSTOM_MSG_REVIVE_PLAYER, sizeof(SurvivalReviveMessage_t), &message);

	// set locally
	onPlayerRevive(message.PlayerId, message.FromPlayerId);
}

//--------------------------------------------------------------------------
void onSetPlayerDead(int playerId, char isDead)
{
  int totalTicks = PLAYER_BASE_REVIVE_TICKS - (PLAYER_REVIVE_COST_PER_REVIVE_TICKS * State.PlayerStates[playerId].State.TimesRevivedSinceRoundStart);
  if (totalTicks < PLAYER_MIN_REVIVE_TICKS) totalTicks = PLAYER_MIN_REVIVE_TICKS;

	State.PlayerStates[playerId].IsDead = isDead;
	State.PlayerStates[playerId].ReviveCooldownTicks = isDead ? totalTicks : 0;
#if LOG_STATS2
	DPRINTF("%d died (%d)\n", playerId, isDead);
#endif
}

//--------------------------------------------------------------------------
int onSetPlayerDeadRemote(void * connection, void * data)
{
	SurvivalSetPlayerDeadMessage_t * message = (SurvivalSetPlayerDeadMessage_t*)data;
	onSetPlayerDead(message->PlayerId, message->IsDead);

	return sizeof(SurvivalSetPlayerDeadMessage_t);
}

//--------------------------------------------------------------------------
void setPlayerDead(Player* player, char isDead)
{
	SurvivalSetPlayerDeadMessage_t message;

	// send out
	message.PlayerId = player->PlayerId;
	message.IsDead = isDead;
	netSendCustomAppMessage(NET_DELIVERY_CRITICAL, netGetDmeServerConnection(), -1, CUSTOM_MSG_PLAYER_DIED, sizeof(SurvivalSetPlayerDeadMessage_t), &message);

	// set locally
	onSetPlayerDead(message.PlayerId, message.IsDead);
}

//--------------------------------------------------------------------------
void onSetPlayerWeaponMods(int playerId, int weaponId, u8* mods)
{
	int i;
	Player* p = playerGetAll()[playerId];
	if (!p)
		return;

	GadgetBox* gBox = p->GadgetBox;
	for (i = 0; i < 10; ++i)
		gBox->Gadgets[weaponId].AlphaMods[i] = mods[i];
		
#if LOG_STATS2
	DPRINTF("%d set %d mods\n", playerId, weaponId);
#endif
}

//--------------------------------------------------------------------------
int onSetPlayerWeaponModsRemote(void * connection, void * data)
{
	SurvivalSetWeaponModsMessage_t * message = (SurvivalSetWeaponModsMessage_t*)data;
	onSetPlayerWeaponMods(message->PlayerId, message->WeaponId, message->Mods);

	return sizeof(SurvivalSetPlayerDeadMessage_t);
}

//--------------------------------------------------------------------------
void setPlayerWeaponMods(Player* player, int weaponId, int* mods)
{
	int i;
	SurvivalSetWeaponModsMessage_t message;

	// send out
	message.PlayerId = player->PlayerId;
	message.WeaponId = (u8)weaponId;
	for (i = 0; i < 10; ++i)
		message.Mods[i] = (u8)mods[i];
	netSendCustomAppMessage(NET_DELIVERY_CRITICAL, netGetDmeServerConnection(), -1, CUSTOM_MSG_PLAYER_SET_WEAPON_MODS, sizeof(SurvivalSetWeaponModsMessage_t), &message);

	// set locally
	onSetPlayerWeaponMods(message.PlayerId, message.WeaponId, message.Mods);
}

//--------------------------------------------------------------------------
void onSetPlayerStats(int playerId, struct SurvivalPlayerState* stats)
{
	memcpy(&State.PlayerStates[playerId].State, stats, sizeof(struct SurvivalPlayerState));
}

//--------------------------------------------------------------------------
int onSetPlayerStatsRemote(void * connection, void * data)
{
	SurvivalSetPlayerStatsMessage_t * message = (SurvivalSetPlayerStatsMessage_t*)data;
	onSetPlayerStats(message->PlayerId, &message->Stats);

	return sizeof(SurvivalSetPlayerStatsMessage_t);
}

//--------------------------------------------------------------------------
void sendPlayerStats(int playerId)
{
	SurvivalSetPlayerStatsMessage_t message;

	// send out
	message.PlayerId = playerId;
	memcpy(&message.Stats, &State.PlayerStates[playerId].State, sizeof(struct SurvivalPlayerState));
	netSendCustomAppMessage(NET_DELIVERY_CRITICAL, netGetDmeServerConnection(), -1, CUSTOM_MSG_PLAYER_SET_STATS, sizeof(SurvivalSetPlayerStatsMessage_t), &message);
}

//--------------------------------------------------------------------------
void onPlayerUseItem(int playerId, enum MysteryBoxItem item)
{
  Player** players = playerGetAll();

  // set player item to -1
  State.PlayerStates[playerId].State.Item = -1;

  // use item
  switch (item)
  {
    case MYSTERY_BOX_ITEM_INVISIBILITY_CLOAK:
    {
      State.PlayerStates[playerId].InvisibilityCloakStopTime = gameGetTime() + ITEM_INVISCLOAK_DURATION;
      if (State.PlayerStates[playerId].IsLocal) {
        pushSnack("Invisibility Cloak Equipped!", TPS, players[playerId]->LocalPlayerIndex);
      }
      break;
    }
    case MYSTERY_BOX_ITEM_EMP_HEALTH_GUN:
    {
      State.PlayerStates[playerId].HealthTornadoStopTime = gameGetTime() + ITEM_HEALTHTORNADO_DURATION;
      if (State.PlayerStates[playerId].IsLocal) {
        pushSnack("Health Tornado Activated!", TPS, players[playerId]->LocalPlayerIndex);
      }
      break;
    }
    default:
    {
      break;
    }
  }
}

//--------------------------------------------------------------------------
int onPlayerUseItemRemote(void * connection, void * data)
{
  SurvivalPlayerUseItem_t message;

  memcpy(&message, data, sizeof(SurvivalPlayerUseItem_t));
	onPlayerUseItem(message.PlayerId, message.Item);

	return sizeof(SurvivalPlayerUseItem_t);
}

//--------------------------------------------------------------------------
void playerUseItem(int playerId, enum MysteryBoxItem item)
{
	SurvivalPlayerUseItem_t message;

	// send out
	message.PlayerId = playerId;
	message.Item = item;
	netSendCustomAppMessage(NET_DELIVERY_CRITICAL, netGetDmeServerConnection(), -1, CUSTOM_MSG_PLAYER_USE_ITEM, sizeof(SurvivalPlayerUseItem_t), &message);

	// locally
	onPlayerUseItem(playerId, item);
}

//--------------------------------------------------------------------------
void onSetPlayerDoublePoints(char isActive[GAME_MAX_PLAYERS], int timeOfDoublePoints[GAME_MAX_PLAYERS])
{
	int i;
	for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
		State.PlayerStates[i].IsDoublePoints = isActive[i];
		State.PlayerStates[i].TimeOfDoublePoints = timeOfDoublePoints[i];
	}
}

//--------------------------------------------------------------------------
int onSetPlayerDoublePointsRemote(void * connection, void * data)
{
	SurvivalSetPlayerDoublePointsMessage_t * message = (SurvivalSetPlayerDoublePointsMessage_t*)data;
	onSetPlayerDoublePoints(message->IsActive, message->TimeOfDoublePoints);

	return sizeof(SurvivalSetPlayerDoublePointsMessage_t);
}

//--------------------------------------------------------------------------
void sendDoublePoints(void)
{
	int i;
	SurvivalSetPlayerDoublePointsMessage_t message;

	// build active list
	for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
		message.IsActive[i] = State.PlayerStates[i].IsDoublePoints;
		message.TimeOfDoublePoints[i] = State.PlayerStates[i].TimeOfDoublePoints;
	}

	// send out
	netSendCustomAppMessage(NET_DELIVERY_CRITICAL, netGetDmeServerConnection(), -1, CUSTOM_MSG_PLAYER_SET_DOUBLE_POINTS, sizeof(SurvivalSetPlayerDoublePointsMessage_t), &message);
}

//--------------------------------------------------------------------------
void setDoublePoints(int isActive)
{
	int i;
	SurvivalSetPlayerDoublePointsMessage_t message;
	Player** players = playerGetAll();

	// build active list
	for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
		Player* p = players[i];
		if (p) {
			State.PlayerStates[i].TimeOfDoublePoints = gameGetTime();
			State.PlayerStates[i].IsDoublePoints = isActive;
		}

		message.IsActive[i] = State.PlayerStates[i].IsDoublePoints;
		message.TimeOfDoublePoints[i] = State.PlayerStates[i].TimeOfDoublePoints;
	}

	// send out
	netSendCustomAppMessage(NET_DELIVERY_CRITICAL, netGetDmeServerConnection(), -1, CUSTOM_MSG_PLAYER_SET_DOUBLE_POINTS, sizeof(SurvivalSetPlayerDoublePointsMessage_t), &message);

	// locally
	onSetPlayerDoublePoints(message.IsActive, message.TimeOfDoublePoints);
}

//--------------------------------------------------------------------------
void onSetPlayerDoubleXP(char isActive[GAME_MAX_PLAYERS], int timeOfDoubleXP[GAME_MAX_PLAYERS])
{
	int i;
	for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
		State.PlayerStates[i].IsDoubleXP = isActive[i];
		State.PlayerStates[i].TimeOfDoubleXP = timeOfDoubleXP[i];
	}
}

//--------------------------------------------------------------------------
int onSetPlayerDoubleXPRemote(void * connection, void * data)
{
	SurvivalSetPlayerDoubleXPMessage_t * message = (SurvivalSetPlayerDoubleXPMessage_t*)data;
	onSetPlayerDoubleXP(message->IsActive, message->TimeOfDoubleXP);

	return sizeof(SurvivalSetPlayerDoubleXPMessage_t);
}

//--------------------------------------------------------------------------
void sendDoubleXP(void)
{
	int i;
	SurvivalSetPlayerDoubleXPMessage_t message;

	// build active list
	for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
		message.IsActive[i] = State.PlayerStates[i].IsDoubleXP;
		message.TimeOfDoubleXP[i] = State.PlayerStates[i].TimeOfDoubleXP;
	}

	// send out
	netSendCustomAppMessage(NET_DELIVERY_CRITICAL, netGetDmeServerConnection(), -1, CUSTOM_MSG_PLAYER_SET_DOUBLE_XP, sizeof(SurvivalSetPlayerDoubleXPMessage_t), &message);
}

//--------------------------------------------------------------------------
void setDoubleXP(int isActive)
{
	int i;
	SurvivalSetPlayerDoubleXPMessage_t message;
	Player** players = playerGetAll();

	// build active list
	for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
		Player* p = players[i];
		if (p) {
			State.PlayerStates[i].TimeOfDoubleXP = gameGetTime();
			State.PlayerStates[i].IsDoubleXP = isActive;
		}

		message.IsActive[i] = State.PlayerStates[i].IsDoubleXP;
		message.TimeOfDoubleXP[i] = State.PlayerStates[i].TimeOfDoubleXP;
	}

	// send out
	netSendCustomAppMessage(NET_DELIVERY_CRITICAL, netGetDmeServerConnection(), -1, CUSTOM_MSG_PLAYER_SET_DOUBLE_XP, sizeof(SurvivalSetPlayerDoubleXPMessage_t), &message);

	// locally
	onSetPlayerDoubleXP(message.IsActive, message.TimeOfDoubleXP);
}

//--------------------------------------------------------------------------
void onSetFreeze(char isActive)
{
	State.Freeze = isActive;
	if (isActive)
		State.TimeOfFreeze = gameGetTime();
}

//--------------------------------------------------------------------------
int onSetFreezeRemote(void * connection, void * data)
{
	SurvivalSetFreezeMessage_t * message = (SurvivalSetFreezeMessage_t*)data;
	onSetFreeze(message->IsActive);

	return sizeof(SurvivalSetFreezeMessage_t);
}

//--------------------------------------------------------------------------
void setFreeze(int isActive)
{
	SurvivalSetFreezeMessage_t message;

	// send out
	message.IsActive = isActive;
	netSendCustomAppMessage(NET_DELIVERY_CRITICAL, netGetDmeServerConnection(), -1, CUSTOM_MSG_PLAYER_SET_FREEZE, sizeof(SurvivalSetFreezeMessage_t), &message);

	// locally
	onSetFreeze(isActive);
}

//--------------------------------------------------------------------------
void onSetRound50Time(int time)
{
  char buffer[64];
  
  snprintf(buffer, sizeof(buffer), "50 Rounds Completed %02d:%02d.%03d", time / TIME_MINUTE, (time % TIME_MINUTE) / TIME_SECOND, time % TIME_SECOND);
  pushSnack(buffer, TPS * 10, 0);
  State.Round50Time = time;
}

//--------------------------------------------------------------------------
int onSetRound50TimeRemote(void * connection, void * data)
{
  int time;
  memcpy(&time, data, sizeof(time));

  onSetRound50Time(time);
	return sizeof(time);
}

//--------------------------------------------------------------------------
void setRound50Time(int time)
{
  if (time <= 0) return;

  // send and set locally
	netSendCustomAppMessage(NET_DELIVERY_CRITICAL, netGetDmeServerConnection(), -1, CUSTOM_MSG_SET_ROUND_50_TIME, sizeof(time), &time);
  onSetRound50Time(time);
}

//--------------------------------------------------------------------------
void checkForRound50Time(void)
{
  // update time took to beat 50 rounds
  if (State.RoundNumber == 50 && State.Round50Time == 0 && gameAmIHost()) {
    setRound50Time(gameGetTime() - State.InitializedTime);
  }
}

//--------------------------------------------------------------------------
int getWeaponPrestigeCost(int prestigeLevel)
{
  return WEAPON_PRESTIGE_COST[prestigeLevel];
}

//--------------------------------------------------------------------------
int canUpgradeWeapon(Player * player, enum WEAPON_IDS weaponId) {
	if (!player)
		return 0;

	GadgetBox* gBox = player->GadgetBox;

	switch (weaponId)
	{
		case WEAPON_ID_VIPERS:
		case WEAPON_ID_MAGMA_CANNON:
		case WEAPON_ID_ARBITER:
		case WEAPON_ID_FUSION_RIFLE:
		case WEAPON_ID_MINE_LAUNCHER:
		case WEAPON_ID_B6:
		case WEAPON_ID_OMNI_SHIELD:
		case WEAPON_ID_FLAIL:
			return gBox->Gadgets[(int)weaponId].Level >= 0 && gBox->Gadgets[(int)weaponId].Level < VENDOR_MAX_WEAPON_LEVEL;
		default: return 0;
	}
}

//--------------------------------------------------------------------------
int getUpgradeCost(Player * player, enum WEAPON_IDS weaponId) {
	if (!player)
		return 0;

	GadgetBox* gBox = player->GadgetBox;
	int level = gBox->Gadgets[(int)weaponId].Level;
	if (level < 0 || level >= VENDOR_MAX_WEAPON_LEVEL)
		return 0;
		
	// determine discount rate
	float rate = clamp(1 - (PLAYER_UPGRADE_VENDOR_FACTOR * State.PlayerStates[player->PlayerId].State.Upgrades[UPGRADE_VENDOR]), 0, 1);

	return ceilf(UPGRADE_COST[level] * rate);
}

//--------------------------------------------------------------------------
void respawnDeadPlayers(void) {
	int i;
	Player** players = playerGetAll();

	for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
		Player * p = players[i];
		if (p && playerIsDead(p)) {
			State.PlayerStates[i].State.TimesRevivedSinceRoundStart = 0;
      if (p->IsLocal) {
			  playerRespawn(p);
      }
		}
		
		State.PlayerStates[i].IsDead = 0;
		State.PlayerStates[i].ReviveCooldownTicks = 0;
    //memset(State.PlayerStates[i].State.WeaponPrestige, 0, sizeof(State.PlayerStates[i].State.WeaponPrestige));
	}
}

//--------------------------------------------------------------------------
void setPlayerQuadCooldownTimer(Player * player) {
	player->timers.damageMuliplierTimer = 1200 + State.PlayerStates[player->PlayerId].State.Upgrades[UPGRADE_PICKUPS] * TPS * 1.0;
  player->DamageMultiplier = 4;
}

//--------------------------------------------------------------------------
void setPlayerShieldCooldownTimer(void) {
	
  Player* player = NULL;

	// pointer to player is in $s1
	asm volatile (
    ".set noreorder;"
		"move %0, $s1"
		: : "r" (player)
	);

  player->timers.armorLevelTimer = 1800 + State.PlayerStates[player->PlayerId].State.Upgrades[UPGRADE_PICKUPS] * TPS * 1.25;
  POKE_U32((u32)player + 0x2FB4, 3);
}

//--------------------------------------------------------------------------
void onV10MagDamageMoby(Moby* target, MobyColDamageIn* in)
{
  if (in->Damager) {
    VECTOR dt;
    vector_subtract(dt, target->Position, in->Damager->Position);
    float dist = vector_length(dt);
    float min = 0.2;
    Player* damager = guberMobyGetPlayerDamager(in->Damager);
    if (damager) min += 0.05 * playerGetWeaponAlphaModCount(damager->GadgetBox, WEAPON_ID_MAGMA_CANNON, ALPHA_MOD_AREA);

    float falloff = minf(1, maxf(min, minf(1, 1 - (dist / 32))));
    float origDmg = in->DamageHp;
    in->DamageHp *= falloff;
  }

  mobyCollDamageDirect(target, in);
}

//--------------------------------------------------------------------------
void onHealthBomb(Player* fromPlayer, VECTOR position) {
  
  int i;
  VECTOR dt;
  Player** players = playerGetAll();

  // heal / revive
  if (fromPlayer) {
    for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
      Player* player = players[i];
      if (!player || !playerIsConnected(player)) continue;

      vector_subtract(dt, player->PlayerPosition, position);
      if (vector_sqrmag(dt) < (ITEM_EMP_HEALTH_EFFECT_RADIUS*ITEM_EMP_HEALTH_EFFECT_RADIUS)) {
        if (playerIsDead(player) && player->IsLocal) {
          playerRevive(player, fromPlayer->PlayerId);
        } else {
          playerSetHealth(player, clamp(player->Health + (player->MaxHealth * ITEM_HEALTHTORNADO_HEAL_PERCENT), 0, player->MaxHealth));
        }
      }
    }
  }
  
  // explode
  //spawnExplosion(position, 5, 0x80800000);
  u32 color = 0x80800000;
  mobyPlaySoundByClass(0, 0, mobySpawnExplosion(
    vector_read(position), 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0,
    0, 0, color, color, color, color, color, color, color, color, color,
    0, 0, 0, 0, 2, 0, 0, 0
  ), MOBY_ID_ARBITER_ROCKET0);

  if (fromPlayer && fromPlayer->IsLocal) mobReactToExplosionAt(fromPlayer->PlayerId, position, 1, 8);
}

//--------------------------------------------------------------------------
void onEmpExplode(Moby* moby, VECTOR from, VECTOR to, u32 a3, u32 t0, u32 t1, u32 t2, u32 t3, float f12, float f13, float f14, float f15) {
  
  int i;
  Player** players = playerGetAll();
  VECTOR dt;
  Player* fromPlayer = guberMobyGetPlayerDamager(moby);

  if (fromPlayer) {
    for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
      Player* player = players[i];
      if (!player || !player->SkinMoby) continue;

      vector_subtract(dt, player->PlayerPosition, to);
      if (vector_sqrmag(dt) < (ITEM_EMP_HEALTH_EFFECT_RADIUS*ITEM_EMP_HEALTH_EFFECT_RADIUS)) {
        if (playerIsDead(player)) {
          playerRevive(player, fromPlayer->PlayerId);
        } else {
          playerSetHealth(player, player->MaxHealth);
        }
      }
    }
  }
  
  ((void (*)(Moby* moby, VECTOR from, VECTOR to, u32 a3, u32 t0, u32 t1, u32 t2, u32 t3, float f12, float f13, float f14, float f15))0x00420ac0)(
    moby, from, to, a3, t0, t1, t2, t3,
    f12, f13, f14, f15
  );
}

//--------------------------------------------------------------------------
int onEmpHitMoby(Moby* moby) {
  Player* player = guberMobyGetPlayerDamager(moby);
  return player != 0;
}

//--------------------------------------------------------------------------
Moby* onEmpFired(int oclass, int pvarSize, Moby* empLauncherMoby) {
  
  if (empLauncherMoby) {
    Player* firedFromPlayer = guberMobyGetPlayerDamager(empLauncherMoby);
    if (firedFromPlayer) {

      // remove item
      if (State.PlayerStates[firedFromPlayer->PlayerId].State.Item == MYSTERY_BOX_ITEM_EMP_HEALTH_GUN)
        State.PlayerStates[firedFromPlayer->PlayerId].State.Item = -1;

      // unequip emp
      if (firedFromPlayer->IsLocal && firedFromPlayer->WeaponHeldId == WEAPON_ID_EMP) {
        playerEquipWeapon(firedFromPlayer, playerGetLocalEquipslot(firedFromPlayer->LocalPlayerIndex, 0));
      }
    }
  }

  return mobySpawn(oclass, pvarSize);
}

//--------------------------------------------------------------------------
void playerRewardXp(int playerId, int weaponId, int xp)
{
  Player* player = playerGetAll()[playerId];
  if (!player || !player->PlayerMoby) return;

	struct SurvivalPlayer* pState = &State.PlayerStates[playerId];

  // give xp
  pState->State.XP += xp * (pState->IsDoubleXP ? 2 : 1);
  u64 targetForNextToken = getXpForNextToken(pState->State.TotalTokens);

  // handle weapon xp
  if (weaponId > 1) {
    int xpCount = playerGetWeaponAlphaModCount(player->GadgetBox, weaponId, ALPHA_MOD_XP);
    pState->State.XP += xpCount * XP_ALPHAMOD_XP;
  }

  // give tokens
  while (pState->State.XP >= targetForNextToken)
  {
    pState->State.XP -= targetForNextToken;
    pState->State.TotalTokens += 1;
    pState->State.CurrentTokens += 1;
    targetForNextToken = getXpForNextToken(pState->State.TotalTokens);

    if (player->IsLocal) {
      sendPlayerStats(playerId);
    }
  }
}

//--------------------------------------------------------------------------
void playerOnPushedIntoWall(Player* player)
{
  if (!player || !player->SkinMoby || !player->PlayerMoby) return;
  
  // push mobs away
  mobReactToExplosionAt(player->PlayerId, player->PlayerPosition, 1, 8);

  // move player out of clipped wall
  // using lastGoodPos doesn't always return us to before the clip
  if (player->IsLocal) {
    playerSetPosRot(player, player->Ground.lastGoodPos, player->PlayerRotation);
  }
}

//--------------------------------------------------------------------------
void onV10VipersHitSurface(Moby* moby)
{
  ((void (*)(Moby*))0x003C05D8)(moby);

  // explosion
  ((void (*)(float damage, float radius, VECTOR p, u32 damageFlags, Moby* moby, Moby* hitMoby))0x003c3a48)(1, 0.5, moby->Position, 0x801, moby, NULL);
}

//--------------------------------------------------------------------------
int playerIsSmashingFlail(Player* player)
{
  if (!player) return 0;

  // jump smash
  if (player->PlayerState == PLAYER_STATE_JUMP_ATTACK && player->WeaponHeldId == WEAPON_ID_FLAIL) {
    return 1;
  }
  
  // ground smash
  if (player->PlayerState == PLAYER_STATE_FLAIL_ATTACK && player->PlayerMoby && player->PlayerMoby->AnimSeqId == 0x33) {
    return 1;
  }
  
  return 0;
}

//--------------------------------------------------------------------------
void processPlayer(int pIndex) {
	VECTOR t;
	int i = 0, localPlayerIndex, heldWeapon, hasMessage = 0;
	char strBuf[32];
	Player** players = playerGetAll();
	Player* player = players[pIndex];
	struct SurvivalPlayer * playerData = &State.PlayerStates[pIndex];
  GameSettings* gs = gameGetSettings();

	if (!player || !player->PlayerMoby)
		return;

	int isDeadState = playerIsDead(player) || player->Health == 0;
	int actionCooldownTicks = decTimerU8(&playerData->ActionCooldownTicks);
	int messageCooldownTicks = decTimerU8(&playerData->MessageCooldownTicks);
	int reviveCooldownTicks = decTimerU16(&playerData->ReviveCooldownTicks);
	if (playerData->IsDead && reviveCooldownTicks > 0 && playerGetNumLocals() == 1) {
    int x,y;
    VECTOR pos = {0,0,1,0};
    vector_add(pos, player->PlayerPosition, pos);
    if (gfxWorldSpaceToScreenSpace(pos, &x, &y)) {
      snprintf(strBuf, sizeof(strBuf), "\x1c  %02d", reviveCooldownTicks/60);
      gfxScreenSpaceText(x, y, 0.75, 0.75, 0x80FFFFFF, strBuf, -1, 4);
      dzoDrawReviveMsg(pIndex, pos, reviveCooldownTicks/60);
    }
	}

  // last hit
  if (player->Health != playerData->LastHealth) playerData->TicksSinceHealthChanged = 0;
  else playerData->TicksSinceHealthChanged += 1;
  playerData->LastHealth = player->Health;

	// set max health
	player->MaxHealth = 50 + (PLAYER_UPGRADE_HEALTH_FACTOR * State.PlayerStates[pIndex].State.Upgrades[UPGRADE_HEALTH]);

	// set speed
  // increase with speed upgrades
  // add base speed boost with hoverboots
	player->Speed = 1 + (PLAYER_UPGRADE_SPEED_FACTOR * State.PlayerStates[pIndex].State.Upgrades[UPGRADE_SPEED]);
  player->Speed += playerGetStackableCount(pIndex, STACKABLE_ITEM_HOVERBOOTS) * ITEM_STACKABLE_HOVERBOOTS_SPEED_BUF;
  if (player->timers.freezeTimer)
    player->Speed *= 0.65;

	// adjust speed of chargeboot stun
	if (player->PlayerState == PLAYER_STATE_JUMP_BOUNCE) {
		*(float*)((u32)player + 0x25C4) = player->Speed;
	} else {
		*(float*)((u32)player + 0x25C4) = 1.0;
	}
	
  // handle invis cloak
  if (playerData->InvisibilityCloakStopTime > 0) {
    int timeUntilEnd = gameGetTime() - playerData->InvisibilityCloakStopTime;
    if (timeUntilEnd < 0) {
      player->SkinMoby->Opacity = 0x20;
    } else {
      player->SkinMoby->Opacity = 0x80;
      playerData->InvisibilityCloakStopTime = -1;
    }
  }

  // handle health tornado
  if (playerData->HealthTornadoStopTime > 0) {
    int timeUntilEnd = gameGetTime() - playerData->HealthTornadoStopTime;
    if (timeUntilEnd < 0) {
      if (!playerData->HealthTornadoActivateTicks) {
        onHealthBomb(player, player->PlayerPosition);
        playerData->HealthTornadoActivateTicks = ITEM_HEALTHTORNADO_PERIOD_TICKS;
      }
      else {
        playerData->HealthTornadoActivateTicks--;
      }
    } else {
      playerData->HealthTornadoActivateTicks = 0;
      playerData->HealthTornadoStopTime = -1;
    }
  }

  // update state timers
  if (playerStates[pIndex] != player->PlayerState)
    playerStateTimers[pIndex] = 0;
  else
    playerStateTimers[pIndex] += 1;
  playerStates[pIndex] = player->PlayerState;

	if (player->IsLocal) {
		
		GadgetBox* gBox = player->GadgetBox;
		localPlayerIndex = player->LocalPlayerIndex;
		heldWeapon = player->WeaponHeldId;

	  // set max xp
		u32 xp = playerData->State.XP;
		u32 nextXp = getXpForNextToken(playerData->State.TotalTokens);
    if (playerGetNumLocals() > 1) setPlayerWeaponsMenu(localPlayerIndex);
		setPlayerEXP(localPlayerIndex, xp / (float)nextXp);

		// find closest mob
		playerData->MinSqrDistFromMob = 1000000;
		playerData->MaxSqrDistFromMob = 0;
		GuberMoby* gm = guberMobyGetFirst();
		
		while (gm)
		{
			if (gm->Moby && mobyIsMob(gm->Moby))
			{
				vector_subtract(t, player->PlayerPosition, gm->Moby->Position);
				float sqrDist = vector_sqrmag(t);
				if (sqrDist < playerData->MinSqrDistFromMob)
					playerData->MinSqrDistFromMob = sqrDist;
			  if (sqrDist > playerData->MaxSqrDistFromMob)
					playerData->MaxSqrDistFromMob = sqrDist;
			}
			
			gm = (GuberMoby*)gm->Guber.Prev;
		}

		// handle death
		if (isDeadState && !playerData->IsDead) {

      // increment DeathsByMob if we were killed by a mob
      if (player->PlayerMoby->CollDamage >= 0) {
        MobyColDamage* damage = mobyGetDamage(player->PlayerMoby, 0xFFFFFF, 1);
        if (damage && mobyIsMob(damage->Damager)) {
          struct MobPVar* pvars = (struct MobPVar*)damage->Damager->PVar;
          if (pvars) {
            playerData->State.DeathsByMob[pvars->MobVars.SpawnParamsIdx] += 1;
          }
        }
      }

      if (playerData->State.Item == MYSTERY_BOX_ITEM_REVIVE_TOTEM) {
        playerRevive(player, player->PlayerId);
        uiShowPopup(0, "Self Revive Used!");
        playerData->State.Item = -1;
      } else {
			  setPlayerDead(player, 1);
      }
		} else if (playerData->IsDead && !isDeadState) {
			setPlayerDead(player, 0);
		}

		// force to last good position
		// if (isDeadState && player->timers.state > TPS) {
		// 	vector_subtract(t, player->PlayerPosition, player->Ground.lastGoodPos);
		// 	if (vector_sqrmag(t) > 1) {
		// 		playerSetPosRot(player, player->Ground.lastGoodPos, player->PlayerRotation);
		// 		player->Health = 0;
		// 		player->PlayerState = PLAYER_STATE_DEATH;
		// 	}
		// }

		// set experience to min of level and max level 
		for (i = WEAPON_ID_VIPERS; i <= WEAPON_ID_FLAIL; ++i) {
			if (gBox->Gadgets[i].Level >= 0) {
				gBox->Gadgets[i].Experience = gBox->Gadgets[i].Level < VENDOR_MAX_WEAPON_LEVEL ? gBox->Gadgets[i].Level : VENDOR_MAX_WEAPON_LEVEL;
			}

      // embed prestige in weapon "modActiveWeapon" mod entry
      // only lower 8 bits are used by game
      int slot = weaponIdToSlot(i);
      if (slot > 0) {
        gBox->Gadgets[i].UNK_10 = (gBox->Gadgets[i].UNK_10 & 0xFF) | (playerData->State.WeaponPrestige[slot] << 8);
		  }
    }

		// decrement flail ammo while spinning flail
		GameOptions* gameOptions = gameGetOptions();
		if (!gameOptions->GameFlags.MultiplayerGameFlags.UnlimitedAmmo
			&& player->PlayerState == PLAYER_STATE_FLAIL_ATTACK
			&& player->timers.state >= 60) {
				if (gBox->Gadgets[WEAPON_ID_FLAIL].Ammo > 0) {
					if ((player->timers.state % 60) == 0) {
            ((void (*)(GadgetBox*, int, int))0x00627180)(gBox, WEAPON_ID_FLAIL, 1);
						//gBox->Gadgets[WEAPON_ID_FLAIL].Ammo -= 1;
					}
				} else {
					PlayerVTable* vtable = playerGetVTable(player);
					if (vtable && vtable->UpdateState)
						vtable->UpdateState(player, PLAYER_STATE_IDLE, 1, 1, 1);
				}
		}

		//
		if (actionCooldownTicks > 0 || player->timers.noInput) {
			if (messageCooldownTicks == 1) {
        hudHidePopup();
      }
      return;
    }

		// handle closing weapons menu
		if (playerData->IsInWeaponsMenu) {
			for (i = 0; i < sizeof(UPGRADEABLE_WEAPONS)/sizeof(u8); ++i)
				setPlayerWeaponMods(player, UPGRADEABLE_WEAPONS[i], gBox->Gadgets[UPGRADEABLE_WEAPONS[i]].AlphaMods);
			playerData->IsInWeaponsMenu = 0;
		}

		// handle vendor logic
		if (State.Vendor) {

			// check player is holding upgradable weapon
			if (canUpgradeWeapon(player, heldWeapon)) {

				// check distance
				vector_subtract(t, player->PlayerPosition, State.Vendor->Position);
				if (vector_sqrmag(t) < (WEAPON_VENDOR_MAX_DIST * WEAPON_VENDOR_MAX_DIST)) {
					
					// get upgrade cost
					int cost = getUpgradeCost(player, heldWeapon);

					// draw help popup
					snprintf(LocalPlayerStrBuffer[localPlayerIndex], sizeof(LocalPlayerStrBuffer[localPlayerIndex]), SURVIVAL_UPGRADE_MESSAGE, cost);
					uiShowPopup(localPlayerIndex, LocalPlayerStrBuffer[localPlayerIndex]);
					hasMessage = 1;
					playerData->MessageCooldownTicks = 2;

					// handle purchase
					if (playerData->State.Bolts >= cost) {

						// handle pad input
						if (padGetButton(localPlayerIndex, PAD_CIRCLE) > 0) {
							playerData->State.Bolts -= cost;
							playerUpgradeWeapon(player, heldWeapon, 0);
							uiShowPopup(localPlayerIndex, uiMsgString(0x2330));
              playPaidSound(player);
							playerData->ActionCooldownTicks = WEAPON_UPGRADE_COOLDOWN_TICKS;
						}
					}
				}
			}
		}

		// handle big al logic
		if (State.BigAl) {

			// check distance
			vector_subtract(t, player->PlayerPosition, State.BigAl->Position);
			if (vector_sqrmag(t) < (BIG_AL_MAX_DIST * BIG_AL_MAX_DIST) && vector_innerproduct(t, player->CameraForward) < -0.9) {
				
				// draw help popup
				uiShowPopup(localPlayerIndex, SURVIVAL_OPEN_WEAPONS_MESSAGE);
				hasMessage = 1;
				playerData->MessageCooldownTicks = 2;

				if (padGetButtonDown(localPlayerIndex, PAD_TRIANGLE) > 0) {
					openWeaponsMenu(localPlayerIndex);
					playerData->ActionCooldownTicks = WEAPON_MENU_COOLDOWN_TICKS;
					playerData->IsInWeaponsMenu = 1;
				}
			}
		}

		// handle bank logic
		if (State.Bankbox) {

      struct BankBoxPVar* bboxPvars = (struct BankBoxPVar*)State.Bankbox->PVar;

			// check distance
			vector_subtract(t, player->PlayerPosition, State.Bankbox->Position);
			if (vector_sqrmag(t) < (BANK_BOX_MAX_DIST * BANK_BOX_MAX_DIST) /* && vector_innerproduct(t, player->CameraForward) < -0.6 */) {
				
				// draw help popup
        char buf[32];
        snprintf(buf, sizeof(buf), SURVIVAL_INTERACT_BANK_BALANCE_MESSAGE, bboxPvars->TotalBolts);
        snprintf(LocalPlayerStrBuffer[localPlayerIndex], sizeof(LocalPlayerStrBuffer[localPlayerIndex]), "%s %s", SURVIVAL_INTERACT_BANK_INTERACT_MESSAGE, buf);
				uiShowPopup(localPlayerIndex, LocalPlayerStrBuffer[localPlayerIndex]);
				hasMessage = 1;
				playerData->MessageCooldownTicks = 2;

				if (padGetButton(localPlayerIndex, PAD_SQUARE) > 0 && bboxPvars->TotalBolts > 0) {
					playerInteractBankBox(player, 0);
					playerData->ActionCooldownTicks = PLAYER_BANK_BOX_COOLDOWN_TICKS;
				} else if (padGetButton(localPlayerIndex, PAD_CIRCLE) > 0 && playerData->State.Bolts > 0) {
					playerInteractBankBox(player, 1);
          playPaidSound(player);
					playerData->ActionCooldownTicks = PLAYER_BANK_BOX_COOLDOWN_TICKS;
				}
			}
		}

		// handle prestige logic
		if (State.PrestigeMachine) {

			// check distance
			vector_subtract(t, player->PlayerPosition, State.PrestigeMachine->Position);
			if (vector_sqrmag(t) < (PRESTIGE_MACHINE_MAX_DIST * PRESTIGE_MACHINE_MAX_DIST)) {
				
        int weaponId = player->WeaponHeldId;
        int slotId = weaponIdToSlot(weaponId);
        if (slotId > 0) {

          int canPrestige = player->GadgetBox->Gadgets[weaponId].Level == VENDOR_MAX_WEAPON_LEVEL;
          int maxPrestige = playerData->State.WeaponPrestige[slotId] >= WEAPON_PRESTIGE_MAX;
          int cost = getWeaponPrestigeCost(playerData->State.WeaponPrestige[slotId]);

          // draw help popup
          if (maxPrestige)
            snprintf(LocalPlayerStrBuffer[localPlayerIndex], sizeof(LocalPlayerStrBuffer[localPlayerIndex]), SURVIVAL_PRESTIGE_WEAPON_MAXED_MESSAGE);
          else if (canPrestige)
            snprintf(LocalPlayerStrBuffer[localPlayerIndex], sizeof(LocalPlayerStrBuffer[localPlayerIndex]), SURVIVAL_PRESTIGE_WEAPON_MESSAGE, cost);
          else
            snprintf(LocalPlayerStrBuffer[localPlayerIndex], sizeof(LocalPlayerStrBuffer[localPlayerIndex]), SURVIVAL_PRESTIGE_WEAPON_NEED_V10_MESSAGE);

          uiShowPopup(localPlayerIndex, LocalPlayerStrBuffer[localPlayerIndex]);
          hasMessage = 1;
          playerData->MessageCooldownTicks = 2;

          if (!maxPrestige && canPrestige && padGetButtonDown(localPlayerIndex, PAD_CIRCLE) > 0 && playerData->State.Bolts >= cost) {
            if (playerPrestigeWeapon(player, weaponId)) {
              playerData->State.Bolts -= cost;
              playerData->ActionCooldownTicks = 10;
              playPaidSound(player);
              pushSnack("Your weapon seems more powerful...", 60, localPlayerIndex);
            }
          }
        }
			}
		}

		// handle upgrade logic
		for (i = 0; i < UPGRADE_COUNT; ++i) {
			Moby* upgradeMoby = State.UpgradeMobies[i];
      if (upgradeMoby) {
        if (!playerData->ActionCooldownTicks) {
          vector_subtract(t, player->PlayerPosition, upgradeMoby->Position);
          if (vector_sqrmag(t) < (UPGRADE_PICKUP_RADIUS * UPGRADE_PICKUP_RADIUS)) {
            
            if (playerData->State.Upgrades[i] < UpgradeMax[i]) {

              struct UpgradePVar* upgradePVars = (struct UpgradePVar*)upgradeMoby->PVar;
              if (!upgradePVars) continue;

              // draw help popup
              snprintf(LocalPlayerStrBuffer[localPlayerIndex], sizeof(LocalPlayerStrBuffer[localPlayerIndex]), SURVIVAL_BUY_UPGRADE_MESSAGES[i], upgradePVars->Uses);
              uiShowPopup(player->LocalPlayerIndex, LocalPlayerStrBuffer[localPlayerIndex]);
              hasMessage = 1;
              playerData->MessageCooldownTicks = 2;

              // handle pad input
              if (padGetButton(localPlayerIndex, PAD_CIRCLE) > 0 && playerData->State.CurrentTokens >= UPGRADE_TOKEN_COST) {
                playerData->State.CurrentTokens -= UPGRADE_TOKEN_COST;
                playerData->ActionCooldownTicks = PLAYER_UPGRADE_COOLDOWN_TICKS;
                if (mapConfig) mapConfig->PickupUpgradeFunc(upgradeMoby, pIndex);
                playPaidSound(player);
              }
            } else {

              // draw maxed out popup
              snprintf(LocalPlayerStrBuffer[localPlayerIndex], sizeof(LocalPlayerStrBuffer[localPlayerIndex]), SURVIVAL_MAXED_OUT_UPGRADE_MESSAGE);
              uiShowPopup(player->LocalPlayerIndex, LocalPlayerStrBuffer[localPlayerIndex]);
              hasMessage = 1;
              playerData->MessageCooldownTicks = 2;
            }
            break;
          }
        }
      }
		}

		// handle revive logic
    int revivingPlayerId = -1;
		if (!isDeadState) {
			for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
				if (i != pIndex) {

					// ensure player exists, is dead, and is on the same team
					struct SurvivalPlayer * otherPlayerData = &State.PlayerStates[i];
					Player * otherPlayer = players[i];
					if (otherPlayer && otherPlayerData->IsDead && (otherPlayerData->ReviveCooldownTicks > 0 || (playerData->RevivingPlayerTicks > 0 && playerData->RevivingPlayerId == i))) {

						// check distance
						vector_subtract(t, player->PlayerPosition, otherPlayer->PlayerPosition);
						if (vector_sqrmag(t) < (PLAYER_REVIVE_MAX_DIST * PLAYER_REVIVE_MAX_DIST)) {

              // draw revive player popup
              // or draw how 
              if (playerData->RevivingPlayerId == -1) {
                snprintf(LocalPlayerStrBuffer[localPlayerIndex], sizeof(LocalPlayerStrBuffer[localPlayerIndex]), SURVIVAL_REVIVE_MESSAGE, gs->PlayerNames[i]);
                uiShowPopup(localPlayerIndex, LocalPlayerStrBuffer[localPlayerIndex]);
                hasMessage = 1;
                playerData->MessageCooldownTicks = 2;
              } else if (playerData->RevivingPlayerId == i) {
                float time = (PLAYER_TIME_TO_REVIVE_TICKS - playerData->RevivingPlayerTicks) / (float)TPS;
                snprintf(LocalPlayerStrBuffer[localPlayerIndex], sizeof(LocalPlayerStrBuffer[localPlayerIndex]), "Reviving... %.2f", time);
                uiShowPopup(localPlayerIndex, LocalPlayerStrBuffer[localPlayerIndex]);
                hasMessage = 1;
                playerData->MessageCooldownTicks = 2;
              }

              // check for interaction pad
              if (padGetButton(localPlayerIndex, PAD_DOWN) > 0) {

                // check that we are reviving this player or no one yet
                if (playerData->RevivingPlayerId == i || playerData->RevivingPlayerId == -1) {
                  //playerData->ActionCooldownTicks = PLAYER_REVIVE_COOLDOWN_TICKS;

                  // count ticks
                  if (playerData->RevivingPlayerId != i) playerData->RevivingPlayerTicks = 0;
                  playerData->RevivingPlayerTicks++;
                  revivingPlayerId = i;

                  // revive after n ticks have passed
                  if (playerData->RevivingPlayerTicks > PLAYER_TIME_TO_REVIVE_TICKS) {
                    playerRevive(otherPlayer, player->PlayerId);
                    playPaidSound(player);
                    playerData->RevivingPlayerTicks = 0;
                    revivingPlayerId = -1;
                  }

                  break;
                }
              }
						}
					}
				}
			}
		}

    // 
    playerData->RevivingPlayerId = revivingPlayerId;

    // handle items
    if (!isDeadState && padGetButtonDown(0, PAD_LEFT) > 0) {
      switch (playerData->State.Item)
      {
        case MYSTERY_BOX_ITEM_INVISIBILITY_CLOAK:
        {
          playerUseItem(pIndex, playerData->State.Item);
          break;
        }
        case MYSTERY_BOX_ITEM_EMP_HEALTH_GUN:
        {
          playerUseItem(pIndex, playerData->State.Item);
          //onHealthBomb(player, player->PlayerPosition);
          //playerEquipWeapon(player, WEAPON_ID_EMP);
          break;
        }
      }
    }

    /*
    static int aaa = 1;
    static int handle = 0;
		if (padGetButtonDown(0, PAD_RIGHT) > 0) {
			aaa += 1;
      TestSoundDef.Index = aaa;
      if (handle)
        soundKillByHandle(handle);
      int id = soundPlay(&TestSoundDef, 0, playerGetFromSlot(0)->PlayerMoby, 0, 0x400);
      if (id >= 0)
        handle = soundCreateHandle(id);
      else
        handle = 0;
			DPRINTF("%d\n", aaa);
		}
		else if (padGetButtonDown(0, PAD_LEFT) > 0) {
			aaa -= 1;
      TestSoundDef.Index = aaa;
      if (handle)
        soundKillByHandle(handle);
      int id = soundPlay(&TestSoundDef, 0, playerGetFromSlot(0)->PlayerMoby, 0, 0x400);
      if (id >= 0)
        handle = soundCreateHandle(id);
      else
        handle = 0;
			DPRINTF("%d\n", aaa);
		}
    */

		if (!hasMessage && messageCooldownTicks == 1) {
      hudHidePopup();
		}
	} else {

    // bug where players on GREEN or higher will remain cranking bolt after finishing
    // so we'll check to see if their remote player state is no longer cranking
    // and we'll stop them
    int remoteState = *(int*)((u32)player + 0x3a80);
    int playerStateTimer = playerStateTimers[pIndex];
    if (player->PlayerState == PLAYER_STATE_BOLT_CRANK
     && remoteState != PLAYER_STATE_BOLT_CRANK
     && playerStateTimer > TPS*3) {

			PlayerVTable* vtable = playerGetVTable(player);
      vtable->UpdateState(player, remoteState, 1, 1, 1);
    }
  }
}

//--------------------------------------------------------------------------
int getRoundBonus(int roundNumber, int numPlayers)
{
	int multiplier = State.RoundIsSpecial ? ROUND_SPECIAL_BONUS_MULTIPLIER : 1;
	int bonus = State.RoundNumber * ROUND_BASE_BOLT_BONUS * numPlayers;
	if (bonus > ROUND_MAX_BOLT_BONUS)
		return ROUND_MAX_BOLT_BONUS * multiplier;

  // give a round bonus for using the demon bells
  if (State.DemonBellCount > 0 && State.RoundDemonBellCount > 0) {
    multiplier += State.RoundDemonBellCount / (float)State.DemonBellCount;
  }

	return bonus * multiplier;
}

//--------------------------------------------------------------------------
void onSetRoundComplete(int gameTime, int boltBonus)
{
	int i;
#if LOG_STATS2
	DPRINTF("round complete. zombies spawned %d/%d\n", State.MobStats.TotalSpawnedThisRound, State.RoundMaxMobCount);
#endif

	// 
	State.RoundEndTime = 0;
	State.RoundCompleteTime = gameTime;

	// add bonus
	for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
		if (!State.PlayerStates[i].IsDead) {
			State.PlayerStates[i].State.Bolts += boltBonus;
			State.PlayerStates[i].State.TotalBolts += boltBonus;
		}
	}

  // vest
  if (State.Bankbox) {
    struct BankBoxPVar* pvars = (struct BankBoxPVar*)State.Bankbox->PVar;
    int vestable = pvars->BoltsAtStartOfRound - pvars->BoltsWithdrawnThisRound;
    if (vestable < 0) vestable = 0;

#if LOG_STATS2
    DPRINTF("bank vested %d (- %d)=>%d (total %d=>%d)", pvars->BoltsAtStartOfRound, pvars->BoltsWithdrawnThisRound, (int)(vestable * (1 + BANK_VEST_FACTOR)), pvars->TotalBolts, pvars->TotalBolts + (int)(vestable * BANK_VEST_FACTOR));
#endif

    int vested = vestable * BANK_VEST_FACTOR;
    if (vested > BANK_MAX_VEST) vested = BANK_MAX_VEST;

    int totalBolts = pvars->TotalBolts + vested;
    if (totalBolts > BANK_MAX_BOLTS) totalBolts = BANK_MAX_BOLTS;
    pvars->TotalBolts = totalBolts;
  }

	respawnDeadPlayers();
}

//--------------------------------------------------------------------------
int onSetRoundCompleteRemote(void * connection, void * data)
{
	SurvivalRoundCompleteMessage_t * message = (SurvivalRoundCompleteMessage_t*)data;
	onSetRoundComplete(message->GameTime, message->BoltBonus);

	return sizeof(SurvivalRoundCompleteMessage_t);
}

//--------------------------------------------------------------------------
void setRoundComplete(void)
{
	SurvivalRoundCompleteMessage_t message;

	// don't allow overwriting existing outcome
	if (State.RoundCompleteTime)
		return;

	// don't allow changing outcome when not host
	if (!State.IsHost)
		return;

	// send out
	GameSettings* gameSettings = gameGetSettings();
	message.GameTime = gameGetTime();
	message.BoltBonus = getRoundBonus(State.RoundNumber, gameSettings->PlayerCount);
	netBroadcastCustomAppMessage(NET_DELIVERY_CRITICAL, netGetDmeServerConnection(), CUSTOM_MSG_ROUND_COMPLETE, sizeof(SurvivalRoundCompleteMessage_t), &message);

	// set locally
	onSetRoundComplete(message.GameTime, message.BoltBonus);
}

//--------------------------------------------------------------------------
void onSetRoundStart(int roundNumber, int gameTime)
{
  int i;
  Player** players = playerGetAll();

  if (State.RoundNumber != roundNumber) {
    State.RoundDemonBellCount = 0;
    demonbellOnRoundChanged(roundNumber);
  }

	// 
	State.RoundNumber = roundNumber;
	State.RoundEndTime = gameTime;
  State.RoundIsSpecial = 0;
  if (mapConfig->DefaultSpawnParams && mapConfig->SpecialRoundParamsCount > 0) {

    // find next special round
    int roundId = roundNumber + 1;
    for (i = 0; i < mapConfig->SpecialRoundParamsCount; ++i) {
      int minRound = mapConfig->SpecialRoundParams[i].MinRound;
      int repeatEvery = mapConfig->SpecialRoundParams[i].RepeatEveryNRounds;
      int repeatCount = mapConfig->SpecialRoundParams[i].RepeatCount;

      if (roundId >= minRound) {
        float iteration = (roundId - minRound) / (float)repeatEvery;
        if (iteration == ceilf(iteration) && (repeatCount <= 0 || iteration < repeatCount)) {
          State.RoundIsSpecial = 1;
          State.RoundSpecialIdx = i;
          break;
        }
      }
    }
  }

  // set best round for each player still in lobby
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    Player* player = players[i];
    if (player && player->SkinMoby) {
      State.PlayerStates[i].State.BestRound = roundNumber;
    }
  }

	DPRINTF("round start %d\n", roundNumber);
}

//--------------------------------------------------------------------------
int onSetRoundStartRemote(void * connection, void * data)
{
	SurvivalRoundStartMessage_t * message = (SurvivalRoundStartMessage_t*)data;
	onSetRoundStart(message->RoundNumber, message->GameTime);

	return sizeof(SurvivalRoundStartMessage_t);
}

//--------------------------------------------------------------------------
void setRoundStart(int skip)
{
	SurvivalRoundStartMessage_t message;

	// don't allow overwriting existing outcome unless skip
	if (State.RoundEndTime && !skip)
		return;

	// don't allow changing outcome when not host
	if (!State.IsHost)
		return;

	// increment round if end
	int targetRound = State.RoundNumber;
	if (!State.RoundEndTime)
		targetRound += 1;
		
	// send out
	message.RoundNumber = targetRound;
	message.GameTime = gameGetTime() + (skip ? 0 : ROUND_TRANSITION_DELAY_MS);
  if (!skip && State.RoundIsSpecial && mapConfig->SpecialRoundParams[State.RoundSpecialIdx].UnlimitedPostRoundTime)
    message.GameTime = -1;

	netBroadcastCustomAppMessage(NET_DELIVERY_CRITICAL, netGetDmeServerConnection(), CUSTOM_MSG_ROUND_START, sizeof(SurvivalRoundStartMessage_t), &message);

	// set locally
	onSetRoundStart(message.RoundNumber, message.GameTime);
}

//--------------------------------------------------------------------------
void forcePlayerHUD(void)
{
	int i;

	// replace normal scoreboard with bolt counter
	for (i = 0; i < GAME_MAX_LOCALS; ++i)
	{
    if (shouldDrawHud()) {
      PlayerHUDFlags* hudFlags = hudGetPlayerFlags(i);
      if (hudFlags) {
        hudFlags->Flags.BoltCounter = 1;
        hudFlags->Flags.NormalScoreboard = 0;
      }
    }
	}

	// show exp
	POKE_U32(0x0054ffc0, 0x0000102D);
  POKE_U16(0x00550054, 0);
}

//--------------------------------------------------------------------------
void destroyOmegaPads(void)
{
	int c = 0;
	GuberMoby* gm = guberMobyGetFirst();
	while (gm)
	{
		Moby* moby = gm->Moby;
		if (moby && moby->OClass == MOBY_ID_PICKUP_PAD && moby->PVar) {
			if (*(u8*)moby->PVar == 5) {
				guberMobyDestroy(moby);
				++c;
			}
		}

		gm = (GuberMoby*)gm->Guber.Prev;
	}
}

//--------------------------------------------------------------------------
void randomizeWeaponPickups(void)
{
	int i,j;
	GameOptions* gameOptions = gameGetOptions();
	char wepCounts[9];
	char wepEnabled[17];
	int pickupCount = 0;
	int pickupOptionCount = 0;
	memset(wepEnabled, 0, sizeof(wepEnabled));
	memset(wepCounts, 0, sizeof(wepCounts));

	if (gameOptions->WeaponFlags.DualVipers) { wepEnabled[2] = 1; pickupOptionCount++; }
	if (gameOptions->WeaponFlags.MagmaCannon) { wepEnabled[3] = 1; pickupOptionCount++; }
	if (gameOptions->WeaponFlags.Arbiter) { wepEnabled[4] = 1; pickupOptionCount++; }
	if (gameOptions->WeaponFlags.FusionRifle) { wepEnabled[5] = 1; pickupOptionCount++; }
	if (gameOptions->WeaponFlags.MineLauncher) { wepEnabled[6] = 1; pickupOptionCount++; }
	if (gameOptions->WeaponFlags.B6) { wepEnabled[7] = 1; pickupOptionCount++; }
	if (gameOptions->WeaponFlags.Holoshield) { wepEnabled[16] = 1; pickupOptionCount++; }
	if (gameOptions->WeaponFlags.Flail) { wepEnabled[12] = 1; pickupOptionCount++; }
	if (gameOptions->WeaponFlags.Chargeboots && gameOptions->GameFlags.MultiplayerGameFlags.SpawnWithChargeboots == 0) { wepEnabled[13] = 1; pickupOptionCount++; }

	if (pickupOptionCount > 0) {
		Moby* moby = mobyListGetStart();
		Moby* mEnd = mobyListGetEnd();

		while (moby < mEnd) {
			if (moby->OClass == MOBY_ID_WEAPON_PICKUP && moby->PVar) {
				
				int target = pickupCount / pickupOptionCount;
				int gadgetId = 1;
				if (target < 2) {
					do { j = rand(pickupOptionCount); } while (wepCounts[j] != target);

					++wepCounts[j];

					i = -1;
					do
					{
						++i;
						if (wepEnabled[i])
							--j;
					} while (j >= 0);

					gadgetId = i;
				}

				// set pickup
#if LOG_STATS2
				DPRINTF("setting pickup at %08X to %d\n", (u32)moby, gadgetId);
#endif
				((void (*)(Moby*, int))0x0043A370)(moby, gadgetId);

				++pickupCount;
			}

			++moby;
		}
	}
}

//--------------------------------------------------------------------------
void setWeaponPickupRespawnTime(void)
{
	GameSettings* gameSettings = gameGetSettings();

	// compute pickup respawn time in ms
	int respawnTimeOffset = WEAPON_PICKUP_PLAYER_RESPAWN_TIME_OFFSETS[(gameSettings->PlayerCountAtStart <= 0 ? 1 : gameSettings->PlayerCountAtStart) - 1];

	Moby* moby = mobyListGetStart();
	Moby* mEnd = mobyListGetEnd();

	while (moby < mEnd) {
		if (moby->OClass == MOBY_ID_WEAPON_PICKUP && moby->PVar) {

			// hide if wrench
			// wrenches are set as pickup id when we've reached the max number of
			// pickups per gadget
			int gadgetId = *(int*)moby->PVar;
			int slotId = weaponIdToSlot(gadgetId) - 1;
			if (gadgetId == 1) {
				moby->Position[2] = 0;
			}
			else if (slotId >= 0 && slotId < 8) {
				int weaponBaseRespawnTime = WEAPON_PICKUP_BASE_RESPAWN_TIMES[slotId];

				// otherwise set cooldown by configuration
        int ms = (weaponBaseRespawnTime - respawnTimeOffset) * mapConfig->WeaponPickupCooldownFactor * TIME_SECOND;
				POKE_U32((u32)moby->PVar + 0x0C, ms);
			}
		}
		++moby;
	}
}

//--------------------------------------------------------------------------
int whoKilledMeHook(Player* player, Moby* moby, int b)
{
  if (!moby)
    return 0;

  // only allow mobs or special mobys
  if (mobyIsMob(moby) || moby->Bolts == -1) {
    //DPRINTF("%08X %f %d %d\n", (u32)moby, player->Health, player->PlayerState, playerIsDead(player));


    return ((int (*)(Player*, Moby*, int))0x005dff08)(player, moby, b);
  }

	return 0;
}

//--------------------------------------------------------------------------
int onMobyPlayDesiredSound(int sound, int a1, Moby* moby)
{
  // catch mobs
  if (mobyIsMob(moby)) {
    int midx = ((struct MobPVar*)moby->PVar)->MobVars.SpawnParamsIdx;
    int sidx = sound % MOBS_PLAY_SOUND_COOLDOWN_MAX_SOUNDIDS;
    if (mobPlaySoundCooldownTicks[midx][sidx]) return -1;
    mobPlaySoundCooldownTicks[midx][sidx] = MOBS_PLAY_SOUND_COOLDOWN;
  }

  // pass to mobyPlaySound
  return mobyPlaySound(sound, a1, moby);
}

//--------------------------------------------------------------------------
Moby* FindMobyOrSpawnBox(int oclass, int defaultToSpawnpointId)
{
	// find
	Moby* m = mobyFindNextByOClass(mobyListGetStart(), oclass);
	
	// if can't find moby then just spawn a beta box at a spawn point
	if (!m) {
		SpawnPoint* sp = spawnPointGet(defaultToSpawnpointId);

		//
		m = mobySpawn(MOBY_ID_BETA_BOX, 0);
		vector_copy(m->Position, &sp->M0[12]);

#if DEBUG
		printf("could not find oclass %04X... spawned box (%08X) at ", oclass, (u32)m);
		vector_print(&sp->M0[12]);
		printf("\n");
#endif
	}

	return m;
}

//--------------------------------------------------------------------------
void spawnUpgrades(void)
{
	VECTOR pos,rot;
	int i,j;
	int r;
	int bakedUpgradeSpawnpointCount = 0;
	char upgradeBakedSpawnpointIdx[UPGRADE_COUNT];

  // only spawn if host
  if (!gameAmIHost())
    return;

	// initialize spawnpoint idx to -1
	// and set moby ref to NULL
	for (i = 0; i < UPGRADE_COUNT; ++i) {
		upgradeBakedSpawnpointIdx[i] = -1;
		State.UpgradeMobies[i] = NULL;
	}

	// count number of baked spawnpoints used for upgrade
  SurvivalBakedConfig_t* bakedConfig = mapConfig->BakedConfig;
	for (i = 0; i < BAKED_SPAWNPOINT_COUNT; ++i) {
		if (bakedConfig->BakedSpawnPoints[i].Type == BAKED_SPAWNPOINT_UPGRADE)
			++bakedUpgradeSpawnpointCount;
	}

	// find free baked spawn idx
	for (i = 0; i < UPGRADE_COUNT; ++i) {

		// if no more free spawnpoints then exit
		if (!bakedUpgradeSpawnpointCount)
			break;

		// randomly select point
		r = 1 + rand(BAKED_SPAWNPOINT_COUNT);
		j = 0;
		while (r) {
			j = (j + 1) % BAKED_SPAWNPOINT_COUNT;
			if (bakedConfig->BakedSpawnPoints[j].Type == BAKED_SPAWNPOINT_UPGRADE && !charArrayContains(upgradeBakedSpawnpointIdx, UPGRADE_COUNT, j))
				--r;
		}

		// assign point to this upgrade
		// decrement list
		upgradeBakedSpawnpointIdx[i] = j;
		--bakedUpgradeSpawnpointCount;
	}

	// spawn
	for (i = 0; i < sizeof(UpgradesEnabled); ++i) {
    int upgradeId = UpgradesEnabled[i];
		int bakedSpIdx = upgradeBakedSpawnpointIdx[upgradeId];
		if (bakedSpIdx < 0)
			continue;

    // don't spawn medic on solo runs
    if (State.ActivePlayerCount == 1 && upgradeId == UPGRADE_MEDIC)
      continue;

		SurvivalBakedSpawnpoint_t* sp = &bakedConfig->BakedSpawnPoints[bakedSpIdx];
		memcpy(pos, sp->Position, sizeof(float)*3);
		memcpy(rot, sp->Rotation, sizeof(float)*3);

    // spawn
    if (mapConfig && mapConfig->CreateUpgradePickupFunc)
		  mapConfig->CreateUpgradePickupFunc(pos, rot, upgradeId);
	}
}

//--------------------------------------------------------------------------
void spawnDemonBell(void)
{
  int i;
  
  // only spawn if host
  if (!gameAmIHost())
    return;

	// count number of baked spawnpoints used for upgrade
  SurvivalBakedConfig_t* bakedConfig = mapConfig->BakedConfig;
	for (i = 0; i < BAKED_SPAWNPOINT_COUNT; ++i) {
		if (bakedConfig->BakedSpawnPoints[i].Type == BAKED_SPAWNPOINT_DEMON_BELL) {
      demonbellCreate(bakedConfig->BakedSpawnPoints[i].Position, State.DemonBellCount);
      State.DemonBellCount += 1;
    }
	}
}

//--------------------------------------------------------------------------
void resetRoundState(void)
{
  int i;
	int gameTime = gameGetTime();

	// 
	State.RoundMaxMobCount = MAX_MOBS_BASE + (int)(MAX_MOBS_ROUND_WEIGHT * (1 + State.RoundNumber));
	State.RoundMaxSpawnedAtOnce = MAX_MOBS_ALIVE_REAL;
	State.RoundInitialized = 0;
	State.RoundStartTime = gameTime;
	State.RoundCompleteTime = 0;
	State.RoundEndTime = 0;
	State.RoundSpawnTicker = 0;
	State.RoundSpawnTickerCounter = 0;
	State.RoundNextSpawnTickerCounter = randRangeInt(MOB_SPAWN_BURST_MIN + (State.RoundNumber * MOB_SPAWN_BURST_MIN_INC_PER_ROUND), MOB_SPAWN_BURST_MAX + (State.RoundNumber * MOB_SPAWN_BURST_MAX_INC_PER_ROUND));

	State.MobStats.TotalAlive = 0;
  State.MobStats.TotalSpawnedThisRound = 0;
  memset(State.MobStats.NumAlive, 0, sizeof(State.MobStats.NumAlive));
  memset(State.MobStats.NumSpawnedThisRound, 0, sizeof(State.MobStats.NumSpawnedThisRound));

  // reset revive counters
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    State.PlayerStates[i].State.TimesRevivedSinceRoundStart = 0;
  }

  // reset bank
  if (State.Bankbox) {
    struct BankBoxPVar* bankPvars = (struct BankBoxPVar*)State.Bankbox->PVar;
    bankPvars->BoltsAtStartOfRound = bankPvars->TotalBolts;
    bankPvars->BoltsDepositThisRound = 0;
    bankPvars->BoltsWithdrawnThisRound = 0;
#if LOG_STATS2
    DPRINTF("bank vestable bolts this round:%d\n", bankPvars->BoltsAtStartOfRound);
#endif
  }

	// 
	if (State.RoundIsSpecial) {
		State.RoundMaxSpawnedAtOnce = mapConfig->SpecialRoundParams[State.RoundSpecialIdx].MaxSpawnedAtOnce;
    State.RoundMaxMobCount *= mapConfig->SpecialRoundParams[State.RoundSpecialIdx].SpawnCountFactor;

    // set max mob count to MaxSpawnedPerRound if every mob param has round limit
    // ie if the special round only has 1 mob param, and that mob is set to only spawn 1 per round
    // then set RoundMaxMobCount to 1 so that the round ends when that mob dies
    int maxPerRound = 0;
    for (i = 0; i < mapConfig->SpecialRoundParams[State.RoundSpecialIdx].SpawnParamCount; ++i) {
      int spawnParamIdx = mapConfig->SpecialRoundParams[State.RoundSpecialIdx].SpawnParamIds[i];
      if (mapConfig->DefaultSpawnParams[spawnParamIdx].MaxSpawnedPerRound <= 0) {
        maxPerRound = State.RoundMaxMobCount;
        break;
      }

      maxPerRound += mapConfig->DefaultSpawnParams[spawnParamIdx].MaxSpawnedPerRound;
    }

    State.RoundMaxMobCount = maxPerRound;
	}

	// 
	State.RoundInitialized = 1;
}

//--------------------------------------------------------------------------
void initialize(PatchStateContainer_t* gameState)
{
	static int startDelay = TPS * 0.2;
	static int waitingForClientsReady = 0;
  static int firstTime = 1;
	char hasTeam[10] = {0,0,0,0,0,0,0,0,0,0};
	Player** players = playerGetAll();
	int i, j;

  if (firstTime) {
    firstTime = 0;
    
    // clear state
    memset(&State, 0, sizeof(State));
    for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
      State.PlayerStates[i].State.Item = -1;
    }
  }

	// Disable normal game ending
	*(u32*)0x006219B8 = 0;	// survivor (8)
	*(u32*)0x00620F54 = 0;	// time end (1)
	*(u32*)0x00621568 = 0;	// kills reached (2)
	*(u32*)0x006211A0 = 0;	// all enemies leave (9)
  *(u32*)0x006210D8 = 0;	// all enemies leave (9)

  // spawn area mod explosion on each ricochet of the v10 vipers
  //HOOK_JAL(0x003C283C, &onV10VipersHitSurface);

  // disable holoshields from disappearing
  //*(u16*)0x00401478 = 2;

  // disable team based holoshield toggling
  // enables players to shoot through eachothers shields
  POKE_U32(0x005A3830, 0x2402FFFF);
  POKE_U32(0x005A3834, 0x14500018);

  // force holoshield hit testing
  *(u32*)0x00401194 = 0;
  *(u32*)0x003FFDE8 = 0x1000000D;

	// Disables end game draw dialog
	*(u32*)0x0061fe84 = 0;

	// sets start of colored weapon icons to v10
	*(u32*)0x005420E0 = 0x2A020009;
	*(u32*)0x005420E4 = 0x10400005;

	// Removes MP check on HudAmmo weapon icon color (so v99 is pinkish)
	*(u32*)0x00542114 = 0;

	// Enable sniper to shoot through multiple enemies
	*(u32*)0x003FC2A8 = 0;

	// Disable sniper shot corn
	//*(u32*)0x003FC410 = 0;
	*(u32*)0x003FC5A8 = 0;

	// Fix v10 arb overlapping shots
	*(u32*)0x003F2E70 = 0x24020000;

  // hook when MobyPlayDesiredSound plays a sound for a moby
  // lets us reduce the # of mob sounds
  HOOK_JAL(0x004f790c, &onMobyPlayDesiredSound);
  HOOK_J(0x004fa800, &onMobyPlayDesiredSound);

  // disable timebase query percentile filter
  // always accept remote time
  POKE_U32(0x01eabd60, 0);

  // Fix locals using same bolt count
  HOOK_J(0x00557C00, &_getLocalBolts);
  POKE_U32(0x00557C04, 0);

  // fix emp
  //POKE_U32(0x0042075C, 0x2C42010B);
  //POKE_U32(0x00420610, 0x24050001);
  //HOOK_JAL(0x00420634, &onEmpHitMoby);
  //HOOK_JAL(0x004209bc, &onEmpExplode);
  //HOOK_JAL(0x0041fb8c, &onEmpFired);

  // hook when v10 mag shot hits
  HOOK_JAL(0x0044B374, &onV10MagDamageMoby);

	// Change mine update function to ours
  u32 mineUpdateFunc = 0x003c6c28;
  u32* updateFuncs = (u32*)0x00249980;
  for (i = 0; i < 113; ++i) {
    if (*updateFuncs == mineUpdateFunc) { *updateFuncs = (u32)&customMineMobyUpdate; }
    updateFuncs++;
  }

	// Change bangelize weapons call to ours
	*(u32*)0x005DD890 = 0x0C000000 | ((u32)&customBangelizeWeapons >> 2);

	// Enable weapon version and v10 name variant in places that display weapon name
	*(u32*)0x00541850 = 0x08000000 | ((u32)&customGetGadgetVersionName >> 2);
	*(u32*)0x00541854 = 0;

	// patch who killed me to prevent damaging others
	*(u32*)0x005E07C8 = 0x0C000000 | ((u32)&whoKilledMeHook >> 2);
	*(u32*)0x005E11B0 = *(u32*)0x005E07C8;

  // patch mobs pushing you into walls and killing you
  POKE_U32(0x005e4188, 0);
  POKE_U32(0x005e419c, 0);
  HOOK_JAL(0x005e41bc, &playerOnPushedIntoWall);

  // patch quad/shield cooldown timer
  HOOK_JAL(0x004468D8, &setPlayerQuadCooldownTimer);
  POKE_U32(0x004468E4, 0);
	HOOK_JAL(0x00446948, &setPlayerShieldCooldownTimer);
  POKE_U32(0x00446954, 0);

  // disable guber event delay until createTime+relDispatchTime reached
  // when players desync, their net time falls behind everyone else's
  // causing events that they receive to be delayed for long periods of time
  // leading to even more desyncing issues
  // since survival can cause a lot of frame lag, especially for players on emu/dzo
  // this fix is required to ensure that important mob guber events trigger on everyone's screen
  POKE_U32(0x00611518, 0x24040000);

	// set default ammo for flail to 8
	//*(u8*)0x0039A3B4 = 8;

	// disable targeting players
	*(u32*)0x005F8A80 = 0x10A20002;
	*(u32*)0x005F8A84 = 0x0000102D;
	*(u32*)0x005F8A88 = 0x24440001;

  // clear if magic not valid
  if (mapConfig->Magic != MAP_CONFIG_MAGIC) {
    memset(mapConfig, 0, sizeof(struct SurvivalMapConfig));
    mapConfig->Magic = MAP_CONFIG_MAGIC;
    printf("clear\n");
  }

  // write map config
  mapConfig->State = &State;
  mapConfig->SpawnGetRandomPointFunc = &spawnGetRandomPoint;
  mapConfig->UpgradePlayerWeaponFunc = &mapUpgradePlayerWeaponHandler;
  mapConfig->PushSnackFunc = &pushSnack;
  mapConfig->PopulateSpawnArgsFunc = &populateSpawnArgsFromConfig;
  mapConfig->ModeCreateMobFunc = &mobCreate;
  mapConfig->ModeMobNukeFunc = &mobNuke;
  mapConfig->ModeSetDoublePointsFunc = &setDoublePoints;
  mapConfig->ModeSetDoubleXPFunc = &setDoubleXP;
  mapConfig->ModeSetFreezeMobsFunc = &setFreeze;
  mapConfig->ModeRevivePlayerFunc = &playerRevive;

	// custom damage cooldown time
	//POKE_U16(0x0060583C, DIFFICULTY_HITINVTIMERS[gameConfig->survivalConfig.difficulty]);
	//POKE_U16(0x0060582C, DIFFICULTY_HITINVTIMERS[gameConfig->survivalConfig.difficulty]);

	// Hook custom net events
	netInstallCustomMsgHandler(CUSTOM_MSG_ROUND_COMPLETE, &onSetRoundCompleteRemote);
	netInstallCustomMsgHandler(CUSTOM_MSG_ROUND_START, &onSetRoundStartRemote);
	netInstallCustomMsgHandler(CUSTOM_MSG_WEAPON_UPGRADE, &onPlayerUpgradeWeaponRemote);
	netInstallCustomMsgHandler(CUSTOM_MSG_REVIVE_PLAYER, &onPlayerReviveRemote);
	netInstallCustomMsgHandler(CUSTOM_MSG_PLAYER_DIED, &onSetPlayerDeadRemote);
	netInstallCustomMsgHandler(CUSTOM_MSG_PLAYER_SET_WEAPON_MODS, &onSetPlayerWeaponModsRemote);
	netInstallCustomMsgHandler(CUSTOM_MSG_PLAYER_SET_STATS, &onSetPlayerStatsRemote);
	netInstallCustomMsgHandler(CUSTOM_MSG_PLAYER_SET_DOUBLE_POINTS, &onSetPlayerDoublePointsRemote);
	netInstallCustomMsgHandler(CUSTOM_MSG_PLAYER_SET_DOUBLE_XP, &onSetPlayerDoubleXPRemote);
	netInstallCustomMsgHandler(CUSTOM_MSG_PLAYER_SET_FREEZE, &onSetFreezeRemote);
	netInstallCustomMsgHandler(CUSTOM_MSG_PLAYER_USE_ITEM, &onPlayerUseItemRemote);
	netInstallCustomMsgHandler(CUSTOM_MSG_MOB_UNRELIABLE_MSG, &mobOnUnreliableMsgRemote);
	netInstallCustomMsgHandler(CUSTOM_MSG_WEAPON_PRESTIGE, &onPlayerPrestigeWeaponRemote);
  netInstallCustomMsgHandler(CUSTOM_MSG_INTERACT_BANK_BOX, &onPlayerInteractBankBoxRemote);
  netInstallCustomMsgHandler(CUSTOM_MSG_WITHDRAWN_BANK_BOX, &onPlayerWithdrawnBankBoxRemote);
  netInstallCustomMsgHandler(CUSTOM_MSG_SET_ROUND_50_TIME, &onSetRound50TimeRemote);

	// set game over string
	strncpy(uiMsgString(0x3477), SURVIVAL_GAME_OVER, strlen(SURVIVAL_GAME_OVER)+1);
	strncpy(uiMsgString(0x3152), SURVIVAL_UPGRADE_MESSAGE, strlen(SURVIVAL_UPGRADE_MESSAGE)+1);
	strncpy(uiMsgString(0x3153), SURVIVAL_REVIVE_MESSAGE, strlen(SURVIVAL_REVIVE_MESSAGE)+1);
	strncpy(uiMsgString(0x24B7), SURVIVAL_HEALTH_GUN, strlen(SURVIVAL_HEALTH_GUN)+1);

	// disable v2s and packs
	cheatsApplyNoV2s();
	cheatsApplyNoPacks();

	// change hud
	forcePlayerHUD();
  setPlayerEXP(0, 0);
  setPlayerEXP(1, 0);

	// set bolts to 0
  memset(BoltCounts, 0, sizeof(BoltCounts));

	// destroy omega mod pads
	destroyOmegaPads();

  // prevent player from doing anything
  padDisableInput();

	// 
	mobInitialize();
  demonbellInitialize();

  //
  memset(playerStates, 0, sizeof(playerStates));
  memset(playerStateTimers, 0, sizeof(playerStateTimers));

  if (startDelay) {
    --startDelay;
    return;
  }

  // wait for all clients to be ready
  // or for 15 seconds
  if (!gameState->AllClientsReady && waitingForClientsReady < (5 * TPS)) {
    uiShowPopup(0, "Waiting For Players...");
    ++waitingForClientsReady;
    return;
  }

  // hide waiting for players popup
  hudHidePopup();

  //
  mapConfig->ClientsReady = 1;

  // re-enable input
  padEnableInput();

  bubbleInit();

	memset(defaultSpawnParamsCooldowns, 0, sizeof(defaultSpawnParamsCooldowns));
  memset(snackItems, 0, sizeof(snackItems));

	// initialize player states
	State.LocalPlayerState = NULL;
	State.NumTeams = 0;
	State.ActivePlayerCount = 0;
	for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
		Player * p = players[i];
    State.PlayerStates[i].RevivingPlayerId = -1;

#if PAYDAY
		State.PlayerStates[i].State.CurrentTokens = 1000;
#endif

		if (p) {

			// is local
			State.PlayerStates[i].IsLocal = p->IsLocal;
			if (p->IsLocal && !State.LocalPlayerState)
				State.LocalPlayerState = &State.PlayerStates[i];
				
			// clear alpha mods
			if (p->GadgetBox && !FirstTimeInitialized)
				memset(p->GadgetBox->ModBasic, 0, sizeof(p->GadgetBox->ModBasic));

			if (!hasTeam[p->Team]) {
				State.NumTeams++;
				hasTeam[p->Team] = 1;
			}

#if PAYDAY
      p->GadgetBox->ModBasic[0] = 64;
      p->GadgetBox->ModBasic[1] = 64;
      p->GadgetBox->ModBasic[2] = 64;
      p->GadgetBox->ModBasic[3] = 64;
      p->GadgetBox->ModBasic[4] = 64;
      p->GadgetBox->ModBasic[5] = 64;
      p->GadgetBox->ModBasic[6] = 64;
      p->GadgetBox->ModBasic[7] = 64;
      State.PlayerStates[i].State.Bolts = 10000000;
#endif

			++State.ActivePlayerCount;
		}
	}

  // solo run start with self revive
  if (State.ActivePlayerCount == 1 && !FirstTimeInitialized) {
    State.PlayerStates[playerGetFromSlot(0)->PlayerId].State.Item = MYSTERY_BOX_ITEM_REVIVE_TOTEM;
  }

	// initialize weapon data
	WeaponDefsData* gunDefs = weaponGetGunLevelDefs();
	for (i = 0; i < 7; ++i) {
		for (j = 0; j < 10; ++j) {
			gunDefs[i].Entries[j].MpLevelUpExperience = VENDOR_MAX_WEAPON_LEVEL;
		}
	}
	WeaponDefsData* flailDefs = weaponGetFlailLevelDefs();
	for (j = 0; j < 10; ++j) {
		flailDefs->Entries[j].MpLevelUpExperience = VENDOR_MAX_WEAPON_LEVEL;
	}

	// randomize weapon picks
	if (State.IsHost) {
		//randomizeWeaponPickups();
	}

	// find vendor
#if defined(VENDOR_OCLASS)
	State.Vendor = FindMobyOrSpawnBox(VENDOR_OCLASS, 1);
#else
	State.Vendor = FindMobyOrSpawnBox(0, 1);
#endif

	// find al
#if defined(BIGAL_OCLASS)
	State.BigAl = FindMobyOrSpawnBox(BIGAL_OCLASS, 2);
#else
	State.BigAl = FindMobyOrSpawnBox(0, 2);
#endif

	// find prestige machine
#if defined(PRESTIGEMACHINE_OCLASS)
	State.PrestigeMachine = FindMobyOrSpawnBox(PRESTIGEMACHINE_OCLASS, 2);
#else
	State.PrestigeMachine = FindMobyOrSpawnBox(0, 2);
#endif

#if UPGRADES
	// spawn upgrades
	spawnUpgrades();
#endif

#if DEMONBELL
	// spawn demonbell
	spawnDemonBell();
#endif

  State.Bankbox = NULL;
	DPRINTF("vendor %08X\n", (u32)State.Vendor);
	DPRINTF("big al %08X\n", (u32)State.BigAl);
	DPRINTF("prestige machine %08X\n", (u32)State.PrestigeMachine);

  // set vendor icon
  if (State.Vendor) {
    State.Vendor->Bangles = 0x8;
  }

	// initialize state
	State.MobStats.MobsDrawnCurrent = 0;
	State.MobStats.MobsDrawnLast = 0;
	State.MobStats.MobsDrawGameTime = 0;
  State.MobStats.TotalSpawned = 0;
	State.Freeze = 0;
	State.TimeOfFreeze = 0;
	State.RoundIsSpecial = 0;
	State.RoundSpecialIdx = 0;
	State.DropCooldownTicks = DROP_COOLDOWN_TICKS_MAX; // try and stop drops from spawning immediately
	State.Difficulty = mapConfig->BakedConfig->Difficulty;
  
	if (!FirstTimeInitialized) {
    State.InitializedTime = gameGetTime();
  }

#if STARTROUND
	State.RoundNumber = STARTROUND - 1;
	//State.RoundIsSpecial = 1;
	//State.RoundSpecialIdx = 4;
  int xp[GAME_MAX_PLAYERS];

#if !PAYDAY
  for (j = 0; j < GAME_MAX_PLAYERS; ++j) {
    State.PlayerStates[j].State.Bolts = 10000;
    xp[j] = 1500;
  }
#endif

  // 
  for (i = 0; i < State.RoundNumber; ++i) {
    State.MobStats.TotalSpawned += MAX_MOBS_BASE + (int)(MAX_MOBS_ROUND_WEIGHT * (1 + i));
    
#if !PAYDAY
    for (j = 0; j < GAME_MAX_PLAYERS; ++j) {
      State.PlayerStates[j].State.Bolts *= 1.225;
      //State.PlayerStates[j].State.TotalBolts = State.PlayerStates[j].State.Bolts;
      xp[j] *= 1.125;
    }
#endif
  }

  for (j = 0; j < GAME_MAX_PLAYERS; ++j) {
    //playerRewardXp(j, -1, xp[j]);
    State.PlayerStates[j].State.XP = xp[j];
    State.PlayerStates[j].State.Upgrades[UPGRADE_HEALTH] = STARTROUND / 4;
  }

  DPRINTF("Skipped to Round #%d with %d mobs spawned\n", State.RoundNumber + 1, State.MobStats.TotalSpawned);
#endif

	resetRoundState();

  // force initial delay on first round
  State.RoundSpawnTicker = TPS * 3;

	// scale default mob params by difficulty
	// for (i = 0; i < mapConfig->DefaultSpawnParamsCount; ++i)
	// {
	// 	struct MobConfig* config = &mapConfig->DefaultSpawnParams[i].Config;
	// 	config->Bolts /= State.Difficulty;
	// 	config->MaxHealth *= State.Difficulty;
	// 	config->Health *= State.Difficulty;
	// 	config->Damage *= State.Difficulty;
	// }

#if FIXEDTARGET
  FIXEDTARGETMOBY = mobySpawn(0xE7D, 0);
  FIXEDTARGETMOBY->Position[0] = 599.368225;
  FIXEDTARGETMOBY->Position[1] = 902.109131;
  FIXEDTARGETMOBY->Position[2] = 505.583221;
#endif

	Initialized = 1;
  FirstTimeInitialized = 1;
}

//--------------------------------------------------------------------------
void updateGameState(PatchStateContainer_t * gameState)
{
	int i,j;

	// game state update
	if (gameState->UpdateGameState)
	{
		gameState->GameStateUpdate.RoundNumber = State.RoundNumber + 1;
	}

  if (isInGame()) {
    // compute round 50 completed time
    checkForRound50Time();

    // update custom game stats after each round
    static int roundCompleted = 0;
    if (roundCompleted != State.RoundNumber && gameAmIHost()) {
      gameState->UpdateCustomGameStats = 1;
      roundCompleted = State.RoundNumber;
    }
  }

	// stats
	if (gameState->UpdateCustomGameStats)
	{
    gameState->CustomGameStatsSize = sizeof(struct SurvivalGameData);
		struct SurvivalGameData* sGameData = (struct SurvivalGameData*)gameState->CustomGameStats.Payload;
		sGameData->RoundNumber = State.RoundNumber;
		sGameData->Version = 0x00000006;
    sGameData->Round50Time = State.Round50Time;

    // set mob ids
    for (i = 0; i < mapConfig->DefaultSpawnParamsCount && i < MAX_MOB_SPAWN_PARAMS; ++i) {
      sGameData->MobIds[i] = (short)mapConfig->DefaultSpawnParams[i].StatId;
    }

    // set per player stats
		for (i = 0; i < GAME_MAX_PLAYERS; ++i)
		{
			sGameData->Kills[i] = State.PlayerStates[i].State.Kills;
			sGameData->Revives[i] = State.PlayerStates[i].State.Revives;
			sGameData->TimesRevived[i] = State.PlayerStates[i].State.TimesRevived;
			sGameData->Points[i] = State.PlayerStates[i].State.TotalBolts * ((mapConfig && mapConfig->BakedConfig) ? mapConfig->BakedConfig->BoltRankMultiplier : 1);
			sGameData->BestRound[i] = State.PlayerStates[i].State.BestRound;
			sGameData->TimesRolledMysteryBox[i] = State.PlayerStates[i].State.TimesRolledMysteryBox;
			sGameData->TimesActivatedDemonBell[i] = State.PlayerStates[i].State.TimesActivatedDemonBell;
			sGameData->TimesActivatedPower[i] = State.PlayerStates[i].State.TimesActivatedPower;
			sGameData->TokensUsedOnGates[i] = State.PlayerStates[i].State.TokensUsedOnGates;

			for (j = 0; j < 8; ++j) {
				sGameData->AlphaMods[i][j] = (u8)State.PlayerStates[i].State.AlphaMods[j];
      }

			memcpy(sGameData->BestWeaponLevel[i], State.PlayerStates[i].State.BestWeaponLevel, sizeof(State.PlayerStates[i].State.BestWeaponLevel));
			memcpy(sGameData->KillsPerMob[i], State.PlayerStates[i].State.KillsPerMob, sizeof(State.PlayerStates[i].State.KillsPerMob));
			memcpy(sGameData->DeathsByMob[i], State.PlayerStates[i].State.DeathsByMob, sizeof(State.PlayerStates[i].State.DeathsByMob));
			memcpy(sGameData->PlayerUpgrades[i], State.PlayerStates[i].State.Upgrades, sizeof(State.PlayerStates[i].State.Upgrades));
		}
	}
}

//--------------------------------------------------------------------------
void gameStart(struct GameModule * module, PatchStateContainer_t * gameState)
{
	GameSettings * gameSettings = gameGetSettings();
	GameOptions * gameOptions = gameGetOptions();
	Player ** players = playerGetAll();
	Player* localPlayer = playerGetFromSlot(0);
	int i;
	char buffer[64];
	int gameTime = gameGetTime();
  float demonBellFactor = 0;

  if (State.DemonBellCount > 0) {
    demonBellFactor = 1 - powf(1 - (State.RoundDemonBellCount / (float)State.DemonBellCount), 2);
  }

	// first
	dlPreUpdate();

	// 
	updateGameState(gameState);

	// Ensure in game
	if (!gameSettings || !isInGame())
		return;

	// Determine if host
	State.IsHost = gameAmIHost();

  // 
  playerConfig = gameState->Config;

	if (!Initialized) {
		initialize(gameState);
		return;
	}

  // get local player data
  struct SurvivalPlayer* localPlayerData = &State.PlayerStates[localPlayer->PlayerId];

	// force weapon pickup respawn times
	setWeaponPickupRespawnTime();

#if LOG_STATS
	static int statsTicker = 0;
	if (statsTicker <= 0) {
		DPRINTF("liveMobCount:%d totalSpawnedThisRound:%d roundNumber:%d roundSpawnTicker:%d\n", State.MobStats.TotalAlive, State.MobStats.TotalSpawnedThisRound, State.RoundNumber, State.RoundSpawnTicker);
		statsTicker = 60 * 15;
	} else {
		--statsTicker;
	}
#endif

#if DEBUG_MPASS
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    State.PlayerStates[i].State.Item = MYSTERY_BOX_ITEM_INVISIBILITY_CLOAK;
    State.PlayerStates[i].State.BlessingSlots = 3;
    State.PlayerStates[i].State.ItemBlessings[0] = BLESSING_ITEM_AMMO_REGEN;
    State.PlayerStates[i].State.ItemBlessings[1] = BLESSING_ITEM_HEALTH_REGEN;
    State.PlayerStates[i].State.ItemBlessings[2] = BLESSING_ITEM_BULL;
  }
#endif

#if DEBUG_PERKS
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    State.PlayerStates[i].State.ItemStackable[STACKABLE_ITEM_EXTRA_JUMP] = 5;
    State.PlayerStates[i].State.ItemStackable[STACKABLE_ITEM_LOW_HEALTH_DMG_BUF] = 1;
    State.PlayerStates[i].State.ItemStackable[STACKABLE_ITEM_EXTRA_SHOT] = 2;
    //State.PlayerStates[i].State.ItemStackable[STACKABLE_ITEM_HOVERBOOTS] = 1;
    State.PlayerStates[i].State.ItemStackable[STACKABLE_ITEM_VAMPIRE] = 3;
    State.PlayerStates[i].State.ItemStackable[STACKABLE_ITEM_ALPHA_MOD_AMMO] = 5;
    State.PlayerStates[i].State.ItemStackable[STACKABLE_ITEM_ALPHA_MOD_SPEED] = 5;
    State.PlayerStates[i].State.ItemStackable[STACKABLE_ITEM_ALPHA_MOD_AREA] = 5;
    State.PlayerStates[i].State.ItemStackable[STACKABLE_ITEM_ALPHA_MOD_IMPACT] = 5;
    State.PlayerStates[i].State.Upgrades[UPGRADE_SPEED] = UpgradeMax[UPGRADE_SPEED];
  }
#endif

#if DEBUG_SOUNDS
  {
    static int aaa = 0;
    const int mobyClass = 0x2751;
		if (padGetButtonDown(0, PAD_RIGHT) > 0) {
			aaa += 1;
			printf("%d\n", aaa);
      mobyPlaySoundByClass(aaa, 0, localPlayer->PlayerMoby, mobyClass);
		} else if (padGetButtonDown(0, PAD_LEFT) > 0) {
			aaa -= 1;
			printf("%d\n", aaa);
      mobyPlaySoundByClass(aaa, 0, localPlayer->PlayerMoby, mobyClass);
		} else if (padGetButtonDown(0, PAD_UP) > 0) {
			printf("%d\n", aaa);
      mobyPlaySoundByClass(aaa, 0, localPlayer->PlayerMoby, mobyClass);
		}
  }
#endif

#if TRAILER
  static int trailerInit = 0;

  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    State.PlayerStates[i].State.Item = MYSTERY_BOX_ITEM_REVIVE_TOTEM;
    State.PlayerStates[i].State.CurrentTokens = 15;
    if (State.PlayerStates[i].State.Bolts < 100000)
      State.PlayerStates[i].State.Bolts = 1000000;
  }

  if (!trailerInit) {
    for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
      State.PlayerStates[i].State.Item = MYSTERY_BOX_ITEM_REVIVE_TOTEM;
      State.PlayerStates[i].State.BlessingSlots = 1;
      State.PlayerStates[i].State.ItemBlessings[0] = BLESSING_ITEM_AMMO_REGEN;
      State.PlayerStates[i].State.CurrentTokens = 15;
      State.PlayerStates[i].State.Bolts = 1000000;
      State.PlayerStates[i].State.Upgrades[UPGRADE_CRIT] = 30;
      State.PlayerStates[i].State.Upgrades[UPGRADE_HEALTH] = 5;
      State.PlayerStates[i].State.Upgrades[UPGRADE_SPEED] = 30;
      State.PlayerStates[i].State.Upgrades[UPGRADE_DAMAGE] = 10;
    }
    trailerInit = 1;
  }
#endif

#if MANUAL_SPAWN
  if (localPlayerHasInput())
	{
    
    // static int aaa = 0;
		// if (padGetButtonDown(0, PAD_RIGHT) > 0) {
		// 	aaa += 1;
		// 	DPRINTF("%d\n", aaa);
		// }
		// else if (padGetButtonDown(0, PAD_LEFT) > 0) {
		// 	aaa -= 1;
		// 	DPRINTF("%d\n", aaa);
		// }

		if (padGetButtonDown(0, PAD_DOWN) > 0) {
			static int manSpawnMobId = 0;

      //if (manSpawnMobId == 0) manSpawnMobId = 2;

      // force one mob type
      //manSpawnMobId = 0;
      //manSpawnMobId = 2;
			//manSpawnMobId = mapConfig->DefaultSpawnParamsCount - 2;
			//manSpawnMobId = mapConfig->DefaultSpawnParamsCount - 1;

      // skip invalid params
      while (mapConfig->DefaultSpawnParams[manSpawnMobId].Probability < 0) {
        manSpawnMobId = (manSpawnMobId + 1) % mapConfig->DefaultSpawnParamsCount;
      }

      // build spawn position
      VECTOR t;
      if (1 || !spawnGetRandomPoint(t, &mapConfig->DefaultSpawnParams[manSpawnMobId])) {
        VECTOR offset = {1,1,1,0};
        vector_scale(t, offset, mapConfig->DefaultSpawnParams[manSpawnMobId].Config.CollRadius*2);
        vector_add(t, t, localPlayer->PlayerPosition);
      }

      //DPRINTF("spawning mob type %d\n", manSpawnMobId);
      
      // spawn
			mobCreate(manSpawnMobId, t, 0, -1, 0, &mapConfig->DefaultSpawnParams[manSpawnMobId].Config);
      manSpawnMobId = (manSpawnMobId + 1) % mapConfig->DefaultSpawnParamsCount;
		}
    // else if (padGetButtonDown(0, PAD_L1 | PAD_RIGHT) > 0) {
    //   State.Freeze = 1;
    //   State.TimeOfFreeze = 0x6FFFFFFF;
    //   DPRINTF("freeze\n");
    // }
    // else if (padGetButtonDown(0, PAD_L1 | PAD_LEFT) > 0) {
    //   State.Freeze = 0;
    //   State.TimeOfFreeze = 0;
    //   DPRINTF("unfreeze\n");
    // }
	}
#endif

#if BENCHMARK
  {
    static int manSpawnMobId = 0;
    if (manSpawnMobId < MAX_MOBS_ALIVE)
    {
      VECTOR t = {396,606,434,0};
      
      t[0] += (manSpawnMobId % 8) * 2;
      t[1] += (manSpawnMobId / 8) * 2;

      int id = manSpawnMobId++ % mapConfig->DefaultSpawnParamsCount;
      mobCreate(id, t, 0, -1, 0, &mapConfig->DefaultSpawnParams[id].Config);
    }
  }
#endif

#if MANUAL_DROP_SPAWN
  if (localPlayerHasInput())
	{
		if (padGetButtonDown(0, PAD_UP) > 0) {
			static int manSpawnDropId = 0;
			static int manSpawnDropIdx = 0;
			manSpawnDropId = (manSpawnDropId + 1) % 5;
      //manSpawnDropId = DROP_NUKE;
			VECTOR t;
			vector_copy(t, localPlayer->PlayerPosition);
			t[0] += 5;

			State.DropCooldownTicks = 0;
			if (mapConfig && mapConfig->CreateMobDropFunc) mapConfig->CreateMobDropFunc(t, (enum DropType)manSpawnDropId, gameGetTime() + (30 * TIME_SECOND), localPlayer->Team);
			++manSpawnDropIdx;
		}
	}
#endif

	// ticks
	mobTick();
  bubbleTick();

  if (mapConfig && mapConfig->OnFrameTickFunc)
    mapConfig->OnFrameTickFunc();

  // tick down mob sound cooldown
  int j;
  for (i = 0; i < MAX_MOB_SPAWN_PARAMS; ++i) {
    for (j = 0; j < MOBS_PLAY_SOUND_COOLDOWN_MAX_SOUNDIDS; ++j) {
      if (mobPlaySoundCooldownTicks[i][j] > 0) {
        --mobPlaySoundCooldownTicks[i][j];
      }
    }
  }

  // handle inf ammo
  if (State.InfiniteAmmoStopTime > 0) {
    int timeUntilEnd = gameGetTime() - State.InfiniteAmmoStopTime;
    if (timeUntilEnd < 0) {
      gameGetOptions()->GameFlags.MultiplayerGameFlags.UnlimitedAmmo = 1;
    } else {
      gameGetOptions()->GameFlags.MultiplayerGameFlags.UnlimitedAmmo = 0;
      State.InfiniteAmmoStopTime = -1;

      // give everyone max ammo
      for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
        Player* player = players[i];
        if (!player || !player->SkinMoby || !player->GadgetBox)
          continue;

        int j = 0;
        for (j = 0; j < 8; ++j) {
          int weaponId = weaponSlotToId(j+1);
          struct GadgetEntry* gadgetEntry = &player->GadgetBox->Gadgets[weaponId];
          if (gadgetEntry->Level >= 0) {
            gadgetEntry->Ammo = playerGetWeaponMaxAmmo(player->GadgetBox, weaponId);
          }
        }
      }
    }
  }

  // draw upgrade counts
  for (i = 0; i < GAME_MAX_LOCALS; ++i) {
    Player* p = playerGetFromSlot(i);
    if (!p || !p->PlayerMoby) continue;
    struct SurvivalPlayer* playerData = &State.PlayerStates[p->PlayerId];

    if (gameIsStartMenuOpen(i) && !playerData->IsInWeaponsMenu) {
      int j;
      for (j = 0; j < sizeof(UpgradesEnabled); ++j) {
        int upgradeId = UpgradesEnabled[j];
        float x = 10;
        float y = 54 + (j * 32);

        Moby* upgradeMoby = State.UpgradeMobies[upgradeId];
        if (!upgradeMoby || !upgradeMoby->PVar) continue;
        struct UpgradePVar* upgradePVars = (struct UpgradePVar*)upgradeMoby->PVar;

        transformToSplitscreenPixelCoordinates(i, &x, &y);

        // draw count
        int count = playerData->State.Upgrades[upgradeId];
        snprintf(buffer, 32, "%d", count);
        gfxScreenSpaceText(x+2, y+8+2, 1, 1, 0x40000000, buffer, -1, 0);
        x = gfxScreenSpaceText(x,   y+8,   1, 1, 0x80C0C0C0, buffer, -1, 0);
        
        // draw icon
        gfxSetupGifPaging(0);
        u64 texId = gfxGetFrameTex(upgradePVars->TexId);
        gfxDrawSprite(x+2,   y+8,   16, 16, 0, 0, 32, 32, 0x80C0C0C0, texId);
        x += 16 + 4;
        gfxDoGifPaging();

        // draw factor
        switch (upgradeId)
        {
          case UPGRADE_HEALTH: snprintf(buffer, 32, "+%d", count * PLAYER_UPGRADE_HEALTH_FACTOR); break;
          case UPGRADE_DAMAGE: snprintf(buffer, 32, "+%.f%%", count * PLAYER_UPGRADE_DAMAGE_FACTOR * 100); break;
          case UPGRADE_SPEED: snprintf(buffer, 32, "+%.f%%", count * PLAYER_UPGRADE_SPEED_FACTOR * 100); break;
          case UPGRADE_CRIT: snprintf(buffer, 32, "%.f%%", count * PLAYER_UPGRADE_CRIT_FACTOR * 100); break;
        }
        gfxScreenSpaceText(x+6, y+8+2, 1, 1, 0x40000000, buffer, -1, 0);
        gfxScreenSpaceText(x+4,   y+8,   1, 1, 0x8000C0C0, buffer, -1, 0);
        
      }
    }
  }

  // draw hud stuff
  if (shouldDrawHud()) {

    if (!localPlayerData->IsInWeaponsMenu) {
      // draw round number
      char* roundStr = uiMsgString(0x25A9);
      gfxScreenSpaceText(31, 281, 0.7, 0.7, 0x40000000, roundStr, -1, 1);
      gfxScreenSpaceText(30, 280, 0.7, 0.7, 0x80E0E0E0, roundStr, -1, 1);
      snprintf(buffer, sizeof(buffer), "%d", State.RoundNumber + 1);
      gfxScreenSpaceText(31, 292, 1, 1, 0x40000000, buffer, -1, 1);
      gfxScreenSpaceText(30, 291, 1, 1, 0x8029E5E6, buffer, -1, 1);

      // draw double bolts
      if (localPlayer && State.PlayerStates[localPlayer->PlayerId].IsDoublePoints) {
        gfxScreenSpaceText(490+1, 40+1, 0.7, 0.7, 0x40000000, "x2", -1, 1);
        gfxScreenSpaceText(490+0, 40+0, 0.7, 0.7, 0x8029E5E6, "x2", -1, 1);
      }

      // draw double xp
      if (localPlayer && State.PlayerStates[localPlayer->PlayerId].IsDoubleXP) {
        gfxScreenSpaceText(208+1, 18+1, 0.7, 0.7, 0x40000000, "x2", -1, 1);
        gfxScreenSpaceText(208+0, 18+0, 0.7, 0.7, 0x8029E5E6, "x2", -1, 1);
      }

      // draw number of mobs spawned
      char* enemiesStr = "ENEMIES";
      gfxScreenSpaceText(31, 241, 0.7, 0.7, 0x40000000, enemiesStr, -1, 1);
      gfxScreenSpaceText(30, 240, 0.7, 0.7, 0x80E0E0E0, enemiesStr, -1, 1);
      //snprintf(buffer, sizeof(buffer), "%d", State.MobStats.TotalAlive + State.MobStats.TotalSpawning);
      snprintf(buffer, sizeof(buffer), "%d", (State.RoundMaxMobCount - State.MobStats.TotalSpawnedThisRound) + State.MobStats.TotalAlive + State.MobStats.TotalSpawning);
      gfxScreenSpaceText(31, 252, 1, 1, 0x40000000, buffer, -1, 1);
      gfxScreenSpaceText(30, 251, 1, 1, 0x8029E5E6, buffer, -1, 1);
    }

    // draw timer
    drawTimer(gameGetTime() - State.InitializedTime);

    // draw player specific values
    // TODO: add support for 3,4 local players
    for (i = 0; i < GAME_MAX_LOCALS; ++i) {
      Player* p = playerGetFromSlot(i);
      if (!p || !p->PlayerMoby) continue;
      if (gameIsStartMenuOpen(i)) continue;

      struct SurvivalPlayer* playerData = &State.PlayerStates[p->PlayerId];
      float x = 0, y = 0;

      if (playerData->IsInWeaponsMenu) continue;

      // draw dread tokens
      {
        x = 457;
        y = 54;
        transformToSplitscreenPixelCoordinates(i, &x, &y);
        snprintf(buffer, 32, "%d", playerData->State.CurrentTokens);
        gfxScreenSpaceText(x+2, y+8+2, 1, 1, 0x40000000, buffer, -1, 2);
        gfxScreenSpaceText(x,   y+8,   1, 1, 0x80C0C0C0, buffer, -1, 2);
        drawDreadTokenIcon(x+5, y, 32);
      }
      
      // draw blessings
      int j;
      for (j = playerData->State.BlessingSlots-1; j >= 0; --j) {
        int itemTexId = -1;
        int itemTexWH = 32;
        u32 itemColor = 0x80C0C0C0;
        switch (playerData->State.ItemBlessings[j])
        {
          case BLESSING_ITEM_MULTI_JUMP: itemTexId = 111 - 3; itemTexWH = 64; break;
          case BLESSING_ITEM_LUCK: itemTexId = 112 - 3; itemTexWH = 64; break;
          case BLESSING_ITEM_BULL: itemTexId = 113 - 3; itemTexWH = 64; break;
          case BLESSING_ITEM_ELEM_IMMUNITY: itemTexId = 114 - 3; itemTexWH = 64; break;
          case BLESSING_ITEM_HEALTH_REGEN: itemTexId = 115 - 3; itemTexWH = 64; break;
          case BLESSING_ITEM_AMMO_REGEN: itemTexId = 116 - 3; itemTexWH = 64; break;
          case BLESSING_ITEM_THORNS: itemTexId = 117 - 3; itemTexWH = 64; break;
          default: break;
        }

        if (itemTexId > 0) {
          x = 15 + (j*20);
          y = SCREEN_HEIGHT - 100;
          transformToSplitscreenPixelCoordinates(i, &x, &y);

          gfxSetupGifPaging(0);
          u64 itemSprite = gfxGetFrameTex(itemTexId);
          gfxDrawSprite(x+2, y+2, 32, 32, 0, 0, itemTexWH, itemTexWH, 0x40000000, itemSprite);
          gfxDrawSprite(x,   y,   32, 32, 0, 0, itemTexWH, itemTexWH, itemColor, itemSprite);
          gfxDoGifPaging();
        }
      }

      // draw current item
      {
        int itemTexId = -1;
        int itemTexWH = 32;
        u32 itemColor = 0x80C0C0C0;
        char useItemChar = 0;
        switch (playerData->State.Item)
        {
          case MYSTERY_BOX_ITEM_REVIVE_TOTEM: itemTexId = 80 - 3; break;
          case MYSTERY_BOX_ITEM_INVISIBILITY_CLOAK: itemTexId = 140 - 3; useItemChar = '\x1A'; break;
          case MYSTERY_BOX_ITEM_EMP_HEALTH_GUN: itemTexId = 15 - 3; useItemChar = '\x1A'; break;
          default: break;
        }

        if (itemTexId > 0) {
          x = 20;
          y = SCREEN_HEIGHT - 50;
          transformToSplitscreenPixelCoordinates(i, &x, &y);

          gfxSetupGifPaging(0);
          u64 itemSprite = gfxGetFrameTex(itemTexId);
          gfxDrawSprite(x+2, y+2, 32, 32, 0, 0, itemTexWH, itemTexWH, 0x40000000, itemSprite);
          gfxDrawSprite(x,   y,   32, 32, 0, 0, itemTexWH, itemTexWH, itemColor, itemSprite);
          gfxDoGifPaging();
        }

        if (useItemChar) {
          x = 5;
          y = SCREEN_HEIGHT - 40;
          transformToSplitscreenPixelCoordinates(i, &x, &y);
          gfxScreenSpaceText(x, y, 0.75, 0.75, 0x80FFFFFF, &useItemChar, 1, 0);
        }
      }
    }
  }

#if DEBUG
  if (padGetButton(0, PAD_CROSS)) {
    *(float*)0x00347BD8 = 0.125;
  }

  //State.PlayerStates[0].State.Upgrades[UPGRADE_SPEED] = 30;
#endif

#if FIXEDTARGET
  if (FIXEDTARGETMOBY && padGetButton(0, PAD_L1 | PAD_R1 | PAD_L2 | PAD_R2)) {
    vector_copy(FIXEDTARGETMOBY->Position, playerGetFromSlot(0)->PlayerPosition);
    printf("fixed target moved: ");
    vector_print(FIXEDTARGETMOBY->Position);
    printf("\n");
  }
#endif

	if (!State.GameOver)
	{
    // update bolt counter
		for (i = 0; i < GAME_MAX_LOCALS; ++i)
		{
      Player* lp = playerGetFromSlot(i);
      if (!lp) continue;

      BoltCounts[i] = State.PlayerStates[lp->PlayerId].State.Bolts;
		}

		// 
		State.ActivePlayerCount = 0;
		for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
			if (players[i])
				State.ActivePlayerCount++;

			processPlayer(i);
		}
    drawSnack();

		// replace normal scoreboard with bolt counter
		forcePlayerHUD();

		// handle freeze and double bolts
		if (State.IsHost) {
			int dblPointsChanged = 0;
			int dblXPChanged = 0;

			// disable double bolts for players
			for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
				if (State.PlayerStates[i].IsDoublePoints && gameTime >= (State.PlayerStates[i].TimeOfDoublePoints + DOUBLE_POINTS_DURATION)) {
					State.PlayerStates[i].IsDoublePoints = 0;
					dblPointsChanged = 1;
					DPRINTF("setting player %d dbl bolts to 0\n", i);
				}

				if (State.PlayerStates[i].IsDoubleXP && gameTime >= (State.PlayerStates[i].TimeOfDoubleXP + DOUBLE_XP_DURATION)) {
					State.PlayerStates[i].IsDoubleXP = 0;
					dblXPChanged = 1;
					DPRINTF("setting player %d dbl xp to 0\n", i);
				}
			}

			// disable freeze
			if (State.Freeze && gameTime >= (State.TimeOfFreeze + FREEZE_DROP_DURATION)) {
				DPRINTF("disabling freeze\n");
				setFreeze(0);
			}

			if (dblPointsChanged)
				sendDoublePoints();

			if (dblXPChanged)
				sendDoubleXP();
		}

		// handle game over
		if (State.IsHost && gameOptions->GameFlags.MultiplayerGameFlags.Survivor && gameTime > (State.InitializedTime + 5*TIME_SECOND))
		{
			int isAnyPlayerAlive = 0;

			// determine number of players alive
			for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
				if (players[i] && players[i]->SkinMoby && !playerIsDead(players[i]) && players[i]->Health > 0) {
					isAnyPlayerAlive = 1;
				}
			}

			// if everyone has died, end game
			if (!isAnyPlayerAlive)
			{
				State.GameOver = 1;
				gameSetWinner(10, 1);
			}
		}

		if (State.RoundCompleteTime)
		{
			// draw round complete message
			if (gameTime < (State.RoundCompleteTime + ROUND_MESSAGE_DURATION_MS)) {
				snprintf(buffer, sizeof(buffer), SURVIVAL_ROUND_COMPLETE_MESSAGE, State.RoundNumber);
				drawRoundMessage(buffer, 1.5, 0);
			}

			if (State.RoundEndTime > 0)
			{
				// handle when round properly ends
				if (gameTime > State.RoundEndTime)
				{
					// reset round state
					resetRoundState();
				}
				else if (State.IsHost)
				{
					// draw round countdown
          uiShowTimer(0, SURVIVAL_NEXT_ROUND_BEGIN_SKIP_MESSAGE, (int)((State.RoundEndTime - gameTime) * (60.0 / TIME_SECOND)));

					// handle skip
					if (localPlayerHasInput() && padGetButtonDown(0, PAD_UP) > 0) {
						setRoundStart(1);
					}
				}
				else
				{
					// draw round countdown
					uiShowTimer(0, SURVIVAL_NEXT_ROUND_TIMER_MESSAGE, (int)((State.RoundEndTime - gameTime) * (60.0 / TIME_SECOND)));
				}
			}
      else if (State.IsHost && State.RoundEndTime < 0)
      {
        // round doesn't begin until host chooses to start
        gfxScreenSpaceText(SCREEN_WIDTH / 2, SCREEN_HEIGHT - 30, 1, 1, 0x80FFFFFF, SURVIVAL_NEXT_ROUND_BEGIN_SKIP_MESSAGE, -1, 4);
        if (localPlayerHasInput() && padGetButtonDown(0, PAD_UP) > 0) {
          setRoundStart(1);
        }
      }
      else if (!State.IsHost && State.RoundEndTime < 0)
      {
        gfxScreenSpaceText(SCREEN_WIDTH / 2, SCREEN_HEIGHT - 30, 1, 1, 0x80FFFFFF, SURVIVAL_NEXT_ROUND_WAIT_FOR_HOST_MESSAGE, -1, 4);
      }
			else if (State.IsHost)
			{
#if AUTOSTART
        setRoundStart(0);
#else
        gfxScreenSpaceText(SCREEN_WIDTH / 2, SCREEN_HEIGHT - 30, 1, 1, 0x80FFFFFF, SURVIVAL_NEXT_ROUND_BEGIN_SKIP_MESSAGE, -1, 4);
        if (localPlayerHasInput() && padGetButtonDown(0, PAD_UP) > 0) {
          setRoundStart(1);
        }
#endif
			}
		}
		else
		{
			// draw round start message
			if (gameTime < (State.RoundStartTime + ROUND_MESSAGE_DURATION_MS)) {
				snprintf(buffer, sizeof(buffer), SURVIVAL_ROUND_START_MESSAGE, State.RoundNumber+1);
				drawRoundMessage(buffer, 1.5, 0);
				if (State.RoundIsSpecial) {
					drawRoundMessage(mapConfig->SpecialRoundParams[State.RoundSpecialIdx].Name, 1, 35);
				}
			}

#if !defined(DISABLE_SPAWNING)
			// host specific logic
			if (State.IsHost && (gameTime - State.RoundStartTime) > ROUND_START_DELAY_MS)
			{
				// decrement mob cooldowns
				for (i = 0; i < mapConfig->DefaultSpawnParamsCount; ++i) {
					if (defaultSpawnParamsCooldowns[i]) {
						defaultSpawnParamsCooldowns[i] -= 1;
					}
				}

				// reduce count per player to reduce lag
				int maxSpawn = State.RoundMaxSpawnedAtOnce;

				// handle spawning
				if (State.RoundSpawnTicker == 0) {
					if (State.MobStats.TotalAlive < maxSpawn && !State.Freeze) {
						if (State.MobStats.TotalSpawnedThisRound < State.RoundMaxMobCount) {
							if (spawnRandomMob()) {
#if QUICK_SPAWN
								State.RoundSpawnTicker = 10;
#else
								++State.RoundSpawnTickerCounter;
								if (State.RoundSpawnTickerCounter > State.RoundNextSpawnTickerCounter)
								{
									State.RoundSpawnTickerCounter = 0;
									State.RoundNextSpawnTickerCounter = randRangeInt(MOB_SPAWN_BURST_MIN + (State.RoundNumber * MOB_SPAWN_BURST_MIN_INC_PER_ROUND), MOB_SPAWN_BURST_MAX + (State.RoundNumber * MOB_SPAWN_BURST_MAX_INC_PER_ROUND));
									

                  // ticks to delay between spawning bursts
                  float maxBurstDelay = lerpf(MOB_SPAWN_BURST_MAX_DELAY, MOB_SPAWN_BURST_MIN_DELAY, clamp((State.RoundNumber / 20.0), 0, 1));
                  maxBurstDelay = lerpf(MOB_SPAWN_BURST_MAX_DELAY, 5, demonBellFactor);
                  State.RoundSpawnTicker = randRangeInt(minf(maxBurstDelay, MOB_SPAWN_BURST_MIN_DELAY), maxBurstDelay);

                  //DPRINTF("MIN:%f MAX:%f BELL:%f TICKER:%f\n", minf(maxBurstDelay, MOB_SPAWN_BURST_MIN_DELAY)/60.0, maxBurstDelay/60.0, demonBellFactor, State.RoundSpawnTicker/60.0);
								}
								else
								{
                  // ticks to delay between spawning mobs within a burst
                  State.RoundSpawnTicker = 1 + lerpf(60.0 / (State.RoundNumber+1), 0, demonBellFactor);
								}
#endif
							}
						} else {

							DPRINTF("finished spawning zombies...\n");
							State.RoundSpawnTicker = -1;
						}
					}
				} else if (State.RoundSpawnTicker < 0) {
					if (State.MobStats.TotalAlive < 0) {
						DPRINTF("%d\n", State.MobStats.TotalAlive);
					}
					// wait for all zombies to die
					if ((State.MobStats.TotalAlive + State.MobStats.TotalSpawning) == 0) {
						setRoundComplete();
					}
				} else {
					--State.RoundSpawnTicker;
				}
			}
#endif
		}
	}
	else
	{
		// end game
		if (State.GameOver == 1)
		{
			gameEnd(4);
			State.GameOver = 2;
		}
	}

  //
  updateDzoHud();

	// last
	dlPostUpdate();
	return;
}

//--------------------------------------------------------------------------
void setLobbyGameOptions(PatchGameConfig_t * gameConfig)
{
	// set game options
	GameOptions * gameOptions = gameGetOptions();
	GameSettings* gameSettings = gameGetSettings();
	if (!gameOptions || gameSettings->GameLoadStartTime <= 0)
		return;

	// apply options
	gameOptions->GameFlags.MultiplayerGameFlags.Juggernaut = 0;
	gameOptions->GameFlags.MultiplayerGameFlags.SpawnWithChargeboots = 1;
	gameOptions->GameFlags.MultiplayerGameFlags.SpecialPickups = 1;
	gameOptions->GameFlags.MultiplayerGameFlags.SpecialPickupsRandom = 0;
	gameOptions->GameFlags.MultiplayerGameFlags.Timelimit = 0;
	gameOptions->GameFlags.MultiplayerGameFlags.KillsToWin = 0;
	gameOptions->GameFlags.MultiplayerGameFlags.RespawnTime = 0;
	gameOptions->GameFlags.MultiplayerGameFlags.Teamplay = 1;

#if !DEBUG
	gameOptions->GameFlags.MultiplayerGameFlags.UnlimitedAmmo = 0;
	gameOptions->GameFlags.MultiplayerGameFlags.Survivor = 1;
	gameOptions->GameFlags.MultiplayerGameFlags.AutospawnWeapons = 0;
#endif

	// no vehicles
	gameOptions->GameFlags.MultiplayerGameFlags.Vehicles = 0;
	gameOptions->GameFlags.MultiplayerGameFlags.Puma = 0;
	gameOptions->GameFlags.MultiplayerGameFlags.Hoverbike = 0;
	gameOptions->GameFlags.MultiplayerGameFlags.Landstalker = 0;
	gameOptions->GameFlags.MultiplayerGameFlags.Hovership = 0;

	// enable all weapons
	gameOptions->WeaponFlags.Chargeboots = 1;
	gameOptions->WeaponFlags.DualVipers = 1;
	gameOptions->WeaponFlags.MagmaCannon = 1;
	gameOptions->WeaponFlags.Arbiter = 1;
	gameOptions->WeaponFlags.FusionRifle = 1;
	gameOptions->WeaponFlags.MineLauncher = 1;
	gameOptions->WeaponFlags.B6 = 1;
	gameOptions->WeaponFlags.Holoshield = 1;
	gameOptions->WeaponFlags.Flail = 1;

  // disable custom game rules
  if (gameConfig) {
    gameConfig->grNoHealthBoxes = 1;
    gameConfig->grNoInvTimer = 0;
    gameConfig->grNoPickups = 0;
    gameConfig->grV2s = 0;
    gameConfig->grVampire = 0;
    gameConfig->grHealthBars = 1;
    gameConfig->prChargebootForever = 0;
    gameConfig->prHeadbutt = 0;
    gameConfig->prPlayerSize = 0;
    gameConfig->grCqPersistentCapture = 0;
    gameConfig->grCqDisableTurrets = 0;
    gameConfig->grCqDisableUpgrades = 0;
  }

	// force everyone to same team as host
	//for (i = 1; i < GAME_MAX_PLAYERS; ++i) {
	//	if (gameSettings->PlayerTeams[i] >= 0) {
	//		gameSettings->PlayerTeams[i] = gameSettings->PlayerTeams[0];
	//	}
	//}
}

//--------------------------------------------------------------------------
void setEndGameScoreboard(PatchGameConfig_t * gameConfig)
{
	u32 * uiElements = (u32*)(*(u32*)(0x011C7064 + 4*18) + 0xB0);
	int i;

	// column headers start at 17
	strncpy((char*)(uiElements[19] + 0x60), "BOLTS", 6);
	strncpy((char*)(uiElements[20] + 0x60), "DEATHS", 7);
	strncpy((char*)(uiElements[21] + 0x60), "REVIVES", 8);

	// rows
	int* pids = (int*)(uiElements[0] - 0x9C);
	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
	{
		// match scoreboard player row to their respective dme id
		int pid = pids[i];
		char* name = (char*)(uiElements[7 + i] + 0x18);
		if (pid < 0 || name[0] == 0)
			continue;

		struct SurvivalPlayerState* pState = &State.PlayerStates[pid].State;

		// set round number
		sprintf((char*)0x003D3AE0, "Survived %d Rounds!", State.RoundNumber);

		// set kills
		sprintf((char*)(uiElements[22 + (i*4) + 0] + 0x60), "%d", pState->Kills);

		// copy over deaths
		strncpy((char*)(uiElements[22 + (i*4) + 2] + 0x60), (char*)(uiElements[22 + (i*4) + 1] + 0x60), 10);

		// set bolts
		sprintf((char*)(uiElements[22 + (i*4) + 1] + 0x60), "%ld", pState->TotalBolts);

		// set revives
		sprintf((char*)(uiElements[22 + (i*4) + 3] + 0x60), "%d", pState->Revives);
	}
}

//--------------------------------------------------------------------------
void lobbyStart(struct GameModule * module, PatchStateContainer_t * gameState)
{
	int i;
	int activeId = uiGetActive();
	static int initializedScoreboard = 0;

	// send final local player stats to others
	if (Initialized == 1) {
		for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
			if (State.PlayerStates[i].IsLocal) {
				sendPlayerStats(i);
			}
		}
    
    bubbleDeinit();

		Initialized = 2;
	}

  // disable ranking
  gameSetIsGameRanked(0);

	// 
	updateGameState(gameState);

	// scoreboard
	switch (activeId)
	{
		case 0x15C:
		{
			if (initializedScoreboard)
				break;

			setEndGameScoreboard(gameState->GameConfig);
			initializedScoreboard = 1;

			// patch rank computation to keep rank unchanged for base mode
			POKE_U32(0x0077ACE4, 0x4600BB06);
			break;
		}
		case UI_ID_GAME_LOBBY:
		{
			setLobbyGameOptions(gameState->GameConfig);
			break;
		}
	}
}

//--------------------------------------------------------------------------
void loadStart(struct GameModule * module, PatchStateContainer_t * gameState)
{
  // reset initialized on load
  // enables level hopping
  Initialized = 0;

	setLobbyGameOptions(gameState->GameConfig);
  
	// point get resurrect point to ours
	*(u32*)0x00610724 = 0x0C000000 | ((u32)&getResurrectPoint >> 2);
	*(u32*)0x005e2d44 = 0x0C000000 | ((u32)&getResurrectPoint >> 2);
}

//--------------------------------------------------------------------------
void start(struct GameModule * module, PatchStateContainer_t * gameState, enum GameModuleContext context)
{
  switch (context)
  {
    case GAMEMODULE_LOBBY: lobbyStart(module, gameState); break;
    case GAMEMODULE_LOAD: loadStart(module, gameState); break;
    case GAMEMODULE_GAME_FRAME: gameStart(module, gameState); break;
    case GAMEMODULE_GAME_UPDATE: break;
  }
}
