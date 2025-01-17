/***************************************************
 * FILENAME :		main.c
 * 
 * DESCRIPTION :
 * 		Infected entrypoint and logic.
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
#include <libdl/graphics.h>
#include <libdl/net.h>
#include <libdl/utils.h>
#include "module.h"
#include "common.h"
#include "messageid.h"
#include "include/game.h"


#define SPLEEF_BOARD_DIMENSION							(10)
#define SPLEEF_BOARD_LEVELS									(2)
#define SPLEEF_BOARD_LEVEL_OFFSET						(40.0)
#define SPLEEF_BOARD_BOX_SIZE								(4.0)
#define SPLEEF_BOARD_SPAWN_RADIUS						(SPLEEF_BOARD_BOX_SIZE * ((SPLEEF_BOARD_DIMENSION + SPLEEF_BOARD_DIMENSION) / 5))
#define SPLEEF_BOARD_BOX_MAX								(SPLEEF_BOARD_DIMENSION * SPLEEF_BOARD_DIMENSION * SPLEEF_BOARD_LEVELS)


const char * SPLEEF_ROUND_WIN = "First!";
const char * SPLEEF_ROUND_SECOND = "Second!";
const char * SPLEEF_ROUND_THIRD = "Third!";
const char * SPLEEF_ROUND_LOSS = "Better luck next time!";

/*
 *
 */
int Initialized = 0;

/*
 *
 */
SpleefState_t SpleefState;

/*
 *
 */
typedef struct SpleefOutcomeMessage
{
	char Outcome[4];
} SpleefOutcomeMessage_t;

/*
 *
 */
typedef struct SpleefDestroyBoxMessage
{
	short BoxId;
	char PlayerId;
} SpleefDestroyBoxMessage_t;


/*
 *
 */
struct SpleefGameData
{
	u32 Version;
	u32 Rounds;
	int Points[GAME_MAX_PLAYERS];
	int BoxesDestroyed[GAME_MAX_PLAYERS];
};


/*
 *
 */
enum GameNetMessage
{
	CUSTOM_MSG_SET_OUTCOME = CUSTOM_MSG_ID_GAME_MODE_START,
	CUSTOM_MSG_DESTROY_BOX
};

/*
 *
 */
Moby* SpleefBox[SPLEEF_BOARD_DIMENSION * SPLEEF_BOARD_DIMENSION * SPLEEF_BOARD_LEVELS];

/*
 *
 */
Moby* SourceBox;

/*
 * Position that boxes are spawned to.
 */
VECTOR StartPos = {
	400,
	400,
	800,
	0
};

/*
 *
 */
VECTOR StartUNK_80 = {
	0.00514222,
	-0.0396723,
	62013.9,
	62013.9
};

void initializeScoreboard(void);

// forwards
void onSetRoundOutcome(char outcome[4]);
int onSetRoundOutcomeRemote(void * connection, void * data);
void setRoundOutcome(int first, int second, int third);

//--------------------------------------------------------------------------
void getWinningPlayer(int * winningPlayerId, int * winningPlayerScore)
{
	int i;
	int pId = 0;
	int pScore = 0;
	Player ** players = playerGetAll();
	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
	{
		if (SpleefState.PlayerPoints[i] > pScore)
		{
			pId = i;
			pScore = SpleefState.PlayerPoints[i];
		}
	}

	*winningPlayerScore = pScore;
	*winningPlayerId = pId;
}

//--------------------------------------------------------------------------
void onSetRoundOutcome(char outcome[4])
{
	memcpy(SpleefState.RoundResult, outcome, 4);
	DPRINTF("outcome set to %d,%d,%d,%d\n", outcome[0], outcome[1], outcome[2], outcome[3]);
}

//--------------------------------------------------------------------------
int onSetRoundOutcomeRemote(void * connection, void * data)
{
	SpleefOutcomeMessage_t * message = (SpleefOutcomeMessage_t*)data;
	onSetRoundOutcome(message->Outcome);

	return sizeof(SpleefOutcomeMessage_t);
}

//--------------------------------------------------------------------------
void setRoundOutcome(int first, int second, int third)
{
	SpleefOutcomeMessage_t message;

	// don't allow overwriting existing outcome
	if (SpleefState.RoundResult[0])
		return;

	// don't allow changing outcome when not host
	if (!SpleefState.IsHost)
		return;

	// send out
	message.Outcome[0] = 1;
	message.Outcome[1] = first;
	message.Outcome[2] = second;
	message.Outcome[3] = third;
	netBroadcastCustomAppMessage(NET_DELIVERY_CRITICAL, netGetDmeServerConnection(), CUSTOM_MSG_SET_OUTCOME, sizeof(SpleefOutcomeMessage_t), &message);

	// set locally
	onSetRoundOutcome(message.Outcome);
}

//--------------------------------------------------------------------------
void onDestroyBox(int id, int playerId)
{
	Moby* box = SpleefBox[id];
	if (box && box->OClass == MOBY_ID_NODE_BOLT_GUARD && !mobyIsDestroyed(box))
	{
		mobyDestroy(box);
	}

	SpleefBox[id] = NULL;

	// 
	if (playerId >= 0)
		SpleefState.PlayerBoxesDestroyed[playerId]++;

	DPRINTF("box destroyed %d by %d\n", id, playerId);
}

//--------------------------------------------------------------------------
int onDestroyBoxRemote(void * connection, void * data)
{
	SpleefDestroyBoxMessage_t * message = (SpleefDestroyBoxMessage_t*)data;

	// if the round hasn't ended
	if (!SpleefState.RoundEndTicks)
		onDestroyBox(message->BoxId, message->PlayerId);

	return sizeof(SpleefDestroyBoxMessage_t);
}

//--------------------------------------------------------------------------
void destroyBox(int id, int playerId)
{
	SpleefDestroyBoxMessage_t message;

	// send out
	message.BoxId = id;
	message.PlayerId = playerId;
	netBroadcastCustomAppMessage(NET_DELIVERY_CRITICAL, netGetDmeServerConnection(), CUSTOM_MSG_DESTROY_BOX, sizeof(SpleefDestroyBoxMessage_t), &message);

	// 
	if (playerId >= 0)
		SpleefState.PlayerBoxesDestroyed[playerId]++;
		
	DPRINTF("sent destroy box %d\n", id);
}

//--------------------------------------------------------------------------
int gameGetTeamScore(int team, int score)
{
	int i = 0;
	int totalScore = 0;
	Player** players = playerGetAll();

	for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
		if (players[i] && players[i]->Team == team) {
			totalScore += SpleefState.PlayerPoints[i];
		}
	}
	
  return totalScore;
}

//--------------------------------------------------------------------------
void changeBoxCollisionIds(void * modelPtr)
{
	int i = 0;

	// Collision offset at +0x10
	u32 colPtr = *(u32*)((u32)modelPtr + 0x10) + 0x70;

	for (i = 0; i < 12; ++i)
	{
		*(u8*)(colPtr + 3) = 0x02;
		colPtr += 4;
	}
}

//--------------------------------------------------------------------------
void boxUpdate(Moby* moby)
{
	int i;
	GameSettings* gameSettings = gameGetSettings();
	MobyColDamage* colDamage = mobyGetDamage(moby, 0xfffffff, 0);

	if (moby->CollDamage != -1 && colDamage && colDamage->Moby == moby)
	{
		int damageClientId = colDamage->Damager->NetObjectGid.HostId;
		if (gameGetMyClientId() == damageClientId)
		{
			// call base
			((void (*)(Moby*))0x00427450)(moby);

			int playerId = 0;
			for (playerId = 0; playerId < GAME_MAX_PLAYERS; ++playerId)
			{
				if (gameSettings->PlayerClients[playerId] == damageClientId)
					break;
			}
			if (playerId ==GAME_MAX_PLAYERS)
				playerId = -1;

			for (i = 0; i < SPLEEF_BOARD_BOX_MAX; ++i)
			{
				if (SpleefBox[i] == moby)
				{
					destroyBox(i, playerId);
					mobyDestroy(moby);
					SpleefBox[i] = NULL;
					return;
				}
			}

			return;
		}

		// remove damage
		moby->CollDamage = -1;
	}

	// call base
	((void (*)(Moby*))0x00427450)(moby);
}

//--------------------------------------------------------------------------
void drawRoundMessage(const char * message, float scale)
{
	GameSettings * gameSettings = gameGetSettings();
	char* rankStrings[] = { "1st", "2nd", "3rd" };
	int fw = gfxGetFontWidth(message, -1, scale);
	float x = 0.5 * SCREEN_WIDTH;
	float y = 0.16 * SCREEN_HEIGHT;
	float p = 10;
	float w = maxf(196.0, fw);
	float h = 120.0;
	int i;

	// draw container
  gfxHelperDrawBox(x, y, 0, 0, w + p, h + p, 0x20FFFFFF, TEXT_ALIGN_TOPCENTER, COMMON_DZO_DRAW_NORMAL);

	// draw message
  gfxHelperDrawText(x, y, 0, 5, scale, 0x80FFFFFF, message, -1, TEXT_ALIGN_TOPCENTER, COMMON_DZO_DRAW_NORMAL);

	// draw ranks
	y += 24.0 * scale;
	scale *= 0.5;
	w /= 1.5;
	for (i = 1; i < 4; ++i)
	{
		int pId = SpleefState.RoundResult[i];
		if (pId >= 0)
		{
			y += 18.0 * scale;
      gfxHelperDrawText(x, y, -(w/2), 0, scale, 0x80FFFFFF, rankStrings[i-1], -1, TEXT_ALIGN_MIDDLELEFT, COMMON_DZO_DRAW_NORMAL);
      gfxHelperDrawText(x, y, (w/2), 0, scale, 0x80FFFFFF, gameSettings->PlayerNames[pId], -1, TEXT_ALIGN_MIDDLERIGHT, COMMON_DZO_DRAW_NORMAL);
		}
	}
}

//--------------------------------------------------------------------------
void updateGameState(PatchStateContainer_t * gameState)
{
	int i,j;

	// game state update
	if (gameState->UpdateGameState)
	{
		gameState->GameStateUpdate.RoundNumber = SpleefState.RoundNumber + 1;
	}

	// stats
	if (gameState->UpdateCustomGameStats)
	{
    gameState->CustomGameStatsSize = sizeof(struct SpleefGameData);
		struct SpleefGameData* sGameData = (struct SpleefGameData*)gameState->CustomGameStats.Payload;
		sGameData->Rounds = SpleefState.RoundNumber+1;
		DPRINTF("spleef ran for %d rounds\n", sGameData->Rounds);
		sGameData->Version = 0x00000001;
		
		for (i = 0; i < GAME_MAX_PLAYERS; ++i)
		{
			DPRINTF("%d: %d points %d boxes\n", i, SpleefState.PlayerPoints[i], SpleefState.PlayerBoxesDestroyed[i]);
			sGameData->Points[i] = SpleefState.PlayerPoints[i];
			sGameData->BoxesDestroyed[i] = SpleefState.PlayerBoxesDestroyed[i];
		}
	}
}

//--------------------------------------------------------------------------
void resetRoundState(void)
{
	GameSettings * gameSettings = gameGetSettings();
	Player ** players = playerGetAll();
	int gameTime = gameGetTime();
	int i,j,k, count=0;
	VECTOR pos, rot = {0,0,0,0}, center;
	Moby* hbMoby = 0;

	// 
	SpleefState.RoundInitialized = 0;
	SpleefState.RoundStartTicks = gameTime;
	SpleefState.RoundEndTicks = 0;
	SpleefState.RoundResult[0] = 0;
	SpleefState.RoundResult[1] = -1;
	SpleefState.RoundResult[2] = -1;
	SpleefState.RoundResult[3] = -1;
	memset(SpleefState.RoundPlayerState, -1, GAME_MAX_PLAYERS);

	// Center
	center[0] = StartPos[0] + (SPLEEF_BOARD_BOX_SIZE * (SPLEEF_BOARD_DIMENSION / (float)2.0));
	center[1] = StartPos[1] + (SPLEEF_BOARD_BOX_SIZE * (SPLEEF_BOARD_DIMENSION / (float)2.0));
	center[2] = StartPos[2];

	// spawn players
	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
	{
		Player * p = players[i];
		if (!p)
			continue;

		// set state
		SpleefState.RoundPlayerState[i] = 0;

		// move to spawn
		float theta = (p->PlayerId / (float)gameSettings->PlayerCount) * (float)2.0 * MATH_PI;
		while (theta > MATH_TAU)
			theta -= MATH_PI;

		pos[0] = center[0] + (cosf(theta) * SPLEEF_BOARD_SPAWN_RADIUS);
		pos[1] = center[1] + (sinf(theta) * SPLEEF_BOARD_SPAWN_RADIUS);
		pos[2] = center[2] + (float)30;

		// 
		rot[2] = theta - MATH_PI;

		// 
		playerRespawn(p);
		playerSetPosRot(p, pos, rot);
	}

	// reset boxes
	vector_copy(pos, StartPos);
	memset(rot, 0, sizeof(rot));
	pos[3] = SourceBox->Position[3];

	// Spawn boxes
	for (k = 0; k < SPLEEF_BOARD_LEVELS; ++k)
	{
		for (i = 0; i < SPLEEF_BOARD_DIMENSION; ++i)
		{
			for (j = 0; j < SPLEEF_BOARD_DIMENSION; ++j)
			{
				// delete old one
				int boxId = (k * SPLEEF_BOARD_DIMENSION * SPLEEF_BOARD_DIMENSION) + (i * SPLEEF_BOARD_DIMENSION) + j;
				if (!SpleefBox[boxId] || SpleefBox[boxId]->OClass != MOBY_ID_NODE_BOLT_GUARD || mobyIsDestroyed(SpleefBox[boxId]))
				{
					// spawn
					SpleefBox[boxId] = hbMoby = mobySpawn(MOBY_ID_NODE_BOLT_GUARD, 0);

					if (hbMoby)
					{
						vector_copy(hbMoby->Position, pos);

						hbMoby->UpdateDist = 0xFF;
						hbMoby->Drawn = 0x01;
						hbMoby->DrawDist = 0x0080;
						hbMoby->Opacity = 0x80;
						hbMoby->State = 1;

						hbMoby->Scale = (float)0.0425 * SPLEEF_BOARD_BOX_SIZE;
						hbMoby->Lights = 0x202;
						hbMoby->GuberMoby = 0;
						hbMoby->PUpdate = &boxUpdate;

						// For this model the vector here is copied to 0x80 in the moby
						// This fixes the occlusion bug
						vector_copy(hbMoby->LSphere, StartUNK_80);

						// Copy from source box
						hbMoby->AnimSeq = SourceBox->AnimSeq;
						hbMoby->PClass = SourceBox->PClass;
						hbMoby->CollData = SourceBox->CollData;
						hbMoby->MClass = SourceBox->MClass;

						++count;
					}
				}

				pos[1] += SPLEEF_BOARD_BOX_SIZE;
			}

			pos[0] += SPLEEF_BOARD_BOX_SIZE;
			pos[1] = StartPos[1];
		}

		pos[0] = StartPos[0];
		pos[1] = StartPos[1];
		pos[2] -= SPLEEF_BOARD_LEVEL_OFFSET;
	}

	// 
	SpleefState.RoundInitialized = 1;

	// this has to be here otherwise the rounds won't reset correctly
	// think that this eats cycles and that helps sync things maybe?
	// not super sure
	printf("count: %d, source: %08x, new: %08x\n", count, (u32)SourceBox, (u32)hbMoby);

	// 
	#if DEBUG
		if (hbMoby)
			hbMoby->Opacity = 0xFF;
		printf("Round %d started\n", SpleefState.RoundNumber);
	#endif
}

//--------------------------------------------------------------------------
int whoKilledMeHook(void)
{
	return 0;
}

/*
 * NAME :		initialize
 * 
 * DESCRIPTION :
 * 			Initializes the gamemode.
 * 
 * NOTES :
 * 			This is called only once at the start.
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void initialize(PatchStateContainer_t* gameState)
{
  static int startDelay = 60 * 0.2;
	static int waitingForClientsReady = 0;
	GameSettings * gameSettings = gameGetSettings();
	GameOptions * gameOptions = gameGetOptions();
  PatchGameConfig_t* gameConfig = gameState->GameConfig;
	Player ** players = playerGetAll();
	int i;

	// Set death barrier
	gameSetDeathHeight(StartPos[2] - (SPLEEF_BOARD_LEVEL_OFFSET * SPLEEF_BOARD_LEVELS) - 10);

	// Set survivor
	gameOptions->GameFlags.MultiplayerGameFlags.Survivor = 1;
	gameOptions->GameFlags.MultiplayerGameFlags.RespawnTime = -1;

	// Hook set outcome net event
	netInstallCustomMsgHandler(CUSTOM_MSG_SET_OUTCOME, &onSetRoundOutcomeRemote);
	netInstallCustomMsgHandler(CUSTOM_MSG_DESTROY_BOX, &onDestroyBoxRemote);
	
	// Disable normal game ending
	*(u32*)0x006219B8 = 0;	// survivor (8)
	*(u32*)0x00621A10 = 0;  // survivor (8)

  if (startDelay) {
    --startDelay;
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

	// Spawn box so we know the correct model and collision pointers
	SourceBox = mobySpawn(MOBY_ID_BETA_BOX, 0);
	
	// change collision ids
	//changeBoxCollisionIds(SourceBox->PClass);

	// clear spleefbox array
	memset(SpleefBox, 0, sizeof(SpleefBox));
  memset(&SpleefState, 0, sizeof(SpleefState));

	// Initialize scoreboard
	initializeScoreboard();
	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
	{
		Player * p = players[i];
		SpleefState.PlayerPoints[i] = 0;
	}

	// patch who killed me to prevent damaging others
	*(u32*)0x005E07C8 = 0x0C000000 | ((u32)&whoKilledMeHook >> 2);
	*(u32*)0x005E11B0 = *(u32*)0x005E07C8;

	// initialize state
	SpleefState.GameOver = 0;
	SpleefState.RoundNumber = 0;
	memset(SpleefState.PlayerKills, 0, sizeof(SpleefState.PlayerKills));
	memset(SpleefState.PlayerBoxesDestroyed, 0, sizeof(SpleefState.PlayerBoxesDestroyed));
	resetRoundState();

	Initialized = 1;
}

/*
 * NAME :		gameStart
 * 
 * DESCRIPTION :
 * 			Infected game logic entrypoint.
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
	GameSettings * gameSettings = gameGetSettings();
	Player ** players = playerGetAll();
	Player * localPlayer = (Player*)0x00347AA0;
	GameData * gameData = gameGetData();
	int i;

	// Ensure in game
	if (!gameSettings || !isInGame())
		return;

	// Determine if host
	SpleefState.IsHost = gameIsHost(localPlayer->Guber.Id.GID.HostId);

	if (!Initialized)
		initialize(gameState);

	int killsToWin = gameGetOptions()->GameFlags.MultiplayerGameFlags.KillsToWin;

	// 
	updateGameState(gameState);

#if DEBUG
	if (padGetButton(0, PAD_L3 | PAD_R3) > 0)
		SpleefState.GameOver = 1;
#endif

	if (!gameHasEnded() && !SpleefState.GameOver)
	{
		if (SpleefState.RoundResult[0])
		{
			if (SpleefState.RoundEndTicks)
			{
				// draw round message
				if (SpleefState.RoundResult[1] == localPlayer->PlayerId)
				{
					drawRoundMessage(SPLEEF_ROUND_WIN, 1.5);
				}
				else if (SpleefState.RoundResult[2] == localPlayer->PlayerId)
				{
					drawRoundMessage(SPLEEF_ROUND_SECOND, 1.5);
				}
				else if (SpleefState.RoundResult[3] == localPlayer->PlayerId)
				{
					drawRoundMessage(SPLEEF_ROUND_THIRD, 1.5);
				}
				else
				{
					drawRoundMessage(SPLEEF_ROUND_LOSS, 1.5);
				}

				// handle when round properly ends
				if (gameGetTime() > SpleefState.RoundEndTicks)
				{
					// increment round
					++SpleefState.RoundNumber;

					// reset round state
					resetRoundState();
				}
			}
			else
			{
				// Handle game outcome
				for (i = 1; i < 4; ++i)
				{
					int playerIndex = SpleefState.RoundResult[i];
					if (playerIndex >= 0) {
						SpleefState.PlayerPoints[playerIndex] += 4 - i;
						DPRINTF("player %d score %d\n", playerIndex, SpleefState.PlayerPoints[playerIndex]);
					}
				}

				// set when next round starts
				SpleefState.RoundEndTicks = gameGetTime() + (TIME_SECOND * 5);

				// update winner
				int winningScore = 0;
				getWinningPlayer(&SpleefState.WinningTeam, &winningScore);
				if (killsToWin > 0 && winningScore >= killsToWin)
				{
					SpleefState.GameOver = 1;
				}
			}
		}
		else
		{
			// iterate each player
			for (i = 0; i < GAME_MAX_PLAYERS; ++i)
			{
				SpleefState.PlayerKills[i] = gameData->PlayerStats.Kills[i];
			}

			// host specific logic
			if (SpleefState.IsHost && (gameGetTime() - SpleefState.RoundStartTicks) > (5 * TIME_SECOND))
			{
				int playersAlive = 0, playerCount = 0, lastPlayerAlive = -1;
				for (i = 0; i < GAME_MAX_PLAYERS; ++i)
				{
					if (SpleefState.RoundPlayerState[i] >= 0)
						++playerCount;
					if (SpleefState.RoundPlayerState[i] == 0)
						++playersAlive;
				}

				for (i = 0; i < GAME_MAX_PLAYERS; ++i)
				{
					Player * p = players[i];

					if (p)
					{
						// check if player is dead
						if (playerIsDead(p) || SpleefState.RoundPlayerState[i] == 1)
						{
							// player newly died
							if (SpleefState.RoundPlayerState[i] == 0)
							{
								DPRINTF("player %d died\n", i);
								SpleefState.RoundPlayerState[i] = 1;

								// set player to first/second/third if appropriate
								if (playersAlive < 4)
								{
									SpleefState.RoundResult[playersAlive] = i;
									DPRINTF("setting %d place to player %d\n", playersAlive, i);
								}
							}
						}
						else
						{
							lastPlayerAlive = i;
						}
					}
				}

				if ((playersAlive == 1 && playerCount > 1) || playersAlive == 0)
				{
					// end
					DPRINTF("end round: playersAlive:%d playerCount:%d\n", playersAlive, playerCount);
					if (lastPlayerAlive >= 0)
					{
						SpleefState.RoundResult[1] = lastPlayerAlive;
						DPRINTF("last player alive is %d\n", lastPlayerAlive);
					}
					setRoundOutcome(SpleefState.RoundResult[1], SpleefState.RoundResult[2], SpleefState.RoundResult[3]);
				}
			}
		}
	}
	else
	{
		// set winner
		gameSetWinner(SpleefState.WinningTeam, 0);

		// end game
		if (SpleefState.GameOver == 1)
		{
			gameEnd(4);
			SpleefState.GameOver = 2;
		}
	}

	// 
	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
	{
		Player * p = players[i];
		if (!p)
			continue;

		if (!playerIsDead(p))
			playerSetHealth(p, PLAYER_MAX_HEALTH);
		else
			playerSetHealth(p, 0);
	}

	return;
}

void setLobbyGameOptions(void)
{
	// deathmatch options
	static char options[] = { 
		0, 0, 			// 0x06 - 0x08
		0, 0, 0, 0, 	// 0x08 - 0x0C
		1, 1, 1, 0,  	// 0x0C - 0x10
		0, 1, 0, 0,		// 0x10 - 0x14
		-1, -1, 0, 1,	// 0x14 - 0x18
	};

	// set game options
	GameOptions * gameOptions = gameGetOptions();
	GameSettings* gameSettings = gameGetSettings();
	if (!gameOptions || !gameSettings || gameSettings->GameLoadStartTime <= 0)
		return;
		
	// apply options
	memcpy((void*)&gameOptions->GameFlags.Raw[6], (void*)options, sizeof(options)/sizeof(char));

	gameOptions->GameFlags.MultiplayerGameFlags.Juggernaut = 0;
	gameOptions->GameFlags.MultiplayerGameFlags.Vehicles = 0;
	gameOptions->GameFlags.MultiplayerGameFlags.Puma = 0;
	gameOptions->GameFlags.MultiplayerGameFlags.Hoverbike = 0;
	gameOptions->GameFlags.MultiplayerGameFlags.Landstalker = 0;
	gameOptions->GameFlags.MultiplayerGameFlags.Hovership = 0;
	gameOptions->GameFlags.MultiplayerGameFlags.SpawnWithChargeboots = 0;
	gameOptions->GameFlags.MultiplayerGameFlags.SpecialPickups = 0;
	gameOptions->GameFlags.MultiplayerGameFlags.UnlimitedAmmo = 1;
	gameOptions->GameFlags.MultiplayerGameFlags.Survivor = 1;
	gameOptions->GameFlags.MultiplayerGameFlags.RespawnTime = -1;
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

			setEndGameScoreboard();
			initializedScoreboard = 1;

			// patch rank computation to keep rank unchanged for base mode
			POKE_U32(0x0077ACE4, 0x4600BB06);
			break;
		}
		case UI_ID_GAME_LOBBY:
		{
			setLobbyGameOptions();
			break;
		}
	}
}

/*
 * NAME :		loadStart
 * 
 * DESCRIPTION :
 * 			Load logic entrypoint.
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
	setLobbyGameOptions();
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
