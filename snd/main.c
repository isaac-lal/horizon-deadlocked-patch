/***************************************************
 * FILENAME :		main.c
 * 
 * DESCRIPTION :
 * 		Handles all search and destroy logic.
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
#include <libdl/graphics.h>
#include <libdl/player.h>
#include <libdl/weapon.h>
#include <libdl/hud.h>
#include <libdl/cheats.h>
#include <libdl/sha1.h>
#include <libdl/dialog.h>
#include <libdl/team.h>
#include <libdl/stdio.h>
#include <libdl/ui.h>
#include <libdl/guber.h>
#include <libdl/color.h>
#include <libdl/radar.h>
#include <libdl/sound.h>
#include <libdl/utils.h>
#include <libdl/net.h>
#include "module.h"
#include "common.h"
#include "messageid.h"

#include "include/pvars.h"

// TODO
// Create scoreboard abstraction in libdl

/*
 * When non-zero, it refreshes the in-game scoreboard.
 */
#define GAME_SCOREBOARD_REFRESH_FLAG        (*(u32*)0x002F9FC8)

/*
 * Target scoreboard value.
 */
#define GAME_SCOREBOARD_TARGET              (*(u32*)0x002FA084)

/*
 * Target scoreboard value.
 */
#define GAME_SCOREBOARD_NODE_TARGET         (*(u32*)0x00310180)

/*
 * Collection of scoreboard items.
 */
#define GAME_SCOREBOARD_ARRAY               ((ScoreboardItem**)0x002FA04C)

/*
 * Number of items in the scoreboard.
 */
#define GAME_SCOREBOARD_ITEM_COUNT          (*(u32*)0x002F9FCC)

/*
 * Max number of rounds before game ends
 */
#define SND_MAX_ROUNDS						(2 * (MapConfig.RoundsToWin-1) + 1)

/*
 *
 */
#define SND_BOMB_TIMER_TEXT_SCALE			(3)

/*
 *
 */
#define SND_TEAM_DEFENDER_ID				(0)
#define SND_TEAM_ATTACKER_ID				(1)

/*
 *
 */
#define SND_BOMB_TIMER_BASE_COLOR1			(0xFFB700B7)
#define SND_BOMB_TIMER_BASE_COLOR2			(0xFF670067)
#define SND_BOMB_TIMER_HIGH_COLOR				(0xFFFFFFFF)

/*
 * Amount of time after a round ends before switching to next round.
 */
#define SND_ROUND_TRANSITION_WAIT_MS		(5 * TIME_SECOND)

/*
 *
 */
#define SND_NODE1_BLIP_TEAM							(2)
#define SND_NODE2_BLIP_TEAM							(5)

/*
 * Popup strings
 */
const char * SND_BOMB_YOU_PICKED_UP = "You picked up the bomb!";
const char * SND_BOMB_PICKED_UP = "The bomb has been picked up!";
const char * SND_BOMB_PLANTED = "The bomb has been planted!";
const char * SND_BOMB_DEFUSED = "Bomb defused!";
const char * SND_BOMB_DROPPED = "The bomb has been dropped!";
const char * SND_ROUND_WIN = "Round win!";
const char * SND_ROUND_LOSS = "Round loss!";
const char * SND_HALF_TIME = "Switching sides...";
const char * SND_DEFEND_HELLO = "Defend your bombsites!";
const char * SND_ATTACK_HELLO = "Destroy the enemy bombsite!";

/*
 *
 */
typedef struct SNDNodeState
{
	Moby * Moby;
	GuberMoby * GuberMoby;
	GuberMoby * OrbGuberMoby;
} SNDNodeState_t;

/*
 *
 */
typedef struct SNDPlayerState
{
	int PlayerIndex;
	int IsBombCarrier;
	short BombsPlanted;
	short BombsDefused;
	short BombsNinjaDefused;
} SNDPlayerState_t;

/*
 *
 */
typedef struct SNDTimerState
{
	int LastPlaySoundSecond;
	u32 Color;
} SNDTimerState_t;

/*
 *
 */
typedef struct SNDOutcomeMessage
{
	int Outcome;
	int GameTime;
} SNDOutcomeMessage_t;

/*
 *
 */
typedef struct SNDBombOutcomeMessage
{
	int NodeIndex;
	int Team;
	int PlayerId;
	int GameTime;
} SNDBombOutcomeMessage_t;


/*
 *
 */
typedef struct GameplayMobyDef
{
	u32 Size;
	char Mission;
	int UID;
	int Bolts;
	int OClass;
	float Scale;
	float DrawDistance;
	float UpdateDistance;
	short UNK_20;
	short UNK_22;
	short UNK_24;
	short UNK_26;
	float PosX;
	float PosY;
	float PosZ;
	float RotX;
	float RotY;
	float RotZ;
	int Group;
	int IsRooted;
	float RootedDistance;
	short UNK_4C;
	short UNK_4E;
	int PVarIndex;
	int Occlusion;
	int UNK_58;
	int Red;
	int Green;
	int Blue;
	int Light;
	int UNK_6C;
} GameplayMobyDef_t;

/*
 *
 */
enum SNDOutcome
{
	SND_OUTCOME_INCOMPLETE = 0,
	SND_OUTCOME_TIME_END,
	SND_OUTCOME_BOMB_DETONATED,
	SND_OUTCOME_BOMB_DEFUSED,
	SND_OUTCOME_ATTACKERS_DEAD
};

/*
 *
 */
enum GameNetMessage
{
	CUSTOM_MSG_SET_ROUND_OUTCOME = CUSTOM_MSG_ID_GAME_MODE_START,
	CUSTOM_MSG_SET_BOMB_OUTCOME,
};

struct SNDMapConfig
{
  VECTOR DefendTeamSpawnPoint;
  VECTOR AttackTeamSpawnPoint;
  VECTOR Node1SpawnPoint;
  VECTOR Node2SpawnPoint;
  VECTOR PackSpawnPoint;
  int BombDetonationTimer;
  int RoundsToWin;
  int RoundsToFlip;
  int RoundTimelimitSeconds;
};

/*
 *
 */
struct SNDState
{
	int RoundNumber;
	int RoundStartTicks;
	int RoundEndTicks;
	int RoundResult;
	int RoundInitialized;
	int RoundLastWinners;
	int TeamWins[2];
	int TeamRolesFlipped;
	int GameOver;
	int WinningTeam;
	int BombPlantSiteIndex;
	int BombPlantedTicks;
	int BombDefused;
	int DefenderTeamId;
	int AttackerTeamId;
	int IsHost;
	int NodeCount;
	char RoundWinner[32];
	GuberMoby * BombPackGuber;
	Moby * BombPackMoby;
	Moby * RadarObjectiveMoby[2];
	Player * BombCarrier;

	VECTOR SpawnPackAt;
	
	SNDNodeState_t Nodes[2];
	SNDPlayerState_t Players[GAME_MAX_PLAYERS];
	SNDTimerState_t Timer;
} SNDState;

struct CustomDzoCommandSndDrawTimer
{
  u32 MsLeft;
  u32 Color;
  float Size;
};

struct CustomDzoCommandSndDrawRoundResult
{
  char Message[64];
};

/*
 *
 */
struct SNDGameData
{
	int Version;
	char RoundWinner[32];
	short BombsPlanted[GAME_MAX_PLAYERS];
	short BombsDefused[GAME_MAX_PLAYERS];
	short BombsNinjaDefused[GAME_MAX_PLAYERS];
};

/*
 *
 */
ScoreboardItem TeamScores[2];

/*
 *
 */
int ScoreboardChanged = 0;

/*
 *
 */
int Initialized = 0;
int InitializedTime = 0;

/*
 *
 */
char shaBuffer = 0;

/*
 * Needed when moving hacker orbs
 */
void * HackerOrbCollisionPointer = 0;

/*
 * Needed when moving hacker orbs
 */
void * NodeBaseCollisionPointer = 0;

/*
 * Configurable settings
 */
struct SNDMapConfig MapConfig __attribute__((section(".config"))) = {
  .DefendTeamSpawnPoint = { 268.386, 122.752, 103.479, 0.8 },
  .AttackTeamSpawnPoint = { 519.269, 396.575, 106.727, -1.351 },
  .Node1SpawnPoint = { 428.368, 239.646, 106.613, 0 },
  .Node2SpawnPoint = { 411.456, 143.924, 105.344, 0 },
  .PackSpawnPoint = { 526.056, 370.259, 107.271, 0 },
  .BombDetonationTimer = 30,
  .RoundsToWin = 6,
  .RoundsToFlip = 3,
  .RoundTimelimitSeconds = 2 * 60,
};

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


// forwards
void onSetRoundOutcome(int outcome, int gameTime);
int onSetRoundOutcomeRemote(void * connection, void * data);
void setRoundOutcome(int outcome);

// forwards
void onSetBombOutcome(int nodeIndex, int team, int playerId, int gameTime);
int onSetBombOutcomeRemote(void * connection, void * data);
void setBombOutcome(int nodeIndex, int team, int playerId);

/*
 * NAME :		updateScoreboard
 * 
 * DESCRIPTION :
 * 			Updates the in game scoreboard.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void updateScoreboard(void)
{
	int i;
	Player * localPlayer = (Player*)0x00347AA0;

	// Update values
	TeamScores[0].Value = SNDState.TeamWins[0];
	TeamScores[1].Value = SNDState.TeamWins[1];

	// Force scoreboard to custom scoreboard values
	if (GAME_SCOREBOARD_ARRAY[0] != &TeamScores[localPlayer->Team])
	{
		GAME_SCOREBOARD_ARRAY[0] = &TeamScores[localPlayer->Team];
		ScoreboardChanged = 1;
	}

	// Force scoreboard to custom scoreboard values
	if (GAME_SCOREBOARD_ARRAY[1] != &TeamScores[!localPlayer->Team])
	{
		GAME_SCOREBOARD_ARRAY[1] = &TeamScores[!localPlayer->Team];
		ScoreboardChanged = 1;
	}

	// Set hud flags
	if (gameGetTime() > (InitializedTime + 50))
	{
		for (i = 0; i < GAME_MAX_LOCALS; ++i)
		{
			PlayerHUDFlags * hud = hudGetPlayerFlags(i);
			if (!hud->Flags.NormalScoreboard)
			{
				hud->Flags.ConquestScoreboard = 0;
				hud->Flags.NormalScoreboard = 1;
				hud->UNK_00[1] = 1; // refresh
			}
		}
	}
}

void hideMoby(Moby * moby, int move)
{
	static VECTOR add = {-1000,-1000,-1000,0};
	if (!moby)
		return;

	moby->CollData = NULL;
	moby->Opacity = 0;
	if (move)
		vector_add(moby->Position, moby->Position, add);
	//moby->RenderDistance = 0;
}

void hideNode(Moby * nodeBaseMoby, int keepNode, int keepOrb, int move)
{
	if (!nodeBaseMoby)
		return;
		
	int i = 2;
	NodeBasePVar_t * nodePvars = (NodeBasePVar_t*)nodeBaseMoby->PVar;
	Moby * orb = nodePvars->HackerOrbMoby;
	Moby ** subItems = (Moby**)nodePvars->ChildMobies;

	// hide base and orb
	if (!keepNode)
	{
		if (nodeBaseMoby->CollData && !NodeBaseCollisionPointer)
			NodeBaseCollisionPointer = nodeBaseMoby->CollData;
		hideMoby(nodeBaseMoby, move);
		i = 0;
	}

	if (!keepOrb)
	{
		if (orb->CollData && !HackerOrbCollisionPointer)
			HackerOrbCollisionPointer = orb->CollData;
		hideMoby(orb, move);
	}

	// hide subitems (turrets)
	for (; i < 4; ++i)
		hideMoby(subItems[i], 1);
}

void showNode(Moby * nodeBaseMoby, VECTOR position)
{
	int i = 1;
	NodeBasePVar_t * nodePvars = (NodeBasePVar_t*)nodeBaseMoby->PVar;
	Moby * orb = nodePvars->HackerOrbMoby;
	Moby ** subItems = (Moby**)nodePvars->ChildMobies;

	vector_copy(nodeBaseMoby->Position, position);
	nodeBaseMoby->OcclIndex |= 1;
	nodeBaseMoby->ModeBits &= ~4;
	nodeBaseMoby->DrawDist = 0xFF;
	nodeBaseMoby->Opacity = 0x80;
	if (NodeBaseCollisionPointer)
		nodeBaseMoby->CollData = NodeBaseCollisionPointer;
	
	if (orb)
	{
		vector_copy(orb->Position, position);
		orb->DrawDist = 0xFF;
		orb->Opacity = 0x80;
		if (HackerOrbCollisionPointer)
			orb->CollData = HackerOrbCollisionPointer;
	}

	for (i = 0; i < 2; ++i)
		if (subItems[i])
			vector_copy(subItems[i]->Position, position);
}

void nodeCapture(GuberMoby * guberMoby, int team)
{
	u32 buffer[] = { team, -1 };
	Guber* guber = &guberMoby->Guber;
	
	if (guber)
	{
		GuberEvent * event = guberEventCreateEvent(guber, 2, 0, 0);
		if (event)
		{
			guberEventWrite(event, buffer + 0, 4);
			guberEventWrite(event, buffer + 1, 4);
		}
	}
}

void spawnPlayer(Player * player, VECTOR position)
{
	VECTOR pos, r = {0,0,0,0};
	float theta = (player->PlayerId / (float)GAME_MAX_PLAYERS) * MATH_TAU;

	vector_copy(pos, position);
	pos[0] += cosf(theta) * 2.5;
	pos[1] += sinf(theta) * 2.5;
	r[2] = position[3];

	player->Explode = 0;
	player->Invisible = 0;
	player->SkinMoby->DrawDist = 255;
	playerRespawn(player);
	playerSetPosRot(player, pos, r);
	DPRINTF("player %d spawn with %d %d\n", player->PlayerId, player->SkinMoby->DrawDist, player->SkinMoby->UpdateDist);
}

void replaceString(int textId, const char * str)
{
	// Get pointer to game string
	char * strPtr = uiMsgString(textId);
	strncpy(strPtr, str, 32);
}

inline void disableBlipPulsing(void)
{
	*(u32*)0x005564A0 = 0x10000071;
}

inline void enableBlipPulsing(void)
{
	*(u32*)0x005564A0 = 0x10620071;
}

void SNDHackerOrbEventHandler(Moby * moby, GuberEvent * event, MobyEventHandler_func eventHandler)
{
	int nodeIndex = -1;
	HackerOrbPVar_t * orbPvars = (HackerOrbPVar_t*)moby->PVar;
	
	/*
	DPRINTF("Hacker Orb Event: %08x\n\t", (u32)moby);
	u32 * buffer = (u32*)event->NetEvent;
	int i;
	for (i = 0; i < sizeof(event->NetEvent)/4; ++i)
		DPRINTF("%08X ", buffer[i]);
	DPRINTF("\n");

	DPRINTF("\n\tNetSendTime: %d\n\tNetSendTo: %d\n\tNetDataOffset: %02x\n\tMsgSendPending: %d\n\tNextEvent: %08x\n",
		event->NetSendTime,
		event->NetSendTo,
		event->NetDataOffset,
		event->MsgSendPending,
		(u32)event->NetEvent
		);
	*/

	// 
	u32 nodeUid = orbPvars->NodeBaseUid;
	if (SNDState.Nodes[0].GuberMoby->Guber.Id.UID == nodeUid)
	{
		nodeIndex = 0;
		if (!SNDState.Nodes[0].OrbGuberMoby)
			nodeCapture(moby->GuberMoby, SND_TEAM_DEFENDER_ID);
		SNDState.Nodes[0].OrbGuberMoby = moby->GuberMoby;
	}
	else if (SNDState.Nodes[1].GuberMoby->Guber.Id.UID == nodeUid)
	{
		nodeIndex = 1;
		if (!SNDState.Nodes[1].OrbGuberMoby)
			nodeCapture(moby->GuberMoby, SND_TEAM_DEFENDER_ID);
		SNDState.Nodes[1].OrbGuberMoby = moby->GuberMoby;
	}

	// get id of event
	u32 eventId = event->NetEvent.EventID;

	// if round is already over then ignore event
	if (SNDState.RoundResult)
		return;

	// handle event
	switch (eventId)
	{
		case 2: // capture
		{
			// get capture team
			int team = *(int*)((u32)event + 12);
			int playerId = *(int*)((u32)event + 16);

			// Only capture if bomb is picked up
			if (SNDState.RoundInitialized && SNDState.IsHost && playerId >= 0 && !SNDState.BombPackMoby && nodeIndex >= 0)
			{
				if (team == SNDState.AttackerTeamId)
				{
					// send to others
					setBombOutcome(nodeIndex, team, playerId);
				}
				else
				{
					// send to others
					setBombOutcome(nodeIndex, team, playerId);

					// set state
					setRoundOutcome(SND_OUTCOME_BOMB_DEFUSED);
				}
			}
			break;
		}
		case 1: // update
		{
			eventHandler(moby, event);

			if (nodeIndex >= 0)
			{
				int nodeTeam = orbPvars->NodeTeam;
				if (nodeTeam != SNDState.DefenderTeamId && nodeTeam != SNDState.AttackerTeamId)
					nodeCapture(moby->GuberMoby, SNDState.DefenderTeamId);
			}

			return;
		}
	}

	eventHandler(moby, event);
}

void SNDWeaponPackEventHandler(Moby * moby, GuberEvent * event, MobyEventHandler_func eventHandler)
{
	Player ** players = playerGetAll();
	Player * p;
	Player * localPlayer = (Player*)0x347AA0;
	int i,j;

	int eventId = event->NetEvent.EventID;
	int isNew = event->NetEvent.ObjUID == *(u32*)(*(u32*)0x00220710);

	// get id of event
	// 0 is pickup
	if (eventId == 0 && !isNew)
	{
		// get id of player picking up pack
		int hostId = event->NetEvent.NetData[3] >> 4;
		DPRINTF("weapon pack event %d\n", hostId);

		// find player with hostId
		for (i = 0; i < GAME_MAX_PLAYERS; ++i)
		{
			p = *players;
			if (p && p->Guber.Id.GID.HostId == hostId)
			{
				DPRINTF("wp player %d\n", p->Guber.Id.GID.HostId);
				// only allow attacking team to pickup
				if (p->Team != SNDState.AttackerTeamId)
					return;
				
				// 
				if (p != localPlayer && localPlayer->Team == SNDState.AttackerTeamId)
					uiShowPopup(localPlayer->LocalPlayerIndex, SND_BOMB_PICKED_UP);

				// remove reference
				SNDState.BombPackMoby = 0;
				SNDState.BombPackGuber = 0;

				// set bomb carrier
				SNDState.BombCarrier = p;
				SNDState.Players[i].IsBombCarrier = 1;
				break;
			}
			++players;
		}
	}

	eventHandler(moby, event);
}

void GuberMobyEventHandler(Moby * moby, GuberEvent * event, MobyEventHandler_func eventHandler)
{
	switch (moby->OClass)
	{
		case MOBY_ID_CONQUEST_HACKER_ORB: SNDHackerOrbEventHandler(moby, event, eventHandler); break;
		case MOBY_ID_WEAPON_PACK: SNDWeaponPackEventHandler(moby, event, eventHandler); break;
		default:
			DPRINTF("GuberMoby event (%04x) with %08x and %08x, handler=%08x\n", moby->OClass, (u32)moby, (u32)event, (u32)eventHandler);
			eventHandler(moby, event);
			break;
	}
}

Moby * spawnExplosion(VECTOR position, float size)
{
	// SpawnMoby_5025
	u128 param_1 = *(u128*)position;
	Moby * moby = ((Moby* (*)(u128, float, int, int, int, int, int, short, short, short, short, short, short,
				short, short, float, float, float, int, Moby *, int, int, int, int, int, int, int, int,
				int, short, Moby *, Moby *, u128)) (0x003c3b38))
				(param_1, size, 0x2, 0x14, 0x10, 0x10, 0x10, 0x10, 0x2, 0, 1, 0, 0,
				0, 0, 0, 0, 2, 0x00080800, 0, 0x00388EF7, 0x000063F7, 0x00407FFFF, 0x000020FF, 0x00008FFF, 0x003064FF, 0x7F60A0FF, 0x280000FF,
				0x003064FF, 0, 0, 0, 0);
				
	soundPlay(&ExplosionSoundDef, 0, moby, 0, 0x400);

	return moby;
}

void setPackLifetime(int lifetime)
{
	// Set lifetime of bomb pack moby
	if (SNDState.BombPackMoby)
	{
		if (SNDState.BombPackMoby->OClass == MOBY_ID_WEAPON_PACK && SNDState.BombPackMoby->PVar)
			*(u32*)((u32)SNDState.BombPackMoby->PVar + 0x8) = lifetime;
	}
}

void killPack()
{
  // destroy all packs
  Moby* m = mobyListGetStart();
  while (m = mobyFindNextByOClass(m, MOBY_ID_WEAPON_PACK))
  {
    m->State = 5; // kill
		DPRINTF("KILLED PACK AT %08X\n", (u32)m);
    ++m;
  }

  SNDState.BombPackMoby = NULL;
  SNDState.BombPackGuber = NULL;
}

void * spawnPackHook(u16 OClass, int pvarSize, int guberId, int arg4, int arg5)
{
	void * result = ((void* (*)(u16, int, int, int, int))0x0061C3A8)(OClass, pvarSize, guberId, arg4, arg5);

	if (OClass == MOBY_ID_WEAPON_PACK)
	{
		Moby * newMoby = (Moby*)(*(u32*)((u32)result + 0x18));

		// only bomb pack can spawn
		if (SNDState.BombPackMoby && SNDState.BombPackMoby != newMoby) {
			DPRINTF("kill pack not bomb pack moby\n");
      killPack();
    }

		SNDState.BombPackMoby = newMoby;
		SNDState.BombPackMoby->ModeBits2 = (SNDState.BombPackMoby->ModeBits2 & 0xff) | ((0x80 + (8 * SNDState.AttackerTeamId)) << 8);

		DPRINTF("spawnPackHook bomb pack moby = %08x\n", (u32)SNDState.BombPackMoby);
	}

	return result;
}

GuberMoby * spawnPackGuber(VECTOR position, u32 mask)
{
	GuberEvent * guberEvent = 0;
	Player * localPlayer = (Player*)0x00347AA0;
	VECTOR unk = {0,0,-1,0};
	int zero = 0;

	// create guber object
	GuberMoby * guberMoby = guberMobyCreateSpawned(MOBY_ID_WEAPON_PACK, 0x40, &guberEvent, &localPlayer->Guber);
	if (guberEvent)
	{
		guberEventWrite(guberEvent, position, 0x0C);
		guberEventWrite(guberEvent, unk, 0x0C);
		guberEventWrite(guberEvent, &mask, 4);
		guberEventWrite(guberEvent, &zero, 4);
	}
	else
	{
		DPRINTF("failed to guberevent pack\n");
	}

	return guberMoby;
}

void drawRoundMessage(const char * message, float scale)
{
	u32 boxColor = 0x20ffffff;
	int fw = gfxGetFontWidth(message, -1, scale);
	float x = 0.5;
	float y = 0.16;
	float p = 0.02;
	float w = (fw / (float)SCREEN_WIDTH) + p;
	float h = (36.0 / SCREEN_HEIGHT) + p;

	// draw container
	gfxScreenSpaceBox(x-(w/2), y-(h/2), w, h, boxColor);

	// draw message
	gfxScreenSpaceText(SCREEN_WIDTH * x, SCREEN_HEIGHT * y, scale, scale * 1.5, 0x80FFFFFF, message, -1, 4);

  // pass to dzo
  if (PATCH_DZO_INTEROP_FUNCS)
  {
    struct CustomDzoCommandSndDrawRoundResult cmd;
    strncpy(cmd.Message, message, sizeof(cmd.Message));
    PATCH_DZO_INTEROP_FUNCS->SendCustomCommandToClient(CUSTOM_DZO_CMD_ID_SND_DRAW_ROUND_RESULT, sizeof(cmd), &cmd);
  }
}


void onSetRoundOutcome(int outcome, int gameTime)
{
	Player** players = playerGetAll();
	int i = 0;

	if (outcome == SND_OUTCOME_BOMB_DETONATED && SNDState.BombPlantSiteIndex >= 0)
	{
		// get plantsite
		SNDNodeState_t * plantSiteNodeState = &SNDState.Nodes[SNDState.BombPlantSiteIndex];

		// detonate
		for (i = 0; i < 5; ++i)
			spawnExplosion(plantSiteNodeState->OrbGuberMoby->Moby->Position, 5);

		// blow up node
		hideNode(plantSiteNodeState->Moby, 1, 0, 0);

		// blow up defenders
		for (i = 0; i < GAME_MAX_PLAYERS; ++i)
		{
			SNDPlayerState_t * player = &SNDState.Players[i];
			if (players[i])
			{
				if (players[i]->Team == SNDState.DefenderTeamId)
				{
					players[i]->timers.explodeTimer = 1;
				}
			}
		}
	}
	else if (outcome == SND_OUTCOME_BOMB_DEFUSED)
	{
		SNDState.BombDefused = 1;
	}

	// 
	SNDState.RoundResult = outcome;
	SNDState.RoundEndTicks = gameTime + SND_ROUND_TRANSITION_WAIT_MS;

	// print halftime message
	if ((SNDState.RoundNumber+1) % MapConfig.RoundsToFlip == 0)
	{
		uiShowPopup(0, SND_HALF_TIME);
		uiShowPopup(1, SND_HALF_TIME);
	}

	DPRINTF("outcome set to %d\n", outcome);
}

int onSetRoundOutcomeRemote(void * connection, void * data)
{
	SNDOutcomeMessage_t * message = (SNDOutcomeMessage_t*)data;
	onSetRoundOutcome(message->Outcome, message->GameTime);

	return sizeof(SNDOutcomeMessage_t);
}

void setRoundOutcome(int outcome)
{
	SNDOutcomeMessage_t message;

	// don't allow overwriting existing outcome
	if (SNDState.RoundResult)
		return;

	// don't allow changing outcome when not host
	if (!SNDState.IsHost)
		return;

	// send out
	message.Outcome = outcome;
	message.GameTime = gameGetTime();
	netBroadcastCustomAppMessage(NET_DELIVERY_CRITICAL, netGetDmeServerConnection(), CUSTOM_MSG_SET_ROUND_OUTCOME, sizeof(SNDOutcomeMessage_t), &message);

	// set locally
	onSetRoundOutcome(outcome, message.GameTime);
}


void onSetBombOutcome(int nodeIndex, int team, int playerId, int gameTime)
{
	int i;
	Player** players = playerGetAll();

	// capture node
	nodeCapture(SNDState.Nodes[nodeIndex].OrbGuberMoby, team);

	if (team == SNDState.AttackerTeamId)
	{
		// bomb has been planted
		uiShowPopup(0, SND_BOMB_PLANTED);
		uiShowPopup(1, SND_BOMB_PLANTED);

		// change capture time to default (defuse time)
		*(u16*)0x00440E68 = 0x3C23;
		*(u8*)0x0044111E = 2;

		// set state
		SNDState.BombPlantedTicks = gameTime;
		SNDState.BombPlantSiteIndex = nodeIndex;

		// remove hacker ray from bomb holder
		for (i = 0; i < GAME_MAX_PLAYERS; ++i)
		{
			SNDPlayerState_t * playerState = &SNDState.Players[i];
			Player* p = players[i];
			if (p && playerState->IsBombCarrier)
			{
				DPRINTF("bomb planted %d, carrier %d\n", playerId, i);
				if (i == playerId) {
					playerState->BombsPlanted++;
				}

				playerState->IsBombCarrier = 0;
				GadgetBox* gBox = p->GadgetBox;
				if (gBox)
					gBox->Gadgets[WEAPON_ID_HACKER_RAY].Level = -1;

				// unequip hacker ray if equipped
				if (p->WeaponHeldGun == WEAPON_ID_HACKER_RAY)
					playerEquipWeapon(p, WEAPON_ID_WRENCH);
			}
		}

		// hide the other bomb site
		hideNode(SNDState.Nodes[!nodeIndex].Moby, 0, 0, 0);
	}
	else
	{
		// bomb defused
		uiShowPopup(0, SND_BOMB_DEFUSED);
		uiShowPopup(1, SND_BOMB_DEFUSED);

		DPRINTF("bomb defused from %d\n", playerId);
		if (playerId >= 0) {
			SNDState.Players[playerId].BombsDefused++;

			// check if ninja
			int attackerAlive = 0;
			for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
				Player* p = players[i];
				if (p && !playerIsDead(p) && p->Team == SNDState.AttackerTeamId) {
					attackerAlive = 1;
					break;
				}
			}

			if (attackerAlive) {
				SNDState.Players[playerId].BombsNinjaDefused++;
				DPRINTF("ninja defuse %d\n", playerId);
			}
		}
	}
}

int onSetBombOutcomeRemote(void * connection, void * data)
{
	SNDBombOutcomeMessage_t * message = (SNDBombOutcomeMessage_t*)data;
	onSetBombOutcome(message->NodeIndex, message->Team, message->PlayerId, message->GameTime);

	return sizeof(SNDBombOutcomeMessage_t);
}

void setBombOutcome(int nodeIndex, int team, int playerId)
{
	SNDBombOutcomeMessage_t message;

	// don't allow changing outcome when not host
	if (!SNDState.IsHost)
		return;

	// send out
	message.NodeIndex = nodeIndex;
	message.Team = team;
	message.PlayerId = playerId;
	message.GameTime = gameGetTime();
	netBroadcastCustomAppMessage(NET_DELIVERY_CRITICAL, netGetDmeServerConnection(), CUSTOM_MSG_SET_BOMB_OUTCOME, sizeof(SNDBombOutcomeMessage_t), &message);

	// set locally
	onSetBombOutcome(nodeIndex, team, playerId, message.GameTime);
}

void playTimerTickSound()
{
	((void (*)(Player*, int, int))0x005eb280)((Player*)0x347AA0, 0x3C, 0);
}

void bombTimerLogic()
{
	int gameTime = gameGetTime();
	char strBuf[16];
	
	if (!SNDState.BombDefused && SNDState.BombPlantedTicks > 0 && SNDState.BombPlantSiteIndex >= 0)
	{
		int timeLeft = (MapConfig.BombDetonationTimer * TIME_SECOND) - (gameTime - SNDState.BombPlantedTicks);
		float timeSecondsLeft = timeLeft / (float)TIME_SECOND;
		float scale = SND_BOMB_TIMER_TEXT_SCALE;
		u32 color = 0xFFFFFFFF;
		int timeSecondsLeftFloor = (int)timeSecondsLeft;
		float timeSecondsRounded = timeSecondsLeftFloor;
		if ((timeSecondsLeft - timeSecondsRounded) > 0.5)
			timeSecondsRounded += 1;


		if (timeLeft <= 0)
		{
			// set end
			setRoundOutcome(SND_OUTCOME_BOMB_DETONATED);
		}
		else
		{
			// update scale
			float t = 1-fabsf(timeSecondsRounded - timeSecondsLeft);
			float x = powf(t, 15);
			scale *= (1.0 + (0.3 * x));

			// update color
			color = colorLerp(SND_BOMB_TIMER_BASE_COLOR1, SND_BOMB_TIMER_BASE_COLOR2, fabsf(sinf(gameTime * 10)));
			color = colorLerp(color, SND_BOMB_TIMER_HIGH_COLOR, x);

			// draw timer
			sprintf(strBuf, "%.02f", timeLeft / (float)TIME_SECOND);
			gfxScreenSpaceText(SCREEN_WIDTH/2, SCREEN_HEIGHT * 0.15, scale, scale, color, strBuf, -1, 4);

      // pass to dzo
      if (PATCH_DZO_INTEROP_FUNCS)
      {
        struct CustomDzoCommandSndDrawTimer cmd;
        cmd.Color = color;
        cmd.Size = scale;
        cmd.MsLeft = timeLeft;
        PATCH_DZO_INTEROP_FUNCS->SendCustomCommandToClient(CUSTOM_DZO_CMD_ID_SND_DRAW_TIMER, sizeof(cmd), &cmd);
      }

			// tick timer
			if (timeSecondsLeftFloor < SNDState.Timer.LastPlaySoundSecond)
			{
				SNDState.Timer.LastPlaySoundSecond = timeSecondsLeftFloor;
				playTimerTickSound();
			}
		}
	}
}

void playerLogic(SNDPlayerState_t * playerState)
{
	Player * localPlayer = playerGetFromSlot(0);
	Player * player = playerGetAll()[playerState->PlayerIndex];
	
	if (!player)
	{
		if (playerState->IsBombCarrier)
		{
			playerState->IsBombCarrier = 0;
			SNDState.BombCarrier = 0;

			// Indicate time to spawn
			if (SNDState.IsHost)
			{
				vector_copy(SNDState.SpawnPackAt, MapConfig.PackSpawnPoint);
				SNDState.SpawnPackAt[3] = 1;
			}
			
			// tell team bomb has dropped
			if (localPlayer->Team == SNDState.AttackerTeamId)
				uiShowPopup(0, SND_BOMB_DROPPED);
		}

		return;
	}

	// Check if died
	if (playerIsDead(player))
	{
		// spawn new bomb on bomb carrier death
		if (playerState->IsBombCarrier)
		{
			playerState->IsBombCarrier = 0;
			SNDState.BombCarrier = 0;

			// spawn new pack if host
			if (SNDState.IsHost)
			{
				// if nonstandard death, then spawn back at start
				if ( player->PlayerState == 106 // drown
					|| player->PlayerState == 118 // death fall
					|| player->PlayerState == 122 // death sink
					|| player->PlayerState == 123 // death lava
					|| player->PlayerState == 148 // death no fall
					)
					vector_copy(SNDState.SpawnPackAt, MapConfig.PackSpawnPoint);
				else
					vector_copy(SNDState.SpawnPackAt, player->PlayerPosition);

				SNDState.SpawnPackAt[3] = 1;
			}

			// tell team bomb has dropped
			if (localPlayer->Team == SNDState.AttackerTeamId)
				uiShowPopup(0, SND_BOMB_DROPPED);
		}
	}
}

void resetRoundState(void)
{
	int i;
	Player ** players = playerGetAll();
	Player * player = NULL;
	GameData * gameData = gameGetData();
	int gameTime = gameGetTime();

	// 
	SNDState.RoundInitialized = 0;
	SNDState.RoundEndTicks = 0;
	SNDState.RoundStartTicks = gameTime;
	SNDState.RoundResult = SND_OUTCOME_INCOMPLETE;
	SNDState.BombDefused = 0;
	SNDState.BombPlantSiteIndex = -1;
	SNDState.BombPlantedTicks = 0;
	SNDState.BombCarrier = 0;
	SNDState.SpawnPackAt[3] = 0;

	// 
	SNDState.Timer.LastPlaySoundSecond = MapConfig.BombDetonationTimer;
	SNDState.Timer.Color = 0xFFFFFFFF;

	// Set round time limit
	gameData->TimeEnd = (gameTime - gameData->TimeStart) + (MapConfig.RoundTimelimitSeconds * TIME_SECOND);

	// set capture time to fast (plant speed)
	*(u16*)0x00440E68 = 0x3CA3;
	*(u8*)0x0044111E = 1;

	// iterate players
	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
	{
		player = players[i];
		
		// update state
		SNDState.Players[i].PlayerIndex = i;
		SNDState.Players[i].IsBombCarrier = 0;

		// Remove hacker rays
		if (player)
		{
			if (player->Team == SNDState.AttackerTeamId)
			{
				spawnPlayer(player, MapConfig.AttackTeamSpawnPoint);

				// remove hacker ray from attackers
				GadgetBox* gBox = player->GadgetBox;
				if (gBox)
					gBox->Gadgets[WEAPON_ID_HACKER_RAY].Level = -1;
			}
			else
			{
				spawnPlayer(player, MapConfig.DefendTeamSpawnPoint);
			}
		}
	}

	// Give nodes to defending team
	if (Initialized)
	{
		// move
		showNode(SNDState.Nodes[0].Moby, MapConfig.Node1SpawnPoint);
		showNode(SNDState.Nodes[1].Moby, MapConfig.Node2SpawnPoint);

		// capture
		nodeCapture(SNDState.Nodes[0].OrbGuberMoby, SNDState.DefenderTeamId);
		nodeCapture(SNDState.Nodes[1].OrbGuberMoby, SNDState.DefenderTeamId);
	}

	// spawn hacker ray pack
	if (SNDState.IsHost)
	{
		SNDState.BombPackGuber = (GuberMoby*)spawnPackGuber(MapConfig.PackSpawnPoint, 1 << WEAPON_ID_HACKER_RAY);
	}

	SNDState.RoundInitialized = 1;
}

void spawnGuberHook(void * a0, GuberEvent  * a1)
{
	if (a1->NetEvent.EventID == 0)
	{
		switch (*(u16*)((u32)a1 + 0x10))
		{
			case MOBY_ID_CONQUEST_NODE_TURRET:
			case MOBY_ID_CONQUEST_POWER_TURRET:
			case MOBY_ID_CONQUEST_ROCKET_TURRET:
			{
				return;
			}
		}
	}

	// pass through
	((void (*)(void*,GuberEvent *))0x0061CD30)(a0, a1);
}

void loadGameplayHook(void * gameplayMobies, void * a1, u32 a2)
{
	DPRINTF("loading gameplay at %08X\n", (u32)gameplayMobies);

	//
	int mobyCount = *(int*)gameplayMobies;
	GameplayMobyDef_t* defs = (GameplayMobyDef_t*)((u32)gameplayMobies + 0x10);
	int i;
	int nodeCount = 0;
	VECTOR empty;
	float * point;

	memset(empty, 0, sizeof(empty));

	for (i = 0; i < mobyCount; ++i)
	{
		if (defs->OClass == MOBY_ID_NODE_BASE)
		{
			switch (nodeCount)
			{
				case 0:
				{
					point = MapConfig.Node1SpawnPoint;
					break;
				}
				case 1:
				{
					if (vector_read(MapConfig.Node2SpawnPoint) == 0)
					{

					}
					else
					{
						point = MapConfig.Node2SpawnPoint;
						break;
					}
				}
				default:
				{
					defs->OClass = MOBY_ID_BETA_BOX;
					defs->PVarIndex = -1;
					point = empty;
					break;
				}
			}

			defs->PosX = point[0];
			defs->PosY = point[1];
			defs->PosZ = point[2];
      defs->Occlusion = 1; // disable occlusion
			++nodeCount;
		}
		else if (defs->OClass == MOBY_ID_PLAYER_TURRET 
					|| defs->OClass == MOBY_ID_PICKUP_PAD 
					|| defs->OClass == MOBY_ID_BLUE_TEAM_HEALTH_PAD
					|| defs->OClass == MOBY_ID_HEALTH_PAD0
		)
		{
			defs->OClass = MOBY_ID_BETA_BOX;
			defs->PVarIndex = -1;
			defs->PosX = defs->PosY = defs->PosZ = 0;
		}

		++defs;
	}


	// call load gameplay func
	((void (*)(void*,void*,u32))0x004ECF70)(gameplayMobies, a1, a2);
}


void updateGameState(PatchStateContainer_t * gameState)
{
	int i;

	// game state update
	if (gameState->UpdateGameState)
	{
		gameState->GameStateUpdate.RoundNumber = SNDState.RoundNumber + 1;
	}

	// stats
	if (gameState->UpdateCustomGameStats)
	{
    gameState->CustomGameStatsSize = sizeof(struct SNDGameData);
		struct SNDGameData* sGameData = (struct SNDGameData*)gameState->CustomGameStats.Payload;
		
		sGameData->Version = 2;
		memcpy(sGameData->RoundWinner, SNDState.RoundWinner, sizeof(SNDState.RoundWinner));
		for (i = 0; i < GAME_MAX_PLAYERS; ++i)
		{
			sGameData->BombsDefused[i] = SNDState.Players[i].BombsDefused;
			sGameData->BombsNinjaDefused[i] = SNDState.Players[i].BombsNinjaDefused;
			sGameData->BombsPlanted[i] = SNDState.Players[i].BombsPlanted;
		}
	}
}

/*
 * NAME :		initialize
 * 
 * DESCRIPTION :
 * 			Initializes the game mode.
 * 			Resets states, generates random weapon ordering, generates random alpha mods.
 * 
 * NOTES :
 * 			This is called once at start.
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void initialize(PatchStateContainer_t* gameState)
{
	static int delayStart = 60 * 0.2;
	static int waitingForClientsReady = 0;
	int i = 0;
	GuberMoby * guberMoby = guberMobyGetFirst();
	GameOptions * gameOptions = gameGetOptions();

	// Reset snd state
	SNDState.RoundNumber = 0;
	SNDState.TeamWins[0] = 0;
	SNDState.TeamWins[1] = 0;
	SNDState.TeamRolesFlipped = 0;
	SNDState.GameOver = 0;
	SNDState.Nodes[0].Moby = 0;
	SNDState.Nodes[1].Moby = 0;
	SNDState.Nodes[0].GuberMoby = 0;
	SNDState.Nodes[1].GuberMoby = 0;
	SNDState.Nodes[0].OrbGuberMoby = 0;
	SNDState.Nodes[1].OrbGuberMoby = 0;
	SNDState.BombPackMoby = 0;
	SNDState.BombPackGuber = 0;
	SNDState.DefenderTeamId = TEAM_BLUE;
	SNDState.AttackerTeamId = TEAM_RED;
	SNDState.NodeCount = 2;
	memset(SNDState.RoundWinner, -1, sizeof(SNDState.RoundWinner));

	// 
	if (vector_read(MapConfig.Node2SpawnPoint) == 0)
		SNDState.NodeCount = 1;

	// Hook set outcome net event
	netInstallCustomMsgHandler(CUSTOM_MSG_SET_ROUND_OUTCOME, &onSetRoundOutcomeRemote);
	netInstallCustomMsgHandler(CUSTOM_MSG_SET_BOMB_OUTCOME, &onSetBombOutcomeRemote);

	// Install spawn pack hook
	*(u32*)0x0061CDC8 = 0x0C000000 | ((u32)&spawnPackHook / 4);

	// Enable hacker ray in packs
	*(u8*)0x00413DB4 = 40;

	// Disable cq popup messages
	*(u32*)0x003D2E6C = 0;

	// Disable normal game ending
	*(u32*)0x006219B8 = 0;	// survivor (8)
	*(u32*)0x00620F54 = 0;	// time end (1)
	*(u32*)0x00621240 = 0;	// homenode (4)

	// Remove blip type write
	*(u32*)0x00553C5C = 0;

	// Overwrite 'you picked up a weapon pack' string to pickup bomb message
	replaceString(0x2331, SND_BOMB_YOU_PICKED_UP);

	// 
	if (delayStart > 0)
	{
		--delayStart;
		return;
	}

  // wait for all clients to be ready
  // or for 15 seconds
  if (!gameState->AllClientsReady && waitingForClientsReady < (5 * 60)) {
    uiShowPopup(0, "Waiting For Players...");
    ++waitingForClientsReady;
    return;
  }

  // hide waiting for players popup
  hudHidePopup();

	// 
	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
	{
		SNDState.Players[i].BombsDefused = 0;
		SNDState.Players[i].BombsNinjaDefused = 0;
		SNDState.Players[i].BombsPlanted = 0;
	}

	// Initialize scoreboard
	for (i = 0; i < 2; ++i)
	{
		TeamScores[i].TeamId = i;
		TeamScores[i].UNK = 0;
		TeamScores[i].Value = 0;
	}

	// 
	ScoreboardChanged = 1;

	// Disable packs
	cheatsApplyNoPacks();

	// 
	SNDState.RadarObjectiveMoby[0] = mobySpawn(MOBY_ID_BETA_BOX, 0);
	SNDState.RadarObjectiveMoby[0]->PClass = 0;
	SNDState.RadarObjectiveMoby[0]->CollData = 0;
	SNDState.RadarObjectiveMoby[1] = mobySpawn(MOBY_ID_BETA_BOX, 0);
	SNDState.RadarObjectiveMoby[1]->PClass = 0;
	SNDState.RadarObjectiveMoby[1]->CollData = 0;

	// Write patch to hook GuberMoby event handler
	*(u32*)0x0061CB30 = 0x8C460014; // move func ptr to a2
	*(u32*)0x0061CB38 = 0x0C000000 | ((u32)&GuberMobyEventHandler / 4); // call our func

	// Use gubers to find our nodes
	while (guberMoby)
	{
		GuberMoby * next = (GuberMoby*)guberMoby->Guber.Prev;

		if (guberMoby->Moby && guberMoby->Moby->OClass == MOBY_ID_NODE_BASE && guberMoby->Moby->Position[0] > 0)
		{
			if (SNDState.Nodes[0].Moby == 0)
			{
				SNDState.Nodes[0].GuberMoby = guberMoby;
				SNDState.Nodes[0].Moby = guberMoby->Moby;
				DPRINTF("Node1: %08x\n", (u32)guberMoby->Moby);
			}
			else if (SNDState.Nodes[1].Moby == 0)
			{
				SNDState.Nodes[1].GuberMoby = guberMoby;
				SNDState.Nodes[1].Moby = guberMoby->Moby;
				DPRINTF("Node2: %08x\n", (u32)guberMoby->Moby);
			}
		}

		guberMoby = next;
	}
	
	// reset snd round state
	resetRoundState();

	Initialized = 1;
	InitializedTime = gameGetTime();
}

/*
 * NAME :		gameStart
 * 
 * DESCRIPTION :
 * 			Snd game logic entrypoint.
 * 
 * NOTES :
 * 			This is called only when in game.
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void gameStart(struct GameModule * module, PatchStateContainer_t * gameState)
{
	int i = 0;
	GameSettings * gameSettings = gameGetSettings();
	Player * localPlayer = (Player*)0x00347AA0;
	Player ** players = playerGetAll();
	int gameTime = gameGetTime();
	GameData * gameData = gameGetData();

  asm (".set noreorder;");

	// Ensure in game
	if (!gameSettings)
		return;

	// Determine if host
	SNDState.IsHost = gameAmIHost(); // gameIsHost(localPlayer->Guber.Id.GID.HostId);

	// Initialize if not yet initialized
	if (!Initialized)
	{
		initialize(gameState);
		return;
	}

#if DEBUG
	if (!SNDState.GameOver && padGetButton(0, PAD_L3 | PAD_R3) > 0)
		SNDState.GameOver = 1;
	if (!SNDState.GameOver && padGetButton(0, PAD_L1 | PAD_UP) > 0 && SNDState.IsHost && !SNDState.BombPlantedTicks)
		setBombOutcome(0, SNDState.AttackerTeamId, -1);
	if (!SNDState.GameOver && padGetButton(0, PAD_L1 | PAD_DOWN) > 0 && SNDState.IsHost && !SNDState.BombPlantedTicks)
		setBombOutcome(1, SNDState.AttackerTeamId, -1);
	if (!SNDState.GameOver && padGetButton(0, PAD_L1 | PAD_LEFT) > 0 && SNDState.IsHost && SNDState.BombPlantedTicks)
		setBombOutcome(0, SNDState.DefenderTeamId, -1);
	if (!SNDState.GameOver && padGetButton(0, PAD_L1 | PAD_RIGHT) > 0 && SNDState.IsHost && SNDState.BombPlantedTicks)
		setBombOutcome(1, SNDState.DefenderTeamId, -1);
#endif

	//
	updateGameState(gameState);

	if (!gameHasEnded() && isInGame() && !SNDState.GameOver)
	{
		if (SNDState.RoundEndTicks)
		{
      // global chat between rounds
      voiceEnableGlobalChat(1);

			// Destroy pack and disable timer
      if (gameData->TimeEnd != -1) {
			  killPack();
			  gameData->TimeEnd = -1;
      }

			// Handle game outcome
			if (SNDState.RoundResult)
			{
				switch (SNDState.RoundResult)
				{
					case SND_OUTCOME_TIME_END:
					case SND_OUTCOME_ATTACKERS_DEAD:
					case SND_OUTCOME_BOMB_DEFUSED:
					{
						// defenders win
						SNDState.RoundWinner[SNDState.RoundNumber] = (u8)(SNDState.BombPlantSiteIndex << 6) | (SNDState.TeamRolesFlipped << 4) | 0x00;
						DPRINTF("round result %02X\n", (u8)SNDState.RoundWinner[SNDState.RoundNumber]);
						if (++SNDState.TeamWins[SNDState.DefenderTeamId] >= MapConfig.RoundsToWin)
							SNDState.GameOver = 1;
						
						SNDState.RoundLastWinners = SNDState.DefenderTeamId;
						break;
					}
					case SND_OUTCOME_BOMB_DETONATED:
					{
						// attackers win
						SNDState.RoundWinner[SNDState.RoundNumber] = (u8)(SNDState.BombPlantSiteIndex << 6) | (SNDState.TeamRolesFlipped << 4) | 0x01;
						DPRINTF("round result %02X\n", (u8)SNDState.RoundWinner[SNDState.RoundNumber]);
						if (++SNDState.TeamWins[SNDState.AttackerTeamId] >= MapConfig.RoundsToWin)
							SNDState.GameOver = 1;

						SNDState.RoundLastWinners = SNDState.AttackerTeamId;
						break;
					}
				}

				// update current winning team
				SNDState.WinningTeam = -1;
				if (SNDState.TeamWins[SNDState.DefenderTeamId] > SNDState.TeamWins[SNDState.AttackerTeamId])
					SNDState.WinningTeam = SNDState.DefenderTeamId;
				else if (SNDState.TeamWins[SNDState.DefenderTeamId] < SNDState.TeamWins[SNDState.AttackerTeamId])
					SNDState.WinningTeam = SNDState.AttackerTeamId;
				
				ScoreboardChanged = 1;
				SNDState.RoundResult = SND_OUTCOME_INCOMPLETE;
			}

			if (gameTime > SNDState.RoundEndTicks)
			{
				// increment round counter
				SNDState.RoundNumber += 1;

				if (SNDState.RoundNumber >= SND_MAX_ROUNDS)
				{
					// reach max number of rounds
					// game must be a draw
					SNDState.WinningTeam = -1;
					SNDState.GameOver = 1;
				}
				else
				{
					// handle half time
					if ((SNDState.RoundNumber % MapConfig.RoundsToFlip) == 0)
					{
						SNDState.TeamRolesFlipped = !SNDState.TeamRolesFlipped;
						SNDState.DefenderTeamId = !SNDState.DefenderTeamId;
						SNDState.AttackerTeamId = !SNDState.AttackerTeamId;
					}

					// reset
					resetRoundState();
				}
			}
			else
			{
				if (localPlayer->Team == SNDState.RoundLastWinners)
					drawRoundMessage(SND_ROUND_WIN, 1.5);
				else
					drawRoundMessage(SND_ROUND_LOSS, 1.5);
			}
		}
		else
		{
      int roundJustStarted = (gameTime - SNDState.RoundStartTicks) < (5 * TIME_SECOND);

      // global chat if dead, team otherwise
      voiceEnableGlobalChat(playerIsDead(localPlayer));

			// Set lifetime of bomb pack moby
			if (SNDState.BombPackMoby)
			{
				if (SNDState.BombPackMoby->OClass != MOBY_ID_WEAPON_PACK)
				{
					SNDState.BombPackMoby = NULL;
					SNDState.BombPackGuber = NULL;
				}
				else
				{
					setPackLifetime(0x01ffffff);
				}
			}

			// Display hello
			if ((SNDState.RoundNumber % MapConfig.RoundsToFlip) == 0 && roundJustStarted)
			{
				if (localPlayer->Team == SNDState.DefenderTeamId)
					drawRoundMessage(SND_DEFEND_HELLO, 1);
				else
					drawRoundMessage(SND_ATTACK_HELLO, 1);
			}

			// Draw objective
			int targetTeams[2] = {0,1};
			Moby * target[2] = {0,0};

			// If bomb is planted, make objective bombsite for all
			if (SNDState.BombPlantSiteIndex >= 0)
			{
				target[0] = SNDState.Nodes[SNDState.BombPlantSiteIndex].Moby;
				targetTeams[0] = SNDState.BombPlantSiteIndex ? SND_NODE2_BLIP_TEAM : SND_NODE1_BLIP_TEAM;
				enableBlipPulsing();
			}
			else if (localPlayer->Team == SNDState.DefenderTeamId || SNDState.BombCarrier)
			{
				// Defenders and bomb carrier's targets are the two nodes
				target[0] = SNDState.Nodes[0].Moby;
				target[1] = SNDState.Nodes[1].Moby;
				targetTeams[0] = SND_NODE1_BLIP_TEAM;
				targetTeams[1] = SND_NODE2_BLIP_TEAM;

				if (localPlayer->Team == SNDState.DefenderTeamId)
				  disableBlipPulsing();
        else
          enableBlipPulsing();
			}
			else if (SNDState.BombPackMoby)
			{
				// attackers, make objective bomb pack
				target[0] = SNDState.BombPackMoby;
				enableBlipPulsing();
			}
      else if (localPlayer->Team == SNDState.AttackerTeamId)
      {
        // bomb is carried by attacking team
        // we're not the carrier
        // so enable pulsing to show who has the bomb on attacking team
        enableBlipPulsing();
      }

			// set objectives
			for (i = 0; i < SNDState.NodeCount; ++i)
			{
				if (target[i])
				{
					int blipId = radarGetBlipIndex(SNDState.RadarObjectiveMoby[i]);
					if (blipId >= 0)
					{
						RadarBlip * blip = radarGetBlips() + blipId;
						blip->X = target[i]->Position[0];
						blip->Y = target[i]->Position[1];
						blip->Life = 0x1F;
						blip->Type = 0x11;
						blip->Team = targetTeams[i];
					}
				}
			}
				
			// set bomb carrier as another objective if attacking team
			for (i = 0; i < GAME_MAX_PLAYERS; ++i)
			{
				Player* p = players[i];
				if (p)
				{
					int blipId = radarGetBlipIndex(p->PlayerMoby);
					if (blipId >= 0)
					{
						RadarBlip * blip = radarGetBlips() + blipId;
						blip->Type = (SNDState.Players[i].IsBombCarrier && localPlayer->Team == SNDState.AttackerTeamId) ? 0x01 : 0x00;
					}
				}
			}

			int attackersAlive = 0, defendersAlive = 0;
			int hasAttackers = 0, hasDefenders = 0;
			for (i = 0; i < GAME_MAX_PLAYERS; ++i)
			{
				Player * p = players[i];
				playerLogic(&SNDState.Players[i]);

				if (p)
				{
          int isDead = playerIsDead(p);

					// turn off blown up state if not dead
					if (!isDead)
					{
						p->Explode = 0;
						p->Invisible = 0;
					}

					if (p->Team == SNDState.AttackerTeamId)
					{
						hasAttackers = 1;
						if (!isDead)
							attackersAlive = 1;
					}
					else if (p->Team == SNDState.DefenderTeamId)
					{
						hasDefenders = 1;
						if (!isDead)
							defendersAlive = 1;
					}
				}
			}

			// host specific logic
			if (SNDState.IsHost)
			{
        if (!roundJustStarted)
        {
          // End round if timelimit hit and no bomb planted
          if (SNDState.BombPlantSiteIndex < 0 && (gameTime - SNDState.RoundStartTicks) > (MapConfig.RoundTimelimitSeconds * TIME_SECOND))
          {
            setRoundOutcome(SND_OUTCOME_TIME_END);
          }

          // no attackers alive and bomb hasn't been planted
          if (hasAttackers && !attackersAlive && !SNDState.BombPlantedTicks)
          {
            setRoundOutcome(SND_OUTCOME_ATTACKERS_DEAD);
          }

          // no defenders alive and bomb has been planted
          if (hasDefenders && !defendersAlive && SNDState.BombPlantedTicks)
          {
            setRoundOutcome(SND_OUTCOME_BOMB_DETONATED);
          }

          // no defenders alive and no attackers alive so just finish the round
          if (!defendersAlive && !attackersAlive)
          {
            if (!SNDState.BombPlantedTicks)
              setRoundOutcome(SND_OUTCOME_ATTACKERS_DEAD);
            else
              setRoundOutcome(SND_OUTCOME_BOMB_DETONATED);
          }
        }

				// Handle spawning pack
				if (!SNDState.RoundEndTicks && SNDState.SpawnPackAt[3] > 0)
				{
					SNDState.SpawnPackAt[3] = 0;
					SNDState.BombPackGuber = (GuberMoby*)spawnPackGuber(SNDState.SpawnPackAt, 1 << WEAPON_ID_HACKER_RAY);
				}
			}

			if (SNDState.BombPlantedTicks)
				gameData->TimeEnd = -1;
			else
				gameData->TimeEnd = (SNDState.RoundStartTicks - gameData->TimeStart) + (MapConfig.RoundTimelimitSeconds * TIME_SECOND);

			//
			bombTimerLogic();
		}
	}
	else if (isInGame())
	{
		// Kill pack
		killPack();

		// set winner
		gameSetWinner(SNDState.WinningTeam, 1);

		// end game
		if (SNDState.GameOver == 1)
		{
			gameEnd(4);
			SNDState.GameOver = 2;
		}
	}

	// 
	updateScoreboard();

	// Update scoreboard on change
	if (ScoreboardChanged)
	{
		GAME_SCOREBOARD_ITEM_COUNT = 2;
		GAME_SCOREBOARD_NODE_TARGET = SND_MAX_ROUNDS;
		GAME_SCOREBOARD_TARGET = MapConfig.RoundsToWin;
		GAME_SCOREBOARD_REFRESH_FLAG = 1;
		ScoreboardChanged = 0;
	}

	return;
}

//--------------------------------------------------------------------------
void setLobbyGameOptions(PatchGameConfig_t * gameConfig)
{
	// conquest homenodes options
	static char cqOptions[] = { 
		1, 1, 			// 0x06 - 0x08
		0, 1, 1, 0, 	// 0x08 - 0x0C
		0, 0, 0, 0,  	// 0x0C - 0x10
		0, 0, 0, 0,		// 0x10 - 0x14
		-1, -1, 1, 1,	// 0x14 - 0x18
	};

	// set game options
	GameOptions * gameOptions = gameGetOptions();
	GameSettings* gameSettings = gameGetSettings();
	if (!gameOptions || !gameSettings || gameSettings->GameLoadStartTime <= 0)
		return;
	
  // disable healthboxes
  gameConfig->grNoHealthBoxes = 2;
  gameConfig->grCqDisableTurrets = 0;
  gameConfig->grCqDisableUpgrades = 0;
  gameConfig->grCqPersistentCapture = 0;

	// set to conquest homenodes
	memcpy((void*)&gameOptions->GameFlags.Raw[6], (void*)cqOptions, sizeof(cqOptions)/sizeof(char));

	// force hacker orbs
	gameOptions->GameFlags.MultiplayerGameFlags.NodeType = 1;

	gameOptions->GameFlags.MultiplayerGameFlags.KillsToWin = 0;
	gameOptions->GameFlags.MultiplayerGameFlags.Survivor = 1;
	gameOptions->GameFlags.MultiplayerGameFlags.RespawnTime = -1;
  gameOptions->GameFlags.MultiplayerGameFlags.RadarBlips = 0;
}

/*
 * NAME :		lobbyStart
 * 
 * DESCRIPTION :
 * 			Infected lobby logic entrypoint.
 * 
 * NOTES :
 * 			This is called only when in lobby.
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void lobbyStart(struct GameModule * module, PatchStateContainer_t * gameState)
{
	int i;
	int activeId = uiGetActive();
	static int initializedScoreboard = 0;

	// 
	updateGameState(gameState);

	// scoreboard
	switch (activeId)
	{
		case 0x15C:
		{
			if (initializedScoreboard)
				break;

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

/*
 * NAME :		loadStart
 * 
 * DESCRIPTION :
 * 			SND load logic entrypoint.
 * 
 * NOTES :
 * 			This is called only when the game has finished reading the level from the disc and before it has started processing the data.
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */

void loadStart(struct GameModule * module, PatchStateContainer_t * gameState)
{
	setLobbyGameOptions(gameState->GameConfig);

	// only handle when loading level
	GameSettings* gs = gameGetSettings();
	if (!gs || gs->GameStartTime >= 0)
		return;

	// Hook load gameplay file
	if (*(u32*)0x004EE664 == 0x0C13B3DC)
		*(u32*)0x004EE664 = 0x0C000000 | (u32)&loadGameplayHook / 4;

	// Patch spawning node turrets
	if (*(u32*)0x0061CB18 == 0x0C18734C)
		*(u32*)0x0061CB18 = 0x0C000000 | (u32)&spawnGuberHook / 4;
    
  // read extra data
  gameState->ReadExtraDataFunc(&MapConfig, 0x50);
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
