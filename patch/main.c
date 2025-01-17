/***************************************************
 * FILENAME :		main.c
 * 
 * DESCRIPTION :
 * 		Manages and applies all Deadlocked patches.
 * 
 * NOTES :
 * 		Each offset is determined per app id.
 * 		This is to ensure compatibility between versions of Deadlocked/Gladiator.
 * 		
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */

#include <tamtypes.h>

#include <libdl/dl.h>
#include <libdl/player.h>
#include <libdl/pad.h>
#include <libdl/time.h>
#include <libdl/net.h>
#include "module.h"
#include "messageid.h"
#include "config.h"
#include "common.h"
#include "include/config.h"
#include <libdl/game.h>
#include <libdl/string.h>
#include <libdl/collision.h>
#include <libdl/stdio.h>
#include <libdl/gamesettings.h>
#include <libdl/radar.h>
#include <libdl/dialog.h>
#include <libdl/patch.h>
#include <libdl/ui.h>
#include <libdl/graphics.h>
#include <libdl/sha1.h>
#include <libdl/utils.h>
#include <libdl/music.h>
#include <libdl/color.h>

/*
 * Array of game modules.
 */
#define GLOBAL_GAME_MODULES_START			((GameModule*)0x000CF000)

/*
 * Camera speed patch offsets.
 * Each offset is used by PatchCameraSpeed() to change the max
 * camera speed setting in game.
 */
#define CAMERA_SPEED_PATCH_OFF1			(*(u16*)0x00561BB8)
#define CAMERA_SPEED_PATCH_OFF2			(*(u16*)0x00561BDC)

#define UI_POINTERS						((u32*)0x011C7064)

#define ANNOUNCEMENTS_CHECK_PATCH		(*(u32*)0x00621D58)

#define GAMESETTINGS_LOAD_PATCH			(*(u32*)0x0072C3FC)
#define GAMESETTINGS_LOAD_FUNC			(0x0072EF78)
#define GAMESETTINGS_GET_INDEX_FUNC		(0x0070C410)
#define GAMESETTINGS_GET_VALUE_FUNC		(0x0070C538)

#define GAMESETTINGS_BUILD_PTR			(*(u32*)0x004B882C)
#define GAMESETTINGS_BUILD_FUNC			(0x00712BF0)

#define GAMESETTINGS_CREATE_PATCH		(*(u32*)0x0072E5B4)
#define GAMESETTINGS_CREATE_FUNC		(0x0070B540)

#define GAMESETTINGS_RESPAWN_TIME      	(*(char*)0x0017380C)
#define GAMESETTINGS_RESPAWN_TIME2      (*(char*)0x012B3638)

#define GAMESETTINGS_SURVIVOR						(*(u8*)0x00173806)
#define GAMESETTINGS_CRAZYMODE					(*(u8*)0x0017381F)

#define PASSWORD_BUFFER									((char*)0x0017233C)

#define KILL_STEAL_WHO_HIT_ME_PATCH				(*(u32*)0x005E07C8)
#define KILL_STEAL_WHO_HIT_ME_PATCH2			(*(u32*)0x005E11B0)

#define FRAME_SKIP_WRITE0				          (*(u32*)0x004A9400)
#define FRAME_SKIP						            (*(u32*)0x0021E1D8)
#define FRAME_SKIP_TARGET_FPS             (*(u32*)0x0021DF60)

#define VOICE_UPDATE_FUNC				((u32*)0x00161e60)

#define IS_PROGRESSIVE_SCAN					(*(int*)0x0021DE6C)

#define EXCEPTION_DISPLAY_ADDR			(0x000C8000)

#define SHRUB_RENDER_DISTANCE				(*(float*)0x0022308C)

#define DRAW_SHADOW_FUNC						((u32*)0x00587b30)

#define GADGET_EVENT_MAX_TLL				(*(short*)0x005DF5C8)
#define FUSION_SHOT_BACKWARDS_BRANCH 		(*(u32*)0x003FA614)

#define COLOR_CODE_EX1							(0x802020E0)
#define COLOR_CODE_EX2							(0x80E0E040)

#define GAME_UPDATE_SENDRATE				(5 * TIME_SECOND)


#define GAME_SCOREBOARD_ARRAY               ((ScoreboardItem**)0x002FA04C)
#define GAME_SCOREBOARD_ITEM_COUNT          (*(u32*)0x002F9FCC)

#define KOTH_BONUS_POINTS_FACTOR    (0.2)

//#define B6_BALL_SHOT_FIRED_REPLACEMENT (1)

// 
void processSpectate(void);
void spectateSetSpectate(int localPlayerIndex, int playerToSpectateOrDisable);
void runMapLoader(void);
int mapReadCustomMapExtraData(void* dst, int len);
void onMapLoaderOnlineMenu(void);
void onConfigOnlineMenu(void);
void onConfigGameMenu(void);
void onConfigInitialize(void);
void onConfigUpdate(void);
void configMenuEnable(void);
void configMenuDisable(void);
void configTrySendGameConfig(void);
void bannerDraw(void);
void bannerTick(void);
int readLocalGlobalVersion(void);

extern int mapsRemoteGlobalVersion;
extern int mapsLocalGlobalVersion;

void resetFreecam(void);
void processFreecam(void);
void extraLocalsRun(void);
void igScoreboardRun(void);
void quickChatRun(void);

#if SCAVENGER_HUNT
void scavHuntRun(void);
#endif

#if COMP
void runCompMenuLogic(void);
void runCompLogic(void);
#endif

#if TEST
void runTestLogic(void);
#endif

#if MAPEDITOR
void onMapEditorGameUpdate(void);
#endif

// gamerules
void grGameStart(void);
void grLobbyStart(void);
void grLoadStart(void);

// 
int hasInitialized = 0;
int sentGameStart = 0;
int lastMenuInvokedTime = 0;
int lastGameState = 0;
int isInStaging = 0;
int hasSendReachedEndScoreboard = 0;
int hasInstalledExceptionHandler = 0;
int lastSurvivor = 0;
int lastRespawnTime = 5;
int lastCrazyMode = 0;
int lastClientType = -1;
int lastAccountId = -1;
int hasShownSurvivalPrestigeMessage = 0;
int hasPendingLobbyNameOverrides = 0;
char mapOverrideResponse = 1;
char showNeedLatestMapsPopup = 0;
char showNoMapPopup = 0;
char showMiscPopup = 0;
char miscPopupTitle[32];
char miscPopupBody[64];
const char * patchConfigStr = "PATCH CONFIG";
char weaponOrderBackup[GAME_MAX_LOCALS][3] = { {0,0,0}, {0,0,0} };
float lastFps = 0;
int renderTimeMs = 0;
float averageRenderTimeMs = 0;
int updateTimeMs = 0;
float averageUpdateTimeMs = 0;
int redownloadCustomModeBinaries = 0;
ServerSetRanksRequest_t lastSetRanksRequest;
VoteToEndState_t voteToEndState;
int flagRequestCounters[GAME_MAX_PLAYERS] = {0,0,0,0,0,0,0,0,0,0};

int magFlinchCooldown[GAME_MAX_PLAYERS] = {0,0,0,0,0,0,0,0,0,0};
int magDamageCooldown[GAME_MAX_PLAYERS] = {0,0,0,0,0,0,0,0,0,0};
float magDamageCooldownDamage[GAME_MAX_PLAYERS] = {0,0,0,0,0,0,0,0,0,0};

const int allVehiclesEnabledTable[4] = {1,1,1,1};

extern int dlIsActive;

int lastLodLevel = 2;
const int lodPatchesPotato[][2] = {
	{ 0x0050e318, 0x03E00008 }, // disable corn
	{ 0x0050e31c, 0x00000000 },
	{ 0x0041d400, 0x03E00008 }, // disable small weapon explosion
	{ 0x0041d404, 0x00000000 },
	{ 0x003F7154, 0x00000000 }, // disable b6 ball shadow
	{ 0x003A18F0, 0x24020000 }, // disable b6 particles
	{ 0x0042EA50, 0x24020000 }, // disable mag particles
	{ 0x0043C150, 0x24020000 }, // disable mag shells
	{ 0x003A18F0, 0x24020000 }, // disable fusion shot particles
	{ 0x0042608C, 0x00000000 }, // disable jump pad blur effect
};

const int lodPatchesNormal[][2] = {
	{ 0x0050e318, 0x27BDFE40 }, // enable corn
	{ 0x0050e31c, 0x7FB40100 },
	{ 0x0041d400, 0x27BDFF00 }, // enable small weapon explosion
	{ 0x0041d404, 0x7FB00060 },
	{ 0x003F7154, 0x0C140F40 }, // enable b6 ball shadow
	{ 0x003A18F0, 0x0C13DC80 }, // enable b6 particles
	{ 0x0042EA50, 0x0C13DC80 }, // enable mag particles
	{ 0x0043C150, 0x0C13DC80 }, // enable mag shells
	{ 0x003A18F0, 0x0C13DC80 }, // enable fusion shot particles
	{ 0x0042608C, 0x0C131194 }, // enable jump pad blur effect
};


// 
struct GameDataBlock
{
  short Offset;
  short Length;
  char EndOfList;
  u8 Payload[484];
};

extern float _lodScale;
extern void* _correctTieLod;
extern MenuElem_OrderedListData_t dataCustomMaps;
extern MenuElem_OrderedListData_t dataCustomModes;
extern MapLoaderState_t MapLoaderState;

struct FlagPVars
{
	VECTOR BasePosition;
	short CarrierIdx;
	short LastCarrierIdx;
	short Team;
	char UNK_16[6];
	int TimeFlagDropped;
};

typedef struct ChangeTeamRequest {
	u32 Seed;
	int PoolSize;
	char Pool[GAME_MAX_PLAYERS];
} ChangeTeamRequest_t;

typedef struct SetLobbyClientPatchConfigRequest {
	int PlayerId;
	PatchConfig_t Config;
} SetLobbyClientPatchConfigRequest_t;

//
enum PlayerStateConditionType
{
	PLAYERSTATECONDITION_REMOTE_EQUALS,
	PLAYERSTATECONDITION_LOCAL_EQUALS,
	PLAYERSTATECONDITION_LOCAL_OR_REMOTE_EQUALS
};

typedef struct PlayerStateRemoteHistory
{
	int CurrentRemoteState;
	int TimeRemoteStateLastChanged;
	int TimeLastRemoteStateForced;
} PlayerStateRemoteHistory_t;

PlayerStateRemoteHistory_t RemoteStateTimeStart[GAME_MAX_PLAYERS];

typedef struct PlayerStateCondition
{
	enum PlayerStateConditionType Type;
	int TicksSince;
	int StateId;
	int MaxTicks; // number of ticks since start of the remote state before state is ignored
} PlayerStateCondition_t;

//
const PlayerStateCondition_t stateSkipRemoteConditions[] = {
	{	// skip when player is swinging
		.Type = PLAYERSTATECONDITION_LOCAL_OR_REMOTE_EQUALS,
		.TicksSince = 0,
		.StateId = PLAYER_STATE_SWING,
		.MaxTicks = 0
	},
	{	// skip when player is drowning
		.Type = PLAYERSTATECONDITION_LOCAL_OR_REMOTE_EQUALS,
		.TicksSince = 0,
		.StateId = PLAYER_STATE_DROWN,
		.MaxTicks = 0
	},
	{	// skip when player is falling into death void
		.Type = PLAYERSTATECONDITION_LOCAL_OR_REMOTE_EQUALS,
		.TicksSince = 0,
		.StateId = PLAYER_STATE_DEATH_FALL,
		.MaxTicks = 0
	},
};

const PlayerStateCondition_t stateForceRemoteConditions[] = {
	{ // force chargebooting
		.Type = PLAYERSTATECONDITION_LOCAL_OR_REMOTE_EQUALS,
		.TicksSince = 15,
		.StateId = PLAYER_STATE_CHARGE,
		.MaxTicks = 60
	},
	// { // force remote if local is still wrenching
	// 	PLAYERSTATECONDITION_LOCAL_EQUALS,
	// 	15,
	// 	19
	// },
	// { // force remote if local is still hyper striking
	// 	PLAYERSTATECONDITION_LOCAL_EQUALS,
	// 	15,
	// 	20
	// }
};

// 
int isUnloading __attribute__((section(".config"))) = 0;
PatchConfig_t config __attribute__((section(".config"))) = {
	.framelimiter = 2,
	.enableGamemodeAnnouncements = 0,
	.enableSpectate = 0,
	.enableSingleplayerMusic = 0,
	.levelOfDetail = 2,
	.enablePlayerStateSync = 0,
	.disableAimAssist = 0,
	.enableFpsCounter = 0,
	.disableCircleToHackerRay = 0,
	.disableScavengerHunt = 0,
  .playerFov = 0,
  .preferredGameServer = 0,
  .enableSingleTapChargeboot = 0,
  .enableFastLoad = 1
};

// 
PatchConfig_t lobbyPlayerConfigs[GAME_MAX_PLAYERS];
PatchGameConfig_t gameConfig;
PatchGameConfig_t gameConfigHostBackup;
int selectedMapIdHostBackup;
PatchInterop_t interopData = {
  .Config = &config,
  .GameConfig = &gameConfig,
  .Client = CLIENT_TYPE_NORMAL,
  .Month = 0,
  .SetSpectate = spectateSetSpectate,
  .MapLoaderFilename = (char*)MapLoaderState.MapFileName
};

// 
PatchStateContainer_t patchStateContainer = {
  .Config = &config,
  .GameConfig = &gameConfig,
  .ReadExtraDataFunc = mapReadCustomMapExtraData
};

/*
 * NAME :		INetUpdate
 * 
 * DESCRIPTION :
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void INetUpdate(void)
{
  //((void (*)(void))0x01e9e798)();

  // MGCL update
  ((void (*)(void))0x00161280)();
}

/*
 * NAME :		runSingletapChargeboot
 * 
 * DESCRIPTION :
 * 			If enabled, double taps L2 when briefly after L2 is released in game.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void runSingletapChargeboot(struct PAD* pad, u8* rdata)
{
  const int MAX_FRAMES_L2_HELD_FOR_SECOND_TAP = 10;
  static char s_wasL2PressedLastFrame[PAD_PORT_MAX * PAD_SLOT_MAX] = {};
  static int s_framesL2LastHeldFor[PAD_PORT_MAX * PAD_SLOT_MAX] = {};
  static int s_secondTapCountdown[PAD_PORT_MAX * PAD_SLOT_MAX] = {};
  static int s_secondTapForTicks[PAD_PORT_MAX * PAD_SLOT_MAX] = {};
  
  if (!config.enableSingleTapChargeboot) return;

  // get pad idx
  int padIdx = (pad->port*PAD_SLOT_MAX) + pad->slot;
  if (padIdx >= (PAD_PORT_MAX * PAD_SLOT_MAX)) return;
  char wasL2PressedLastFrame = s_wasL2PressedLastFrame[padIdx];
  int framesL2LastHeldFor = s_framesL2LastHeldFor[padIdx];
  int secondTapCountdown = s_secondTapCountdown[padIdx];
  int secondTapForTicks = s_secondTapForTicks[padIdx];

  // check if L2 is pressed this frame
  char isL2PressedThisFrame = (rdata[3] & 1) == 0;

  // count how many frames L2 is held for
  if (isL2PressedThisFrame) {
    wasL2PressedLastFrame = 1;
    framesL2LastHeldFor += 1;
  } else if (wasL2PressedLastFrame) {
    wasL2PressedLastFrame = 0;

    // if L2 was held within singletap threshold, prepare second tap
    if (framesL2LastHeldFor < MAX_FRAMES_L2_HELD_FOR_SECOND_TAP) {
      secondTapCountdown = 2;
      secondTapForTicks = 1;
    }

    framesL2LastHeldFor = 0;
  }

  // stall second tap by countdown
  // then hold L2 for set number of ticks
  if (secondTapCountdown > 0) {
    secondTapCountdown -= 1;

    if (secondTapCountdown == 0) {
      rdata[3] &= ~1;
    }
  } else if (secondTapForTicks > 0) {
    rdata[3] &= ~1;
    secondTapForTicks -= 1;
  }

  // update
  s_wasL2PressedLastFrame[padIdx] = wasL2PressedLastFrame;
  s_framesL2LastHeldFor[padIdx] = framesL2LastHeldFor;
  s_secondTapCountdown[padIdx] = secondTapCountdown;
  s_secondTapForTicks[padIdx] = secondTapForTicks;
}

/*
 * NAME :		patchCameraSpeed
 * 
 * DESCRIPTION :
 * 			Patches in-game camera speed setting to max out at 300%.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void patchCameraSpeed()
{
	const u16 SPEED = 0x140;
	char buffer[16];

	// Check if the value is the default max of 64
	// This is to ensure that we only write here when
	// we're in game and the patch hasn't already been applied
	if (CAMERA_SPEED_PATCH_OFF1 == 0x40)
	{
		CAMERA_SPEED_PATCH_OFF1 = SPEED;
		CAMERA_SPEED_PATCH_OFF2 = SPEED+1;
	}

	// Patch edit profile bar
	if (uiGetActive() == UI_ID_EDIT_PROFILE)
	{
		void * editProfile = (void*)UI_POINTERS[30];
		if (editProfile)
		{
			// get cam speed element
			void * camSpeedElement = (void*)*(u32*)(editProfile + 0xC0);
			if (camSpeedElement)
			{
				// update max value
				*(u32*)(camSpeedElement + 0x78) = SPEED;

				// get current value
				float value = *(u32*)(camSpeedElement + 0x70) / 64.0;

				// render
				sprintf(buffer, "%.0f%%", value*100);
				gfxScreenSpaceText(240,   166,   1, 1, 0x80000000, buffer, -1, 1);
				gfxScreenSpaceText(240-1, 166-1, 1, 1, 0x80FFFFFF, buffer, -1, 1);
			}
		}
	}
}

/*
 * NAME :		patchAnnouncements
 * 
 * DESCRIPTION :
 * 			Patches in-game announcements to work in all gamemodes.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void patchAnnouncements()
{
	u32 addrValue = ANNOUNCEMENTS_CHECK_PATCH;
	if (config.enableGamemodeAnnouncements && addrValue == 0x907E01A9)
		ANNOUNCEMENTS_CHECK_PATCH = 0x241E0000;
	else if (!config.enableGamemodeAnnouncements && addrValue == 0x241E0000)
		ANNOUNCEMENTS_CHECK_PATCH = 0x907E01A9;
}

/*
 * NAME :		patchResurrectWeaponOrdering_HookWeaponStripMe
 * 
 * DESCRIPTION :
 * 			Invoked during the resurrection process, when the game wishes to remove all weapons from the given player.
 * 			Before we continue to remove the player's weapons, we backup the list of equipped weapons.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void patchResurrectWeaponOrdering_HookWeaponStripMe(Player * player)
{
	// backup currently equipped weapons
	if (player->IsLocal) {
		weaponOrderBackup[player->LocalPlayerIndex][0] = playerGetLocalEquipslot(player->LocalPlayerIndex, 0);
		weaponOrderBackup[player->LocalPlayerIndex][1] = playerGetLocalEquipslot(player->LocalPlayerIndex, 1);
		weaponOrderBackup[player->LocalPlayerIndex][2] = playerGetLocalEquipslot(player->LocalPlayerIndex, 2);
	}

	// call hooked WeaponStripMe function after backup
	((void (*)(Player*))0x005e2e68)(player);
}

/*
 * NAME :		patchResurrectWeaponOrdering_HookGiveMeRandomWeapons
 * 
 * DESCRIPTION :
 * 			Invoked during the resurrection process, when the game wishes to give the given player a random set of weapons.
 * 			After the weapons are randomly assigned to the player, we check to see if the given weapons are the same as the last equipped weapon backup.
 * 			If they contain the same list of weapons (regardless of order), then we force the order of the new set of weapons to match the backup.
 * 
 * 			Consider the scenario:
 * 				Player dies with 								Fusion, B6, Magma Cannon
 * 				Player is assigned 							B6, Fusion, Magma Cannon
 * 				Player resurrects with  				Fusion, B6, Magma Cannon
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void patchResurrectWeaponOrdering_HookGiveMeRandomWeapons(Player* player, int weaponCount)
{
	int i, j, matchCount = 0;

	// call hooked GiveMeRandomWeapons function first
	((void (*)(Player*, int))0x005f7510)(player, weaponCount);

	// then try and overwrite given weapon order if weapons match equipped weapons before death
	if (player->IsLocal) {

		// restore backup if they match (regardless of order) newly assigned weapons
		for (i = 0; i < 3; ++i) {
			int backedUpSlotValue = weaponOrderBackup[player->LocalPlayerIndex][i];
			for (j = 0; j < 3; ++j) {
				if (backedUpSlotValue == playerGetLocalEquipslot(player->LocalPlayerIndex, j)) {
					matchCount++;
				}
			}
		}

		// if we found a match, set
		if (matchCount == 3) {
			// set equipped weapon in order
			for (i = 0; i < 3; ++i) {
				playerSetLocalEquipslot(player->LocalPlayerIndex, i, weaponOrderBackup[player->LocalPlayerIndex][i]);
			}

			// equip first slot weapon
			playerEquipWeapon(player, weaponOrderBackup[player->LocalPlayerIndex][0]);
		}
	}
}

/*
 * NAME :		patchResurrectWeaponOrdering
 * 
 * DESCRIPTION :
 * 			Installs necessary hooks such that when respawning with same weapons,
 * 			they are equipped in the same order.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void patchResurrectWeaponOrdering(void)
{
	if (!isInGame())
		return;

	HOOK_JAL(0x005e2b2c, &patchResurrectWeaponOrdering_HookWeaponStripMe);
	HOOK_JAL(0x005e2b48, &patchResurrectWeaponOrdering_HookGiveMeRandomWeapons);
}

/*
 * NAME :		getMACAddress
 * 
 * DESCRIPTION :
 * 			    Returns non-zero on success.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
int getMACAddress(u8 output[6])
{
  int i;
  static int hasMACAddress = 0;
  static u8 macAddress[6];
  void* cd = (void*)0x001B26C0;
  void* net_buf = (void*)0x001B2700;
  u8 buf[16];

  // use cached result
  // since the MAC address isn't going to change in the lifetime of the patch
  if (hasMACAddress)
  {
    memcpy(output, macAddress, 6);
    return 1;
  }

  // sceInetInterfaceControl get physical address
  int r = ((int (*)(void* cd, void* net_buf, int interface_id, int code, void* ptr, int len))0x01E9BE70)(cd, net_buf, 1, 13, buf, sizeof(buf));
  if (r)
  {
    memset(output, 0, 6);
    return 0;
  }

  //
  memcpy(macAddress, buf, 6);
  memcpy(output, buf, 6);
  DPRINTF("mac %02X:%02X:%02X:%02X:%02X:%02X\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
  return hasMACAddress = 1;
}

/*
 * NAME :		hasSonyMACAddress
 * 
 * DESCRIPTION :
 * 			    Returns non-zero if the client's MAC address is a known sony mac address.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
int hasSonyMACAddress(void)
{
  int i;
  static int hasSonyMACAddressResult = -1;
  void* cd = (void*)0x001B26C0;
  void* net_buf = (void*)0x001B2700;
  u8 mac[6];

  // use cached result
  // since the MAC address isn't going to change in the lifetime of the patch
  if (hasSonyMACAddressResult >= 0) return hasSonyMACAddressResult;

  // if we can't get the mac address, then assume we're on a PS2
  // but don't save result so that we run again
  if (!getMACAddress(mac)) return 1;
  DPRINTF("mac %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  // latest PCSX2 uses fixed 4 bytes followed by 2 bytes generated from the host net adapter
  // we'll assume that any client matching the first 4 bytes are on pcsx2
  // could have false positives
  u32 firstWordMacAddress = *(u32*)mac;
  if (firstWordMacAddress == 0x821F0400)
    return hasSonyMACAddressResult = 0;

  // check if the first half of our mac address
  // matches any known sony mac addresses
  u32 firstHalfMacAddress = (mac[0] << 16) | (mac[1] << 8) | (mac[2] << 0);
  for (i = 0; i < SONY_MAC_ADDRESSES_COUNT; ++i) {
    if (SONY_MAC_ADDRESSES[i] == firstHalfMacAddress) return hasSonyMACAddressResult = 1;
  }

  return hasSonyMACAddressResult = 0;
}

/*
 * NAME :		patchLevelOfDetail
 * 
 * DESCRIPTION :
 * 			Sets the level of detail.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void patchLevelOfDetail(void)
{
	int i = 0;

	// only apply in game
	if (!isInGame()) {
		lastLodLevel = -1;
		return;
	}

	// patch lod
	if (*(u32*)0x005930B8 == 0x02C3B020)
	{
		*(u32*)0x005930B8 = 0x08000000 | ((u32)&_correctTieLod >> 2);
	}

  if (lastClientType != CLIENT_TYPE_DZO) {

    // force lod on certain maps
    int lod = config.levelOfDetail;
    if (patchStateContainer.SelectedCustomMapId && strcmp(customMapDefs[patchStateContainer.SelectedCustomMapId-1].Filename, "canal city") == 0) {
      lod = 0; // always potato on canal city
    }

    // use custom map min shrub render distance
    short minShrubRenderDist = 0;
    if (patchStateContainer.SelectedCustomMapId) {
      minShrubRenderDist = customMapDefs[patchStateContainer.SelectedCustomMapId-1].ShrubMinRenderDistance;
    }

    // correct lod
    int lodChanged = lod != lastLodLevel;
    switch (lod)
    {
      case 0: // potato
      {
        _lodScale = 0.2;
        SHRUB_RENDER_DISTANCE = maxf(minShrubRenderDist, 50);
        *DRAW_SHADOW_FUNC = 0x03E00008;
        *(DRAW_SHADOW_FUNC + 1) = 0;

        if (lodChanged)
        {
          // set terrain and tie render distance
          POKE_U16(0x00223158, 120);
          *(float*)0x002230F0 = 120 * 1024;

          for (i = 0; i < sizeof(lodPatchesPotato) / (2 * sizeof(int)); ++i)
            POKE_U32(lodPatchesPotato[i][0], lodPatchesPotato[i][1]);
        }
        break;
      }
      case 1: // low
      {
        _lodScale = 0.2;
        SHRUB_RENDER_DISTANCE = maxf(minShrubRenderDist, 100);
        *DRAW_SHADOW_FUNC = 0x03E00008;
        *(DRAW_SHADOW_FUNC + 1) = 0;

        if (lodChanged)
        {
          // set terrain and tie render distance
          POKE_U16(0x00223158, 480);
          *(float*)0x002230F0 = 480 * 1024;

          for (i = 0; i < sizeof(lodPatchesNormal) / (2 * sizeof(int)); ++i)
            POKE_U32(lodPatchesNormal[i][0], lodPatchesNormal[i][1]);

          // disable jump pad blur
          POKE_U32(0x0042608C, 0);
        }
        break;
      }
      case 2: // normal
      {
        _lodScale = 1.0;
        SHRUB_RENDER_DISTANCE = maxf(minShrubRenderDist, 500);
        *DRAW_SHADOW_FUNC = 0x27BDFF90;
        *(DRAW_SHADOW_FUNC + 1) = 0xFFB30038;
        POKE_U16(0x00223158, 960);
        *(float*)0x002230F0 = 960 * 1024;

        if (lodChanged)
        {
          // set terrain and tie render distance
          POKE_U16(0x00223158, 960);
          *(float*)0x002230F0 = 960 * 1024;

          for (i = 0; i < sizeof(lodPatchesNormal) / (2 * sizeof(int)); ++i)
            POKE_U32(lodPatchesNormal[i][0], lodPatchesNormal[i][1]);
        }
        break;
      }
      case 3: // high
      {
        _lodScale = 10.0;
        SHRUB_RENDER_DISTANCE = maxf(minShrubRenderDist, 5000);
        *DRAW_SHADOW_FUNC = 0x27BDFF90;
        *(DRAW_SHADOW_FUNC + 1) = 0xFFB30038;

        for (i = 0; i < sizeof(lodPatchesNormal) / (2 * sizeof(int)); ++i)
          POKE_U32(lodPatchesNormal[i][0], lodPatchesNormal[i][1]);
        break;
      }
    }
  }

	// backup lod
	lastLodLevel = config.levelOfDetail;
}

/*
 * NAME :		patchCameraShake
 * 
 * DESCRIPTION :
 * 			Disables camera shake.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void patchCameraShake(void)
{
	if (!isInGame())
		return;

	if (config.disableCameraShake)
	{
		POKE_U32(0x004b14a0, 0x03E00008);
		POKE_U32(0x004b14a4, 0);
	}
	else
	{
		POKE_U32(0x004b14a0, 0x34030470);
		POKE_U32(0x004b14a4, 0x27BDFF70);
	}
}

/*
 * NAME :		patchGameSettingsLoad_Save
 * 
 * DESCRIPTION :
 * 			Saves the given value to the respective game setting.
 * 			I think this does validation depending on the input type then saves.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 			a0:					Points to the general settings area
 * 			offset0:			Offset to game setting
 * 			offset1:			Offset to game setting input type handler?
 * 			value:				Value to save
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
int patchGameSettings_OpenPasswordInputDialog(void * a0, char * title, char * value, int a3, int maxLength, int t1, int t2, int t3, int t4)
{
	strncpy((char*)value, PASSWORD_BUFFER, maxLength);

	return internal_uiInputDialog(a0, title, value, a3, maxLength, t1, t2, t3, t4);
}

/*
 * NAME :		patchGameSettingsLoad_Save
 * 
 * DESCRIPTION :
 * 			Saves the given value to the respective game setting.
 * 			I think this does validation depending on the input type then saves.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 			a0:					Points to the general settings area
 * 			offset0:			Offset to game setting
 * 			offset1:			Offset to game setting input type handler?
 * 			value:				Value to save
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void patchGameSettingsLoad_Save(void * a0, int offset0, int offset1, int value)
{
	u32 step1 = *(u32*)((u32)a0 + offset0);
	u32 step2 = *(u32*)(step1 + 0x58);
	u32 step3 = *(u32*)(step2 + offset1);
	((void (*)(u32, int))step3)(step1, value);
}

/*
 * NAME :		patchGameSettingsLoad_Hook
 * 
 * DESCRIPTION :
 * 			Called when loading previous game settings into create game.
 * 			Reloads Survivor correctly.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void patchGameSettingsLoad_Hook(void * a0, void * a1)
{
	int index = 0;

	// Load normal
	((void (*)(void *, void *))GAMESETTINGS_LOAD_FUNC)(a0, a1);

	// Get gametype
	index = ((int (*)(int))GAMESETTINGS_GET_INDEX_FUNC)(5);
	int gamemode = ((int (*)(void *, int))GAMESETTINGS_GET_VALUE_FUNC)(a1, index);

	// Handle each gamemode separately
	switch (gamemode)
	{
		case GAMERULE_DM:
		{
			// Save survivor
			patchGameSettingsLoad_Save(a0, 0x100, 0xA4, lastSurvivor);
			break;
		}
		case GAMERULE_CTF:
		{
			// Save crazy mode
			patchGameSettingsLoad_Save(a0, 0x10C, 0xA4, !lastCrazyMode);
			break;
		}
	}

	// enable pw input prompt when password is true
	POKE_U32(0x0072E4A0, 0x0000182D);
	HOOK_JAL(0x0072e510, &patchGameSettings_OpenPasswordInputDialog);
	
	// allow user to enable/disable password
	POKE_U32(0x0072C408, 0x0000282D);
	

	// password
	if (strlen(PASSWORD_BUFFER) > 0) {

		// set default to on
		patchGameSettingsLoad_Save(a0, 0xC4, 0xA4, 1);
		DPRINTF("pw %s\n", PASSWORD_BUFFER);
	}

	if (gamemode != GAMERULE_CQ)
	{
		// respawn timer
		GAMESETTINGS_RESPAWN_TIME2 = lastRespawnTime; //*(u8*)0x002126DC;
	}

	if (GAMESETTINGS_RESPAWN_TIME2 < 0)
		GAMESETTINGS_RESPAWN_TIME2 = lastRespawnTime;
}

/*
 * NAME :		patchGameSettingsLoad
 * 
 * DESCRIPTION :
 * 			Patches game settings load so it reloads Survivor correctly.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void patchGameSettingsLoad()
{
	if (GAMESETTINGS_LOAD_PATCH == 0x0C1CBBDE)
	{
		GAMESETTINGS_LOAD_PATCH = 0x0C000000 | ((u32)&patchGameSettingsLoad_Hook >> 2);
	}
}

/*
 * NAME :		patchPopulateCreateGame_Hook
 * 
 * DESCRIPTION :
 * 			Patches create game populate setting to add respawn timer.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void patchPopulateCreateGame_Hook(void * a0, int settingsCount, u32 * settingsPtrs)
{
	u32 respawnTimerPtr = 0x012B35D8;
	int i = 0;

	// Check if already loaded
	for (; i < settingsCount; ++i)
	{
		if (settingsPtrs[i] == respawnTimerPtr)
			break;
	}

	// If not loaded then append respawn timer
	if (i == settingsCount)
	{
		++settingsCount;
		settingsPtrs[i] = respawnTimerPtr;
	}

	// Populate
	((void (*)(void *, int, u32 *))GAMESETTINGS_BUILD_FUNC)(a0, settingsCount, settingsPtrs);
}

/*
 * NAME :		patchPopulateCreateGame
 * 
 * DESCRIPTION :
 * 			Patches create game populate setting to add respawn timer.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void patchPopulateCreateGame()
{
	// Patch function pointer
	if (GAMESETTINGS_BUILD_PTR == GAMESETTINGS_BUILD_FUNC)
	{
		GAMESETTINGS_BUILD_PTR = (u32)&patchPopulateCreateGame_Hook;
	}

	// Patch default respawn timer
	GAMESETTINGS_RESPAWN_TIME = lastRespawnTime;
}

/*
 * NAME :		patchCreateGame_Hook
 * 
 * DESCRIPTION :
 * 			Patches create game save settings to save respawn timer.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
u64 patchCreateGame_Hook(void * a0)
{
	// Save respawn timer if not survivor
	lastSurvivor = GAMESETTINGS_SURVIVOR;
	lastRespawnTime = GAMESETTINGS_RESPAWN_TIME2;
	lastCrazyMode = GAMESETTINGS_CRAZYMODE;

	// Load normal
	return ((u64 (*)(void *))GAMESETTINGS_CREATE_FUNC)(a0);
}

/*
 * NAME :		patchCreateGame
 * 
 * DESCRIPTION :
 * 			Patches create game save settings to save respawn timer.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void patchCreateGame()
{
	// Patch function pointer
	if (GAMESETTINGS_CREATE_PATCH == 0x0C1C2D50)
	{
		GAMESETTINGS_CREATE_PATCH = 0x0C000000 | ((u32)&patchCreateGame_Hook >> 2);
	}
}

/*
 * NAME :		getMapVehiclesEnabledTable
 * 
 * DESCRIPTION :
 * 			
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
int* getMapVehiclesEnabledTable(int mapId)
{
  return allVehiclesEnabledTable;
}

/*
 * NAME :		patchWideStats_Hook
 * 
 * DESCRIPTION :
 * 			Copies over game mode ranks into respective slot in medius stats.
 * 			This just ensures that the ranks in the database always match up with the rank appearing in game.
 * 			Since, after login, the game tracks its own copies of ranks regardless of the ranks stored in the accounts wide stats.
 * 			This causes problems with custom gamemodes, which ignore rank changes as they are handled separately by the server.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void patchWideStats_Hook(void)
{
	int* mediusStats = (int*)0x001722C0;
	int* stats = *(int**)0x0017277c;

	if (stats) {
		// store each respective gamemode rank in player medius stats
		mediusStats[1] = stats[17]; // CQ
		mediusStats[2] = stats[24]; // CTF
		mediusStats[3] = stats[11]; // DM
		mediusStats[4] = stats[31]; // KOTH
		mediusStats[5] = stats[38]; // JUGGY
	}
}

/*
 * NAME :		patchWideStats
 * 
 * DESCRIPTION :
 * 			Writes hook into the callback of MediusGetLadderStatsWide.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void patchWideStats(void)
{
	*(u32*)0x0015C0EC = 0x08000000 | ((u32)&patchWideStats_Hook >> 2);
}

/*
 * NAME :		getPlayerScore
 * 
 * DESCRIPTION :
 * 			Returns game points for the given player idx.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
int getPlayerScore(int playerIdx)
{
  if (!isInGame()) return 0;
  return ((int (*)(int))0x00625510)(playerIdx);
}

/*
 * NAME :		patchComputePoints_Hook
 * 
 * DESCRIPTION :
 * 			The game uses one function to compute points for each player at the end of the game.
 *      These points are used to determine MVP, assign more/less MMR, etc.
 *      For competitive reasons, 
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
int patchComputePoints_Hook(int playerIdx)
{
  GameData* gameData = gameGetData();
  GameSettings* gameSettings = gameGetSettings();
  int basePoints = getPlayerScore(playerIdx);
  if (!gameData) return basePoints;

  int newPoints = basePoints;
  int i;

  if (gameData && gameSettings && !gameConfig.customModeId) {

    switch (gameSettings->GameRules)
    {
      case GAMERULE_CTF:
      {
        // set player points
        // equal to number of caps for team
        int team = gameSettings->PlayerTeams[playerIdx];
        newPoints = gameData->TeamStats.FlagCaptureCounts[team];
        break;
      }
      // case GAMERULE_KOTH:
      // {
      //   // find best score
      //   int bestScore = 0;
      //   for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
      //     int score = getPlayerScore(i);
      //     if (score > bestScore)
      //       bestScore = score;
      //   }

      //   // award winning team fraction of best score as a bonus
      //   newPoints += (winningTeam == gameSettings->PlayerTeams[playerIdx]) ? (bestScore * KOTH_BONUS_POINTS_FACTOR) : 0;

      //   break;
      // }
    }

  }

  if (gameData && gameData->GameIsOver) {
    //DPRINTF("points for %d => base:%d ours:%d\n", playerIdx, basePoints, newPoints);
  }

  return newPoints;
}

/*
 * NAME :		patchComputePoints
 * 
 * DESCRIPTION :
 * 			Hooks all calls to ComputePoints.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void patchComputePoints(void)
{
  // write RANK_SPREAD so that a 10 is possible
  *(float*)0x001737B0 = 10000.0;

  if (!isInGame())
    return;

  HOOK_JAL(0x006233f0, &patchComputePoints_Hook);
  HOOK_JAL(0x006265a4, &patchComputePoints_Hook);
}

/*
 * NAME :		patchKillStealing_Hook
 * 
 * DESCRIPTION :
 * 			Filters out hits when player is already dead.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
int patchKillStealing_Hook(Player * target, Moby * damageSource, u64 a2)
{
	// if player is already dead return 0
	if (target->Health <= 0)
		return 0;

	// pass through
	return ((int (*)(Player*,Moby*,u64))0x005DFF08)(target, damageSource, a2);
}

/*
 * NAME :		patchKillStealing
 * 
 * DESCRIPTION :
 * 			Patches who hit me on weapon hit with patchKillStealing_Hook.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void patchKillStealing()
{
	// 
	if (KILL_STEAL_WHO_HIT_ME_PATCH == 0x0C177FC2)
	{
		KILL_STEAL_WHO_HIT_ME_PATCH = 0x0C000000 | ((u32)&patchKillStealing_Hook >> 2);
		KILL_STEAL_WHO_HIT_ME_PATCH2 = KILL_STEAL_WHO_HIT_ME_PATCH;
	}
}

/*
 * NAME :		patchAggTime
 * 
 * DESCRIPTION :
 * 			Sets agg time to user configured value.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void patchAggTime(int aggTimeMs)
{
	u16* currentAggTime = (u16*)0x0015ac04;

	// only update on change
	if (*currentAggTime == aggTimeMs)
		return;

	*currentAggTime = aggTimeMs;
	void * connection = netGetDmeServerConnection();
	if (connection)
		netSetSendAggregationInterval(connection, 0, aggTimeMs);
}

/*
 * NAME :		writeFov
 * 
 * DESCRIPTION :
 * 			Replaces game's SetFov function. Hook installed by patchFov().
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void writeFov(int cameraIdx, int a1, int a2, u32 ra, float fov, float f13, float f14, float f15)
{
  static float lastFov = 0;

  GameCamera* camera = cameraGetGameCamera(cameraIdx);
  if (!camera)
    return;
  
  // save last fov
  // or reuse last if fov passed is 0
  if (fov > 0)
    lastFov = fov;
  else if (lastFov > 0)
    fov = lastFov;
  else
    fov = lastFov = camera->fov.ideal;

  // apply our fov modifier
  // only if not scoping with sniper
  if (ra != 0x003FB4E4) fov += (config.playerFov / 10.0) * 1;

  if (a2 > 2) {
    if (a2 != 3) return;
    camera->fov.limit = f15;
    camera->fov.changeType = a2;
    camera->fov.ideal = fov;
    camera->fov.state = 1;
    camera->fov.gain = f13;
    camera->fov.damp = f14;
    return;
  }
  else if (a2 < 1) {
    if (a2 != 0) return;
    camera->fov.ideal = fov;
    camera->fov.changeType = 0;
    camera->fov.state = 1;
    return;
  }

  if (a1 == 0) {
    camera->fov.ideal = fov;
    camera->fov.changeType = 0;
  }
  else {
    camera->fov.changeType = a2;
    camera->fov.init = camera->fov.actual;
    camera->fov.timer = (short)a2;
    camera->fov.timerInv = 1.0 / (float)a2;
  }

  camera->fov.state = 1;
}

/*
 * NAME :		initFovHook
 * 
 * DESCRIPTION :
 * 			Called when FOV is initialized.
 *      Ensures custom FOV is applied on initialization.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void initFovHook(int cameraIdx)
{
  // call base
  ((void (*)(int))0x004ae9e0)(cameraIdx);

  GameCamera* camera = cameraGetGameCamera(cameraIdx);
  writeFov(0, 0, 3, 0, camera->fov.ideal, 0.05, 0.2, 0);
}

/*
 * NAME :		patchFov
 * 
 * DESCRIPTION :
 * 			Installs SetFov override hook.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void patchFov(void)
{
  static int ingame = 0;
  static int lastFov = 0;
	if (!isInGame()) {
		ingame = 0;
    return;
  }

  // replace SetFov function
  HOOK_J(0x004AEA90, &writeFov);
  POKE_U32(0x004AEA94, 0x03E0382D);
  
  HOOK_JAL(0x004B25C0, &initFovHook);

  // initialize fov at start of game
  if (!ingame || lastFov != config.playerFov) {
    GameCamera* camera = cameraGetGameCamera(0);
    if (!camera)
      return;

    writeFov(0, 0, 3, 0, 0, 0.05, 0.2, 0);
    lastFov = config.playerFov;
    ingame = 1;
  }
}

/*
 * NAME :		patchFrameSkip
 * 
 * DESCRIPTION :
 * 			Patches frame skip to always be off.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void patchFrameSkip()
{
	static int disableByAuto = 0;
	static int autoDisableDelayTicks = 0;
	
	int addrValue = FRAME_SKIP_WRITE0;
  int emuClient = CLIENT_TYPE_DZO == interopData.Client || interopData.Client == CLIENT_TYPE_PCSX2; // force framelimiter off for dzo/emu
	int disableFramelimiter = config.framelimiter == 2 || emuClient;
	int totalTimeMs = renderTimeMs + updateTimeMs;
	float averageTotalTimeMs = averageRenderTimeMs + averageUpdateTimeMs; 

  GameSettings* gs = gameGetSettings();
  int intelligentFps = 60;
  if (gs && gs->PlayerCount > 6) intelligentFps = 30;

	if (!emuClient && config.framelimiter == 1) // auto
	{
		// already disabled, to re-enable must have instantaneous high total time
		if (disableByAuto && totalTimeMs > 15.0) {
			autoDisableDelayTicks = 30; // 1 seconds before can disable again
			disableByAuto = 0; 
		} else if (disableByAuto && averageTotalTimeMs > 14.0) {
			autoDisableDelayTicks = 60; // 2 seconds before can disable again
			disableByAuto = 0; 
		}

		// not disabled, to disable must have average low total time
		// and must not have just enabled it
		else if (autoDisableDelayTicks == 0 && !disableByAuto && averageTotalTimeMs < 12.5)
			disableByAuto = 1;

		// decrement disable delay
		if (autoDisableDelayTicks > 0)
			--autoDisableDelayTicks;

		// set framelimiter
		disableFramelimiter = disableByAuto;
	}

	// re-enable framelimiter if last fps is really low
	if (disableFramelimiter && addrValue == 0xAF848859)
	{
		FRAME_SKIP_WRITE0 = 0;
		FRAME_SKIP = 0;
    FRAME_SKIP_TARGET_FPS = 60;
	}
	else if (!disableFramelimiter && addrValue == 0)
	{
		FRAME_SKIP_WRITE0 = 0xAF848859;
    FRAME_SKIP_TARGET_FPS = intelligentFps;
	}
}

/*
 * NAME :		patchAimAssist
 * 
 * DESCRIPTION :
 * 			Enables/disables aim assist
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void patchAimAssist(void)
{
  static int lastValue = 0;
  if (!isInGame()) { lastValue = 0; return; }
  if (lastValue == config.disableAimAssist) return;

  if (config.disableAimAssist) {
    POKE_U32(0x004DC294, 0x10000015);
  } else {
    POKE_U32(0x004DC294, 0x45010015);
  }

  lastValue = config.disableAimAssist;
}

/*
 * NAME :		handleWeaponShotDelayed
 * 
 * DESCRIPTION :
 * 			Forces all incoming weapon shot events to happen immediately.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void handleWeaponShotDelayed(Player* player, char event, int dispatchTime, short gadgetId, char gadgetIdx, struct tNW_GadgetEventMessage * message)
{
  const int MAX_DELAY = TIME_SECOND * 0;

  int startTime = dispatchTime;

  // put clamp on max delay
  if (message) {

#if B6_BALL_SHOT_FIRED_REPLACEMENT
    if (event == 8 && gadgetId == WEAPON_ID_B6) {
      return;
    }
#endif

    int delta = dispatchTime - gameGetTime();
    if (delta > MAX_DELAY) {
      dispatchTime = gameGetTime() + MAX_DELAY;
      if (message) message->ActiveTime = dispatchTime;
    } else if (delta < 0) {
      dispatchTime = gameGetTime() - 1;
      if (message) message->ActiveTime = dispatchTime;
    }
  } else if (dispatchTime < 0) {
    dispatchTime = gameGetTime() - TIME_SECOND;
  }

  // if (gadgetId == 5 || (gadgetId == -1 && player->Gadgets[gadgetIdx].id == 5)) {
  //   DPRINTF("GADGET %d EVENT %d TIME %d=>%d (%d)\n", gadgetId, event, startTime, dispatchTime, gameGetTime());
  // }
  
	((void (*)(Player*, char, int, short, char, struct tNW_GadgetEventMessage*))0x005f0318)(player, event, dispatchTime, gadgetId, gadgetIdx, message);
}

/*
 * NAME :		onB6BallSpawned
 * 
 * DESCRIPTION :
 * 			Hooks B6 ball spawn. Sends B6 fired message.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
Moby* onB6BallSpawned(void)
{
  Moby* moby;

	// pointer to b6 ball moby is stored in $v0
	asm volatile (
		"move %0, $v0"
		: : "r" (moby)
	);

  if (!moby) return NULL;
  Player* sourcePlayer = *(Player**)(moby->PVar + 0x90);
  if (!sourcePlayer) return moby;
  if (!sourcePlayer->IsLocal) return moby;

  FireB6Ball_t msg;
  msg.FromPlayerId = sourcePlayer->PlayerId;
  msg.Level = sourcePlayer->GadgetBox->Gadgets[WEAPON_ID_B6].Level;
  memcpy(msg.Position, moby->Position, 12);
  memcpy(msg.Velocity, moby->PVar + 0x40, 12);

  void* connection = netGetDmeServerConnection();
  if (connection) {
    netSendCustomAppMessage(NET_DELIVERY_CRITICAL, connection, -1, CUSTOM_MSG_B6_BALL_FIRED, sizeof(msg), &msg);
  }

  return moby;
}

/*
 * NAME :		onB6BallFiredRemote
 * 
 * DESCRIPTION :
 * 			Handles incoming B6 fired event.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
int onB6BallFiredRemote(void* connection, void* data)
{
  FireB6Ball_t msg;
  memcpy(&msg, data, sizeof(msg));

  if (!isInGame()) return sizeof(msg);

  DPRINTF("RECV B6 FIRED FROM %d\n", msg.FromPlayerId);

  Player* player = playerGetAll()[msg.FromPlayerId];
  if (!player || !player->PlayerMoby) return sizeof(msg);
  if (player->Gadgets[0].id != WEAPON_ID_B6) {
    DPRINTF("player does not have B6 equipped\n");
    return sizeof(msg);
  }

  Moby* b6Moby = player->Gadgets[0].pMoby;
  if (!b6Moby || b6Moby->OClass != MOBY_ID_B6_OBLITERATOR) return sizeof(msg);

  // ensure gadget level matches source player
  if (player->GadgetBox->Gadgets[WEAPON_ID_B6].Level != msg.Level) {
    player->GadgetBox->Gadgets[WEAPON_ID_B6].Level = msg.Level;
    if (b6Moby) weaponMobyUpdateBangles(b6Moby, WEAPON_ID_B6, msg.Level);
  }

  // player shot recoil animation
  ((void (*)(Player*, int))0x005dcdd8)(player, -1);

  // b6 moby shot fired animation
  mobyAnimTransition(b6Moby, 2, 2, 0);

  // play sound
  mobyPlaySound(0, 0, b6Moby);

  // fire b6 bomb
  VECTOR pos, vel;
  memcpy(pos, msg.Position, 12);
  memcpy(vel, msg.Velocity, 12);
  Moby* ball = ((Moby* (*)(Moby* gadget, VECTOR pos, VECTOR vel, int, Moby* heroMoby, Player* hero, int dispatchTime, int))0x003f5f80)
    (player->Gadgets[0].pMoby, pos, vel, 0, player->PlayerMoby, player, gameGetTime(), 0);

	return sizeof(msg);
}

/*
 * NAME :		getFusionShotDirection
 * 
 * DESCRIPTION :
 * 			Ensures that the fusion shot will hit its target.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
u128 getFusionShotDirection(Player* target, int a1, u128 vFrom, u128 vTo, Moby* targetMoby)
{
  VECTOR from, to;

	// move from registers to memory
  vector_write(from, vFrom);
  vector_write(to, vTo);

  // if we have a valid player target
	// then we want to check that the given to from will hit
	// if it doesn't then we want to force it to hit
  if (target)
  {
    // if the shot didn't hit the target then we want to force the shot to hit the target
    int hit = CollLine_Fix(from, to, 2, NULL, 0);
    if (!hit || CollLine_Fix_GetHitMoby() != target->PlayerMoby) {
      vTo = ((u128 (*)(Moby*, Player*))0x003f7968)(targetMoby, target);
    }
  }

	// call base get direction
  return ((u128 (*)(Player*, int, u128, u128, float))0x003f9fc0)(target, a1, vFrom, vTo, 0.0);
}

/*
 * NAME :		patchFusionReticule
 * 
 * DESCRIPTION :
 * 			Enables/disables the fusion reticule.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void patchFusionReticule(void)
{
  static int patched = -1;
  if (!isInGame()) {
    patched = -1; // reset
    return;
  }

  // don't allow reticule when disabled by host
  if (gameConfig.grNoSniperHelpers)
    return;

  // already patched
  if (patched == config.enableFusionReticule)
    return;

  if (config.enableFusionReticule) {
    POKE_U32(0x003FAFA4, 0x8203266D);
    POKE_U32(0x003FAFA8, 0x1060003E);

    POKE_U32(0x003FAEE0, 0);
  } else {
    POKE_U32(0x003FAFA4, 0x24020001);
    POKE_U32(0x003FAFA8, 0x1062003E);

    POKE_U32(0x003FAEE0, 0x10400070);
  }

  patched = config.enableFusionReticule;
}

/*
 * NAME :		patchCycleOrder
 * 
 * DESCRIPTION :
 * 			Forces the configured cycle order when applicable.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void patchCycleOrder(void)
{
  int i;
  int order[3];

  if (!isInGame()) return;
  if (config.fixedCycleOrder == FIXED_CYCLE_ORDER_OFF) return;

  for (i = 0; i < GAME_MAX_LOCALS; ++i) {

    // check we're holding the three cycle weapons
    int w0 = playerGetLocalEquipslot(i, 0);
    int w1 = playerGetLocalEquipslot(i, 1);
    int w2 = playerGetLocalEquipslot(i, 2);
    int mask = (1 << w0) | (1 << w1) | (1 << w2);
    int cycleMask = (1 << WEAPON_ID_MAGMA_CANNON) | (1 << WEAPON_ID_FUSION_RIFLE) | (1 << WEAPON_ID_B6);
    if (cycleMask != mask) return;

    // get order
    switch (config.fixedCycleOrder)
    {
      case FIXED_CYCLE_ORDER_MAG_FUS_B6:
      {
        order[0] = WEAPON_ID_MAGMA_CANNON;
        order[1] = WEAPON_ID_FUSION_RIFLE;
        order[2] = WEAPON_ID_B6;
        break;
      }
      case FIXED_CYCLE_ORDER_MAG_B6_FUS:
      {
        order[0] = WEAPON_ID_MAGMA_CANNON;
        order[1] = WEAPON_ID_B6;
        order[2] = WEAPON_ID_FUSION_RIFLE;
        break;
      }
      default: return;
    }

    // determine where in order we are based on first equipped weapon
    int index = 0;
    while (order[index] != w0 && index < 2)
      ++index;
    
    // update other two weapons to match order if not already
    int e1 = order[(index + 1) % 3];
    int e2 = order[(index + 2) % 3];
    if (w1 != e1) playerSetLocalEquipslot(i, 1, e1);
    if (w2 != e2) playerSetLocalEquipslot(i, 2, e2);
  }
}

/*
 * NAME :		flagHandlePickup
 * 
 * DESCRIPTION :
 * 			Gives flag to player or returns flag by player.
 *      Does some additional error checking to prevent multiple players
 *      from interacting with the flag at the same time.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void flagHandlePickup(Moby* flagMoby, int pIdx)
{
  Player* player = playerGetAll()[pIdx];
  if (!player || !flagMoby) return;
	struct FlagPVars* pvars = (struct FlagPVars*)flagMoby->PVar;
  if (!pvars) return;

	if (flagMoby->State != 1)
		return;

	// flag is currently returning
	if (flagIsReturning(flagMoby))
		return;

	// flag is currently being picked up
	if (flagIsBeingPickedUp(flagMoby))
		return;
	
  // only allow actions by living players
  if (playerIsDead(player) || player->Health <= 0)
    return;

  if (player->Team == pvars->Team) {
    if (!flagIsAtBase(flagMoby)) flagReturnToBase(flagMoby, 0, pIdx);
  } else {
    flagPickup(flagMoby, pIdx);
    player->HeldMoby = flagMoby;
  }

  DPRINTF("player %d picked up flag %X at %d\n", player->PlayerId, flagMoby->OClass, gameGetTime());
}

/*
 * NAME :		flagRequestPickup
 * 
 * DESCRIPTION :
 * 			Requests to either pickup or return the given flag.
 *      If host, this request is automatically passed to the handler.
 *      If not host, this request is sent over the net to the host.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void flagRequestPickup(Moby* flagMoby, int pIdx)
{
  
  int gameTime = gameGetTime();
  Player* player = playerGetAll()[pIdx];
  if (!player || !flagMoby) return;
	struct FlagPVars* pvars = (struct FlagPVars*)flagMoby->PVar;
  if (!pvars) return;

  if (gameAmIHost())
  {
    // handle locally
    flagHandlePickup(flagMoby, pIdx);
  }
  else if (flagRequestCounters[pIdx] == 0)
  {
    // send request to host
    void* dmeConnection = netGetDmeServerConnection();
    if (dmeConnection) {
      ClientRequestPickUpFlag_t msg;
      msg.GameTime = gameGetTime();
      msg.PlayerId = player->PlayerId;
      msg.FlagUID = guberGetUID(flagMoby);
      netSendCustomAppMessage(NET_DELIVERY_CRITICAL, dmeConnection, gameGetHostId(), CUSTOM_MSG_ID_FLAG_REQUEST_PICKUP, sizeof(ClientRequestPickUpFlag_t), &msg);
      flagRequestCounters[pIdx] = 2;
      DPRINTF("sent request flag pickup %d\n", gameTime);
    }
  }
}

/*
 * NAME :		customFlagLogic
 * 
 * DESCRIPTION :
 * 			Reimplements flag pickup logic but runs through our host authoritative logic.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void customFlagLogic(Moby* flagMoby)
{
	VECTOR t;
	int i;
	Player** players = playerGetAll();
	int gameTime = gameGetTime();
	GameOptions* gameOptions = gameGetOptions();

	if (!isInGame()) {
    return;
  }

	// validate flag
	if (!flagMoby)
		return;

	// get pvars
	struct FlagPVars* pvars = (struct FlagPVars*)flagMoby->PVar;
	if (!pvars)
		return;

	// 
	if (flagMoby->State != 1)
		return;

	// flag is currently returning
	if (flagIsReturning(flagMoby))
		return;

	// flag is currently being picked up
	if (flagIsBeingPickedUp(flagMoby))
		return;
	
	// return to base if flag has been idle for 40 seconds
	if ((pvars->TimeFlagDropped + (TIME_SECOND * 40)) < gameTime && !flagIsAtBase(flagMoby)) {
		flagReturnToBase(flagMoby, 0, 0xFF);
		return;
	}

	// return to base if flag landed on bad ground
	if (!flagIsOnSafeGround(flagMoby)) {
		flagReturnToBase(flagMoby, 0, 0xFF);
		return;
	}

	for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
		Player* player = players[i];
		if (!player || !player->IsLocal)
			continue;

		// wait 3 seconds for last carrier to be able to pick up again
		if ((pvars->TimeFlagDropped + 3*TIME_SECOND) > gameTime && i == pvars->LastCarrierIdx)
			continue;

		// only allow actions by living players
		if (playerIsDead(player) || player->Health <= 0)
			continue;

		// something to do with vehicles
		// not sure exactly when this case is true
		if (((int (*)(Player*))0x00619b58)(player))
			continue;

		// skip player if they've only been alive for < 3 seconds
		if (player->timers.timeAlive <= 180)
			continue;

		// skip player if in vehicle
		if (player->Vehicle)
			continue;

		// skip if player state is in vehicle and critterMode is on
		if (player->Camera && player->PlayerState == PLAYER_STATE_VEHICLE && player->Camera->camHeroData.critterMode)
			continue;

		// skip if player is on teleport pad
		if (player->Ground.pMoby && player->Ground.pMoby->OClass == MOBY_ID_TELEPORT_PAD)
			continue;

		// player must be within 2 units of flag
		vector_subtract(t, flagMoby->Position, player->PlayerPosition);
		float sqrDistance = vector_sqrmag(t);
		if (sqrDistance > (2*2))
			continue;

		// player is on different team than flag and player isn't already holding flag
		if (player->Team != pvars->Team) {
			if (!player->HeldMoby) {
        flagRequestPickup(flagMoby, i);
				return;
			}
		}
		// player is on same team so attempt to return flag
		else if (gameOptions->GameFlags.MultiplayerGameFlags.FlagReturn) {
			vector_subtract(t, pvars->BasePosition, flagMoby->Position);
			float sqrDistanceToBase = vector_sqrmag(t);
			if (sqrDistanceToBase > 0.1) {
        flagRequestPickup(flagMoby, i);
				return;
			}
		}
	}

	//0x00619b58
}

/*
 * NAME :		onRemoteClientRequestPickUpFlag
 * 
 * DESCRIPTION :
 * 			Handles when a remote client sends the CUSTOM_MSG_ID_FLAG_REQUEST_PICKUP message.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
int onRemoteClientRequestPickUpFlag(void * connection, void * data)
{
	int i;
	ClientRequestPickUpFlag_t msg;
  Player** players;
	memcpy(&msg, data, sizeof(msg));

	// ignore if not in game
	if (!isInGame() || !gameAmIHost())
	  return sizeof(ClientRequestPickUpFlag_t);

	DPRINTF("remote player %d requested pick up flag %X at %d\n", msg.PlayerId, msg.FlagUID, msg.GameTime);

  // get list of players
  players = playerGetAll();

	// get remote player or ignore message
	Player* remotePlayer = playerGetAll()[msg.PlayerId];
	if (!remotePlayer)
	  return sizeof(ClientRequestPickUpFlag_t);

  // get flag
  GuberMoby* gm = (GuberMoby*)guberGetObjectByUID(msg.FlagUID);
  if (gm && gm->Moby)
  {
    Moby* flagMoby = gm->Moby;
    flagHandlePickup(flagMoby, msg.PlayerId);
  }

	return sizeof(ClientRequestPickUpFlag_t);
}

/*
 * NAME :		runFlagPickupFix
 * 
 * DESCRIPTION :
 * 			Ensures that player's don't "lag through" the flag.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void runFlagPickupFix(void)
{
	VECTOR t;
	int i = 0;
	Player** players = playerGetAll();

	if (!isInGame()) {
    memset(flagRequestCounters, 0, sizeof(flagRequestCounters));
		return;
	}

  // decrement request counters
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    if (flagRequestCounters[i] > 0) flagRequestCounters[i] -= 1;
  }

  netInstallCustomMsgHandler(CUSTOM_MSG_ID_FLAG_REQUEST_PICKUP, &onRemoteClientRequestPickUpFlag);
	
  // set pickup cooldown to 0.5 seconds
  //POKE_U16(0x00418aa0, 500);

	// disable normal flag update code
	POKE_U32(0x00418858, 0x03E00008);
	POKE_U32(0x0041885C, 0x0000102D);

  // allow flag to land on swimmable water
  POKE_U32(0x0038E130, 0x004176B0);

	// run custom flag update on flags
	GuberMoby* gm = guberMobyGetFirst();
  while (gm)
  {
    if (gm->Moby)
    {
      switch (gm->Moby->OClass)
      {
        case MOBY_ID_BLUE_FLAG:
        case MOBY_ID_RED_FLAG:
        case MOBY_ID_GREEN_FLAG:
        case MOBY_ID_ORANGE_FLAG:
        {
					customFlagLogic(gm->Moby);
					break;
				}
			}
		}
    gm = (GuberMoby*)gm->Guber.Prev;
	}

	return;
	/*
	// iterate guber mobies
	// finding each flag
	// we want to check if we're the master
	// if we're not and within some reasonable distance then 
	// just run the flag update manually
  GuberMoby* gm = guberMobyGetFirst();
  while (gm)
  {
    if (gm->Moby)
    {
      switch (gm->Moby->OClass)
      {
        case MOBY_ID_BLUE_FLAG:
        case MOBY_ID_RED_FLAG:
        case MOBY_ID_GREEN_FLAG:
        case MOBY_ID_ORANGE_FLAG:
        {
					int flagUpdateRan = 0;

					// ensure no one is master
					void * master = masterGet(gm->Guber.Id.UID);
					if (master)
						masterDelete(master);

					// detect if flag is currently held
					for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
						Player* player = players[i];
						if (player && player->HeldMoby == gm->Moby) {
							flagUpdateRan = 1;
							break;
						}
					}

					if (!flagUpdateRan) {
						for (i = 0; i < GAME_MAX_LOCALS; ++i) {
							// get local player
							Player* localPlayer = playerGetFromSlot(i);
							if (localPlayer) {
								// get distance from local player to flag
								vector_subtract(t, gm->Moby->Position, localPlayer->PlayerPosition);
								float sqrDist = vector_sqrmag(t);

								if (sqrDist < (5 * 5))
								{
									// run update
									((void (*)(Moby*, u32))0x00418858)(gm->Moby, 0);
									flagUpdateRan = 1;

									// we only need to run it once per client
									// even if both locals are near the flag
									break;
								}
							}
						}
					}

					// run flag update as host if no one else is nearby
					if (gameAmIHost() && !flagUpdateRan) {
						((void (*)(Moby*, u32))0x00418858)(gm->Moby, 0);
					}
          break;
        }
      }
    }
    gm = (GuberMoby*)gm->Guber.Prev;
  }
	*/
}

/*
 * NAME :		patchFlagCaptureMessage_Hook
 * 
 * DESCRIPTION :
 * 			Replaces default flag capture popup text with custom one including
 *      the name of the player who captured the flag.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void patchFlagCaptureMessage_Hook(int localPlayerIdx, int msgStringId, GuberEvent* event)
{
  static char buf[64];
  int teams[4] = {0,0,0,0};
  int teamCount = 0;
  int i;
  GameSettings* gs = gameGetSettings();

  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    int team = gs->PlayerTeams[i];
    if (team >= 0 && teams[team] == 0) {
      teams[team]++;
      teamCount++;
    }
  }

  // flag captured
  if (gs && teamCount < 3 && event && event->NetEvent.EventID == 1 && event->NetEvent.NetData[0] == 1) {

    // pid of player who captured the flag
    int pid = event->NetEvent.NetData[1];
    if (pid >= 0 && pid < GAME_MAX_PLAYERS)
    {
      snprintf(buf, 64, "%s has captured the flag!", gs->PlayerNames[pid]);
      uiShowPopup(localPlayerIdx, buf);
      return;
    }
  }

  // call underlying GUI PrintPrompt
  ((void (*)(int, int))0x00540190)(localPlayerIdx, msgStringId);
}

/*
 * NAME :		patchFlagCaptureMessage
 * 
 * DESCRIPTION :
 * 			Hooks patchFlagCaptureMessage_Hook
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void patchFlagCaptureMessage(void)
{
  if (!isInGame()) return;

  // moves some logic around in a loop to free up an extra instruction slot
  // so that we can pass a pointer to the guber event to our custom message handler
  POKE_U32(0x00418808, 0x92242FEA);
  POKE_U32(0x00418830, 0x263131F0);
  HOOK_JAL(0x00418814, &patchFlagCaptureMessage_Hook);
  POKE_U32(0x00418810, 0x8FA60068);
}

/*
 * NAME :		runHealthPickupFix
 * 
 * DESCRIPTION :
 * 			Ensures that player's don't "lag through" the health.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void runHealthPickupFix(void)
{
	if (!isInGame()) return;

  typedef void (*guberMobyMasterVTableUpdate_func)(void * guber);

	// run master update on healthboxes
	GuberMoby* gm = guberMobyGetFirst();
  while (gm)
  {
    if (gm->Moby)
    {
      switch (gm->Moby->OClass)
      {
        case MOBY_ID_HEALTH_BOX_MULT:
        {
					((guberMobyMasterVTableUpdate_func)(0x00411F60))(gm->Moby);
					break;
				}
			}
		}
    gm = (GuberMoby*)gm->Guber.Prev;
	}
}

/*
 * NAME :		onMobyUpdate
 * 
 * DESCRIPTION :
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void onMobyUpdate(Moby* moby)
{
  playerSyncTick();
  processGameModulesUpdate();

  ((void (*)(Moby*))0x003BD5A8)(moby);

  playerSyncPostTick();
}

/*
 * NAME :		onHeroUpdate
 * 
 * DESCRIPTION :
 * 			Runs gadget event after player updates.
 *      Ensures gadget collision tests happen after hero hitbox is updated.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void onHeroUpdate(void)
{
  // base 
  ((void (*)(void))0x005ce1d8)();

  // after hero update
  // gadget events
  GameSettings* gs = gameGetSettings();
  if (gameConfig.customModeId != CUSTOM_MODE_SURVIVAL && gs && gs->GameRules != GAMERULE_CQ) {
    runPlayerGadgetEventHandlers();
  }
}

/*
 * NAME :		onCalculateHeroAimPos
 * 
 * DESCRIPTION :
 * 		  Triggered when the game attempts to compute the aim position of a player.
 *      This hook is for the collision detection from the player to the aiming direction.
 *      To fix the mag bending, we want to always return 0 (no hit) to prevent bending around hit surface.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
int onCalculateHeroAimPos(VECTOR from, VECTOR to, u64 hitFlag, Moby * moby, MobyColDamageIn* colDamageIn)
{
  if (moby) {
    Player* player = guberMobyGetPlayerDamager(moby);
    if (player && player->WeaponHeldId == WEAPON_ID_MAGMA_CANNON) {
      return 0;
    }
  }

  return CollLine_Fix(from, to, hitFlag, moby, colDamageIn);
}

/*
 * NAME :		onSetPlayerState_GetHit
 * 
 * DESCRIPTION :
 * 		  Triggered when the game registers a flinch and attempts to set the player to the flinch state.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void onSetPlayerState_GetHit(Player * player, int stateId, int a2, int a3, int t0)
{
  // disable local flinching for remote targets when using new player sync
  if (gameConfig.grNewPlayerSync && player && !player->IsLocal) return;

  // if we're disabling damage cooldown
  // and the source is the mag, apply a cooldown
  if (gameConfig.grNoInvTimer && player->LastDamagedMeGadgetId == WEAPON_ID_MAGMA_CANNON) {

    char dmgIdx = player->PlayerMoby->CollDamage;
    int noFlinch = magFlinchCooldown[player->PlayerId];
    if (dmgIdx >= 0) {
      MobyColDamage* colDamage = mobyGetDamage(player->PlayerMoby, 0x80481C40, 0);
      if (colDamage) {
        DPRINTF("mag dmg %f\n", colDamage->DamageHp);
        playerDecHealth(player, colDamage->DamageHp - magDamageCooldownDamage[player->PlayerId]);

        if (!magFlinchCooldown[player->PlayerId]) magFlinchCooldown[player->PlayerId] = 60;
        if (!magDamageCooldown[player->PlayerId]) magDamageCooldown[player->PlayerId] = 5;
        magDamageCooldownDamage[player->PlayerId] = maxf(magDamageCooldownDamage[player->PlayerId], colDamage->DamageHp);
      }

    }

    player->PlayerMoby->CollDamage = -1;
    POKE_U8((u32)player + 0x2ecc, 0);
    if (noFlinch) return;
  }

  PlayerVTable * vtable = playerGetVTable(player);
  vtable->UpdateState(player, stateId, a2, a3, t0);
}

/*
 * NAME :		getB6Damage
 * 
 * DESCRIPTION :
 * 			Adds falloff curve to b6 damage AoE.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
float getB6Damage(Player* hitPlayer)
{
  Moby* b6Moby;
  float radius, damage;

	asm volatile (
    ".set noreorder;"
		"move %0, $s4;"
    "swc1 $f21, 0(%1);"
    "swc1 $f20, 0(%2);"
		: : "r" (b6Moby), "r" (&radius), "r" (&damage)
	);

  // get hip joint of moby
  MATRIX hipJoint;
  mobyGetJointMatrix(hitPlayer->PlayerMoby, 10, hipJoint);

  VECTOR dt;
  vector_subtract(dt, b6Moby->Position, &hipJoint[12]);
  float dist = vector_length(dt);
  DPRINTF("%f/%f dmg:%f\n", dist, radius, damage);

  Player* sourcePlayer = guberMobyGetPlayerDamager(b6Moby);
  if (sourcePlayer) {
    float fullDamage = weaponGetDamage(WEAPON_ID_B6, sourcePlayer->GadgetBox->Gadgets[WEAPON_ID_B6].Level);

    DPRINTF("ground:%d hitJoint:%f groundFromGood:%f\n", hitPlayer->Ground.onGood, hipJoint[14] - b6Moby->Position[2], hitPlayer->PlayerPosition[2] - hitPlayer->Ground.point[2]);

    if (dist < 0.7) damage = fullDamage;
    else if (hitPlayer->Ground.onGood) damage = damage;
    else if (fabsf(hipJoint[14] - b6Moby->Position[2]) < 1.0 && (hitPlayer->PlayerPosition[2] - hitPlayer->Ground.point[2]) < 1.0) damage = damage;
    else damage *= 0.2;

    DPRINTF("damage %f\n", damage);
    return damage;
  }

  DPRINTF("getB6Damage failed\n");
  return 0;
}

/*
 * NAME :		runFixB6EatOnDownSlope
 * 
 * DESCRIPTION :
 * 			B6 shots are supposed to do full damage when a player is hit on the ground.
 * 			But the ground detection fails when walking/cbooting down slopes.
 * 			This allows players to take only partial damage in some circumstances.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void runFixB6EatOnDownSlope(void)
{
  int i = 0;

  while (i < GAME_MAX_LOCALS)
  {
    Player* p = playerGetFromSlot(i);
    if (p) {
      // determine height delta of player from ground
      // player is grounded if "onGood" or height delta from ground is less than 0.3 units
      float hDelta = p->PlayerPosition[2] - *(float*)((u32)p + 0x250 + 0x78);
      int grounded = hDelta < 0.3 || *(int*)((u32)p + 0x250 + 0xA0) != 0;

      // store grounded in unused part of player data
      POKE_U32((u32)p + 0x30C, grounded);
    }

    ++i;
  }

  // patch b6 dist collision calculation to shorter
  // so it explodes closer to target
  POKE_U16(0x003f693c, 0xBD80);

  // patch b6 grounded damage check to read our isGrounded state
  POKE_U16(0x003B56E8, 0x30C);

  // increase b6 full damage range for ungrounded targets
  POKE_U16(0x003b5700, 0x3F80);

  
  // removed because the people do not like
  // patch b6 to deal custom damage curve
  // unless PvE, then use default
  // if (gameConfig.customModeId != CUSTOM_MODE_SURVIVAL) {
  //   HOOK_JAL(0x003B56E8, &getB6Damage);
  //   POKE_U32(0x003B56EC, 0x0040202D);
  //   POKE_U32(0x003B56F0, 0x1000000F);
  //   POKE_U32(0x003B56F4, 0x46000506);
  // }
}

u128 fusionShotUpdatePos(Moby* moby) {

  u128 oldPos = vector_read(moby->Position);

  VECTOR t, to;
  vector_scale(t, (float*)moby->PVar, 8);
  vector_add(to, moby->Position, t);

  // force pos to within moby grid
  // if any component of the new pos is <2 or >1021 then clamp pos along velocity within bounds
  float min = minf(to[0], minf(to[1], to[2]));
  float max = maxf(to[0], maxf(to[1], to[2]));
  int component = -1;
  float target = 0;

  if (min <= 2) {
    if (to[0] == min) component = 0;
    if (to[1] == min) component = 1;
    if (to[2] == min) component = 2;

    target = 2.01;
  } else if (max >= 1021) {
    if (to[0] == max) component = 0;
    if (to[1] == max) component = 1;
    if (to[2] == max) component = 2;

    target = 1020.99;
  }

  // clamp by largest (or smallest) component
  if (component >= 0 && target > 0 && fabsf(t[component]) > 0.01) {
    float scale = (target - moby->Position[component]) / t[component];
    if (scale > 0) {
      vector_scale(t, t, scale);
      vector_add(to, moby->Position, t);
    }
  }

  vector_copy(moby->Position, to);
  return oldPos;
}

/*
 * NAME :		patchWeaponShotLag
 * 
 * DESCRIPTION :
 * 			Patches weapon shots to be less laggy.
 * 			This is always a work-in-progress.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void patchWeaponShotLag(void)
{
  GameSettings* gs = gameGetSettings();
	if (!isInGame())
		return;

	// send all shots reliably
	//u32* ptr = (u32*)0x00627AB4;
	//if (*ptr == 0x906407F8) {
		// change to reliable
		//*ptr = 0x24040000 | 0x40;

		// get rid of additional 3 packets sent
		// since its reliable we don't need redundancy
		//*(u32*)0x0060F474 = 0;
		//*(u32*)0x0060F4C4 = 0;
	//}

	// patches fusion shot so that remote shots aren't given additional "shake"
	// ie, remote shots go in the same direction that is sent by the source client
	POKE_U32(0x003FA28C, 0);

	// patches function that converts fusion shot start and end positions
	// to a direction to always hit target player if there is a target
	POKE_U32(0x003fa5c0, 0x0240402D);
	HOOK_JAL(0x003fa5c8, &getFusionShotDirection);

  // immediately set fusion shot moby state to 1
  // which begins the process of sending the shot fired packet with 1 frame of latency
  // instead of the built in 7 (130ms)
  POKE_U32(0x003fe160, 0);
  HOOK_JAL(0x003fe018, &fusionShotUpdatePos);

	// patch all weapon shots to be shot on remote as soon as they arrive
	// instead of waiting for the gametime when they were shot on the remote
	// since all shots happen immediately (none are sent ahead-of-time)
	// and this only happens when a client's timebase is desync'd
	HOOK_JAL(0x0062ac60, &handleWeaponShotDelayed);
	HOOK_JAL(0x0060f754, &handleWeaponShotDelayed);
	HOOK_JAL(0x0060538c, &handleWeaponShotDelayed);
	HOOK_JAL(0x0060f474, &handleWeaponShotDelayed);

#if B6_BALL_SHOT_FIRED_REPLACEMENT
  HOOK_J(0x003F6164, &onB6BallSpawned);
#endif

	// this disables filtering out fusion shots where the player is facing the opposite direction
	// in other words, a player may appear to shoot behind them but it's just lag/desync
	FUSION_SHOT_BACKWARDS_BRANCH = 0x1000005F;

	// send fusion shot reliably
	if (*(u32*)0x003FCE8C == 0x910407F8)
		*(u32*)0x003FCE8C = 0x24040040;
		
  // extend player timeout from 7.2 seconds to 32.7 seconds
  POKE_U16(0x00613FAC, 0x8000);

  // disable magma cannon shots bending around walls/players
  //POKE_U32(0x003EEBE4, 0);
  //POKE_U32(0x003eeb24, 0);

  // disable aim at near target logic
  HOOK_JAL(0x005F7E64, &onCalculateHeroAimPos);

  // patch GetHit state update to handle custom magma cannon cooldown
  HOOK_JAL(0x005E1DEC, &onSetPlayerState_GetHit);

  // decrement custom magma cannon cooldowns
  int i;
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    if (magDamageCooldown[i]) {
      --magDamageCooldown[i];
      if (!magDamageCooldown[i]) {
        DPRINTF("mag final dmg %d %f\n", i, magDamageCooldownDamage[i]);
        magDamageCooldownDamage[i] = 0;
      }
    }

    if (magFlinchCooldown[i]) magFlinchCooldown[i]--;
  }

  // hook hero update so we can correct when gadget events and hitboxes are updated
  // so that player movement are after gadget events (weapon shots)
  // and hitbox updates are before gadget events
  // for some reason this breaks lock strafe in CQ.. so just turn it off for CQ since it's not played much
  // u32* heroUpdateHookAddr = (u32*)0x003bd854;
  // if (*heroUpdateHookAddr == 0x0C173876) {
  //   HOOK_JAL(0x003bd854, &onHeroUpdate);
      
  //   // disable normal hero gadget event handling
  //   // we want to run this at the end of the update
  //   if (gameConfig.customModeId != CUSTOM_MODE_SURVIVAL && gs->GameRules != GAMERULE_CQ) {
  //     POKE_U32(0x005DFE30, 0);
  //   }
  // }

	// fix b6 eating on down slope
	runFixB6EatOnDownSlope();
}

/*
 * NAME :		runPlayerGadgetEventHandlers
 * 
 * DESCRIPTION :
 * 			Runs the gadget event handlers for each player.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void runPlayerGadgetEventHandlers(void)
{
  int i;
  Player** players = playerGetAll();

  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    Player* player = players[i];
    if (!player || !player->PlayerMoby) continue;

    // MapToMoby (updates moby hitbox)
    ((void (*)(Player*))0x005df3d0)(player);
  }
}

/*
 * NAME :		patchRadarScale
 * 
 * DESCRIPTION :
 * 			Changes the radar scale and zoom.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void patchRadarScale(void)
{
	u32 frameSizeValue = 0;
	u32 minimapBorderColor = 0;
	u32 minimapTeamBorderColorInstruction = 0x0000282D;
	float scale = 1;
	if (!isInGame())
		return;

	// adjust expanded frame size
	if (!config.minimapScale) {
		frameSizeValue = 0x460C6300;
		minimapBorderColor = 0x80969696;
		minimapTeamBorderColorInstruction = 0x0200282D;
	}
	
	// read expanded animation time (0=shrunk, 1=expanded)
	float frameExpandedT = *(float*)0x0030F750;
	float smallScale = 1 + (config.minimapSmallZoom / 5.0);
	float bigScale = 1 + (config.minimapBigZoom / 5.0);
	scale = lerpf(smallScale, bigScale, frameExpandedT);
	
	// set radar frame size during animation
	POKE_U32(0x00556900, frameSizeValue);
	// remove border 
	POKE_U32(0x00278C90, minimapBorderColor);
	// remove team border
	POKE_U32(0x00556D08, minimapTeamBorderColorInstruction);

	// set minimap scale
	*(float*)0x0038A320 = 500 * scale; // when idle
	*(float*)0x0038A324 = 800 * scale; // when moving

	// set blip scale
	*(float*)0x0038A300 = 0.04 / scale;
}

/*
 * NAME :		patchStateUpdate_Hook
 * 
 * DESCRIPTION :
 * 			Performs some extra logic to optimize sending state updates.
 * 			State updates are sent by default every 4 frames.
 * 			Every 16 frames a full state update is sent. This sends additional data as position and player state.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
int patchStateUpdate_Hook(void * a0, void * a1)
{
	int v0 = ((int (*)(void*,void*))0x0061e130)(a0, a1);
	Player * p = (Player*)((u32)a0 - 0x2FEC);

  // we don't have a free bit to store p2/p3 in the tNW_PlayerPadInputMessage
  // users will have to use new player sync
  if (p->IsLocal && p->LocalPlayerIndex > 1)
    return 0;

	// when we're dead we don't really need to send the state very often
	// so we'll only send it every second
	// and when we do we'll send a full update (including position and player state)
	if (p->Health <= 0)
	{
		// only send every 60 frames
		int tick = *(int*)((u32)a0 + 0x1D8);
		if (tick % 60 != 0)
			return 0;

		// set to 1 to force full state update
		//*(u8*)((u32)p + 0x31cf) = 1;
	}
	else
	{
		// set to 1 to force full state update
		//*(u8*)((u32)p + 0x31cf) = 1;
	}

	return v0;
}

/*
 * NAME :		patchStateUpdate
 * 
 * DESCRIPTION :
 * 			Patches state update get size function to call our hooked method.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void patchStateUpdate(void)
{
	if (*(u32*)0x0060eb80 == 0x0C18784C)
		*(u32*)0x0060eb80 = 0x0C000000 | ((u32)&patchStateUpdate_Hook >> 2);
}

/*
 * NAME :		runCorrectPlayerChargebootRotation
 * 
 * DESCRIPTION :
 * 			Forces all remote player's rotations while chargebooting to be in the direction of their velocity.
 * 			This fixes some movement lag caused when a player's rotation is desynced from their chargeboot direction.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void runCorrectPlayerChargebootRotation(void)
{
	int i;
	VECTOR t;
	Player** players = playerGetAll();
  static int heldTriangleTimers[GAME_MAX_PLAYERS];

	if (!isInGame())
		return;

	for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
		Player* p = players[i];

    if (p) {

      // fix mag slide
      if (p->PlayerState == PLAYER_STATE_COMBO_ATTACK && p->timers.state > 40) {
        p->PlayerState = 0;
        DPRINTF("stopped mag slide\n");
      }

      // sync triangle cancelling
      if (!p->IsLocal && playerPadGetButton(p, PAD_TRIANGLE) > 0) {
        heldTriangleTimers[i]++;
        if (heldTriangleTimers[i] > 12) {
          p->timers.noInput = 1;
        }
      } else {
        heldTriangleTimers[i] = 0;
      }

      // only apply to remote players chargebooting
      if (!p->IsLocal && p->PlayerState == PLAYER_STATE_CHARGE) {


        float yaw = *(float*)((u32)p + 0x3a58);

        // get delta between angles
        float dt = clampAngle(yaw - p->PlayerRotation[2]);
        
        // if delta is small, ignore
        if (fabsf(dt) > 0.001) {

          // apply delta rotation to hero turn function
          // at interpolation speed of 1/5 second
          ((void (*)(Player*, float, float, float))0x005e4850)(p, 0, 0, dt * 0.01666 * 5);
        }
      }
    }
	}
}

//--------------------------------------------------------------------------
int onSetPlayerDzoCosmeticsRemote(void * connection, void * data)
{
  return 4 + 0x10*5;
}

/*
 * NAME :		onClientVoteToEndStateUpdateRemote
 * 
 * DESCRIPTION :
 * 			Receives when the host updates the vote to end state.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
int onClientVoteToEndStateUpdateRemote(void * connection, void * data)
{
  memcpy(&voteToEndState, data, sizeof(voteToEndState));
	return sizeof(voteToEndState);
}

/*
 * NAME :		onClientVoteToEndRemote
 * 
 * DESCRIPTION :
 * 			Receives when another client has voted to end.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
int onClientVoteToEndRemote(void * connection, void * data)
{
  int playerId;
  memcpy(&playerId, data, sizeof(playerId));
	onClientVoteToEnd(playerId);

	return sizeof(playerId);
}

/*
 * NAME :		onClientVoteToEnd
 * 
 * DESCRIPTION :
 * 			Handles when a client votes to end.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void onClientVoteToEnd(int playerId)
{
  int i;
  GameSettings* gs = gameGetSettings();
  if (!gs) return;
  if (!gameAmIHost()) return;

  // set
  voteToEndState.Votes[playerId] = 1;

  // tally votes
  voteToEndState.Count = 0;
  for (i = 0; i < GAME_MAX_PLAYERS; ++i)
    if (voteToEndState.Votes[i]) voteToEndState.Count += 1;

  // set timeout
  if (voteToEndState.TimeoutTime <= 0)
    voteToEndState.TimeoutTime = gameGetTime() + TIME_SECOND*30;

  // send update
  netBroadcastCustomAppMessage(NET_DELIVERY_CRITICAL, netGetDmeServerConnection(), CUSTOM_MSG_VOTE_TO_END_STATE_UPDATED, sizeof(voteToEndState), &voteToEndState);
  DPRINTF("End player:%d timeout:%d\n", playerId, voteToEndState.TimeoutTime - gameGetTime());
}

/*
 * NAME :		sendClientVoteForEnd
 * 
 * DESCRIPTION :
 * 			Broadcasts to all other clients in lobby that this client has voted to end.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void sendClientVoteForEnd(void)
{
  int i = 0;
  for (i = 0; i < GAME_MAX_LOCALS; ++i) {
    Player* p = playerGetFromSlot(i);
    if (!p || !p->IsLocal || !p->PlayerMoby) continue;

    int playerId = p->PlayerId;
    if (!gameAmIHost()) {
      netSendCustomAppMessage(NET_DELIVERY_CRITICAL, netGetDmeServerConnection(), -1, CUSTOM_MSG_PLAYER_VOTED_TO_END, 4, &playerId);
    } else {
      onClientVoteToEnd(playerId);
    }
  }
}

/*
 * NAME :		voteToEndNumberOfVotesRequired
 * 
 * DESCRIPTION :
 * 			Returns the number of votes required for the vote to pass.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
int voteToEndNumberOfVotesRequired(void)
{
  GameSettings* gs = gameGetSettings();
  GameData* gameData = gameGetData();
  int i;
  int playerCount = 0;

  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    if (gs->PlayerClients[i] >= 0) playerCount += 1;
  }

  if (gameData->NumStartTeams > 2) return playerCount;
  else return (int)(playerCount * 0.6) + 1;
}

/*
 * NAME :		runVoteToEndLogic
 * 
 * DESCRIPTION :
 * 			Handles all logic related to when a team Ends.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void runVoteToEndLogic(void)
{
  if (!isInGame()) { patchStateContainer.VoteToEndPassed = 0; memset(&voteToEndState, 0, sizeof(voteToEndState)); return; }
  if (gameHasEnded()) return;
  if (voteToEndState.Count <= 0 || voteToEndState.TimeoutTime <= 0) return;

  int i;
  int gameTime = gameGetTime();
  GameData* gameData = gameGetData();
  GameSettings* gs = gameGetSettings();
  Player* player = playerGetFromSlot(0);
  char buf[64];

  int votesNeeded = voteToEndNumberOfVotesRequired();
  if (voteToEndState.Count >= votesNeeded && gameAmIHost()) {

    // reset
    memset(&voteToEndState, 0, sizeof(voteToEndState));

    // pass to modules
    patchStateContainer.VoteToEndPassed = 1;

    // end game
    gameEnd(1);
    return;
  }

  if (voteToEndState.TimeoutTime > gameTime) {

    // have we voted?
    int haveVoted = 0;
    if (player) haveVoted = voteToEndState.Votes[player->PlayerId];

    // draw
    int secondsLeft = (voteToEndState.TimeoutTime - gameTime) / TIME_SECOND;
    char* buttonCombo = "(\x18+\x19) ";
    snprintf(buf, sizeof(buf), "%sVote to End (%d/%d)    %d...", haveVoted ? "" : buttonCombo, voteToEndState.Count, votesNeeded, secondsLeft);
    gfxScreenSpaceText(12, SCREEN_HEIGHT - 18, 1, 1, 0x80000000, buf, -1, 0);
    gfxScreenSpaceText(10, SCREEN_HEIGHT - 20, 1, 1, 0x80FFFFFF, buf, -1, 0);

    // vote to end
    if (!haveVoted && padGetButtonDown(0, PAD_L3 | PAD_R3) > 0) {
      sendClientVoteForEnd();
    }

    // pass to dzo
    if (PATCH_DZO_INTEROP_FUNCS) {
      CustomDzoCommandDrawVoteToEnd_t cmd;
      cmd.SecondsLeft = secondsLeft;
      cmd.Count = voteToEndState.Count;
      cmd.VotesRequired = votesNeeded;
      PATCH_DZO_INTEROP_FUNCS->SendCustomCommandToClient(CUSTOM_DZO_CMD_ID_VOTE_TO_END, sizeof(cmd), &cmd);
    }
  } else if (gameAmIHost() && voteToEndState.TimeoutTime < gameTime) {
    // reset
    memset(&voteToEndState, 0, sizeof(voteToEndState));
    netBroadcastCustomAppMessage(NET_DELIVERY_CRITICAL, netGetDmeServerConnection(), CUSTOM_MSG_VOTE_TO_END_STATE_UPDATED, sizeof(voteToEndState), &voteToEndState);
  }
}

/*
 * NAME :		onClientReady
 * 
 * DESCRIPTION :
 * 			Handles when a client loads the level.
 *      Sets a bit indicating the client has loaded and then checks if all clients have loaded.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void onClientReady(int clientId)
{
  GameSettings* gs = gameGetSettings();
  int i;
  int bit = 1 << clientId;

  // set ready
  patchStateContainer.AllClientsReady = 0;
  patchStateContainer.ClientsReadyMask |= bit;
  DPRINTF("received client ready from %d\n", clientId);

  if (!gs)
    return;

  // check if all clients are ready
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    int clientId = gs->PlayerClients[i];
    if (clientId >= 0) {
      bit = 1 << clientId;
      if ((patchStateContainer.ClientsReadyMask & bit) == 0)
        return;
    }
  }

  patchStateContainer.AllClientsReady = 1;
  DPRINTF("all clients ready\n");
}

/*
 * NAME :		onClientReadyRemote
 * 
 * DESCRIPTION :
 * 			Receives when another client has loaded the level and passes it to the onClientReady handler.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
int onClientReadyRemote(void * connection, void * data)
{
  int clientId;
  memcpy(&clientId, data, sizeof(clientId));
	onClientReady(clientId);

	return sizeof(clientId);
}

/*
 * NAME :		sendClientReady
 * 
 * DESCRIPTION :
 * 			Broadcasts to all other clients in lobby that this client has finished loading the level.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void sendClientReady(void)
{
  int clientId = gameGetMyClientId();
  int bit = 1 << clientId;
  if (patchStateContainer.ClientsReadyMask & bit)
    return;

	netSendCustomAppMessage(NET_DELIVERY_CRITICAL, netGetDmeServerConnection(), -1, CUSTOM_MSG_CLIENT_READY, 4, &clientId);
  DPRINTF("send client ready\n");

	// locally
  onClientReady(clientId);
}

/*
 * NAME :		runClientReadyMessager
 * 
 * DESCRIPTION :
 * 			Sends the current game info to the server.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void runClientReadyMessager(void)
{
  GameSettings* gs = gameGetSettings();

  if (!gs)
  {
    patchStateContainer.AllClientsReady = 0;
    patchStateContainer.ClientsReadyMask = 0;
  }
  else if (isInGame())
  {
    sendClientReady();
  }
}

/*
 * NAME :		runSendGameUpdate
 * 
 * DESCRIPTION :
 * 			Sends the current game info to the server.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
int runSendGameUpdate(void)
{
	static int lastGameUpdate = 0;
	static int newGame = 0;
	GameSettings * gameSettings = gameGetSettings();
	GameOptions * gameOptions = gameGetOptions();
  GameData * gameData = gameGetData();
  Player** players = playerGetAll();
	int gameTime = gameGetTime();
	int i;
	void * connection = netGetLobbyServerConnection();

	// skip if not online, in lobby, or the game host
	if (!connection || !gameSettings || !gameAmIHost())
	{
		lastGameUpdate = -GAME_UPDATE_SENDRATE;
		newGame = 1;
		return 0;
	}

	// skip if time since last update is less than sendrate
	if ((gameTime - lastGameUpdate) < GAME_UPDATE_SENDRATE)
		return 0;

	// update last sent time
	lastGameUpdate = gameTime;

	// construct
	patchStateContainer.GameStateUpdate.RoundNumber = 0;
	patchStateContainer.GameStateUpdate.TeamsEnabled = gameOptions->GameFlags.MultiplayerGameFlags.Teamplay;
	patchStateContainer.GameStateUpdate.Version = 1;

	// copy over client ids
	memcpy(patchStateContainer.GameStateUpdate.ClientIds, gameSettings->PlayerClients, sizeof(patchStateContainer.GameStateUpdate.ClientIds));

	// reset some stuff whenever we enter a new game
	if (newGame)
	{
		memset(patchStateContainer.GameStateUpdate.TeamScores, 0, sizeof(patchStateContainer.GameStateUpdate.TeamScores));
		memset(patchStateContainer.CustomGameStats.Payload, 0, sizeof(patchStateContainer.CustomGameStats.Payload));
		newGame = 0;
	}

	// copy teams over
	memcpy(patchStateContainer.GameStateUpdate.Teams, gameSettings->PlayerTeams, sizeof(patchStateContainer.GameStateUpdate.Teams));

	// 
	if (isInGame())
	{
		memset(patchStateContainer.GameStateUpdate.TeamScores, 0, sizeof(patchStateContainer.GameStateUpdate.TeamScores));

    if (gameSettings->GameRules == GAMERULE_JUGGY) {
      for (i = 0; i < GAME_MAX_PLAYERS; ++i)
      {
        Player* player = players[i];
        if (player) {
          int team = patchStateContainer.GameStateUpdate.Teams[i] = playerGetJuggSafeTeam(player);
          patchStateContainer.GameStateUpdate.TeamScores[team] = gameData->PlayerStats.Kills[i];
        }
      }
    } else {
      for (i = 0; i < GAME_SCOREBOARD_ITEM_COUNT; ++i)
      {
        ScoreboardItem * item = GAME_SCOREBOARD_ARRAY[i];
        if (item)
          patchStateContainer.GameStateUpdate.TeamScores[item->TeamId] = item->Value;
      }
    }
	}

	return 1;
}

/*
 * NAME :		runEnableSingleplayerMusic
 * 
 * DESCRIPTION :
 * 
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Troy "Agent Moose" Pruitt
 */
void runEnableSingleplayerMusic(void)
{
	static int FinishedConvertingTracks = 0;
	static int AddedTracks = 0;
  static int Loading = 0;

  if (isInMenus()) {
    Loading = 0;
    return;
  }

	if (!config.enableSingleplayerMusic || !musicIsLoaded())
		return;

  // indicate to user we're loading sp music
  // running uiRunCallbacks triggers our vsync hook and reinvokes this method
  // while it is still looping
  if (Loading)
  {
    // after finished loading, and in game, set music to include new tracks
    if (Loading == 2 && FinishedConvertingTracks && isInGame()) {
      ((void (*)(int,int,int))0x0051f928)(4,13 + AddedTracks,0x400);
      POKE_U16(0x004A8328, 13 + AddedTracks);
      Loading = 3;
    }

    return;
  }

  Loading = 1;
	u32 NewTracksLocation = 0x001CF940;
	if (!FinishedConvertingTracks || *(u32*)NewTracksLocation == 0)
	{
		AddedTracks = 0;
		int MultiplayerSectorID = *(u32*)0x001CF85C;
    char Stack[0x800];
		int Sector = 0x001CE470;
		int a;
		int Offset = 0;

		// Zero out stack by the appropriate heap size (0x2a0 in this case)
		// This makes sure we get the correct values we need later on.
		memset((u32*)Stack, 0, 0x800);

		// Loop through each Sector
		for(a = 0; a < 12; a++)
		{
      // let the game handle net and rendering stuff
      // this prevents the user from lagging out if the disc
      // takes forever to seek
      //uiRunCallbacks();

			Offset += 0x18;
			int MapSector = *(u32*)(Sector + Offset);
			// Check if Map Sector is not zero
			if (MapSector != 0)
			{
				internal_wadGetSectors(MapSector, 1, Stack);
				int SectorID = *(u32*)(Stack + 0x4);

				// BUG FIX AREA: If Stack is set to 0x23ac00, you need to add SectorID != 0x1DC1BE to if statement.
				// The bug is: On first load, the SectorID isn't what I need it to be,
				// the internal_wadGetSectors function doesn't update it quick enough for some reason.
				// the following if statement fixes it

				// make sure SectorID doesn't match 0x1dc1be, if so:
				// - Subtract 0x18 from offset and -1 from loop.
				if (SectorID != 0x0)
				{
					DPRINTF("Sector: 0x%X\n", MapSector);
					DPRINTF("Sector ID: 0x%X\n", SectorID);

					// do music stuffs~
					// Get SP 2 MP Offset for current SectorID.
					int SP2MP = SectorID - MultiplayerSectorID;
					// Remember we skip the first track because it is the start of the sp track, not the body of it.
					int b = 0;
					int Songs = Stack + 0x18;
					// while current song doesn't equal zero, then convert.
					// if it does equal zero, that means we reached the end of the list and we move onto the next batch of tracks.
					do
					{
						// Left Audio
						int StartingSong = *(u32*)(Songs + b);
						// Right Audio
						int EndingSong = *(u32*)((u32)(Songs + b) + 0x8);
						// Convert Left/Right Audio
						int ConvertedSong_Start = SP2MP + StartingSong;
						int ConvertedSong_End = SP2MP + EndingSong;
						// Apply newly Converted tracks
						*(u32*)(NewTracksLocation) = ConvertedSong_Start;
						*(u32*)(NewTracksLocation + 0x08) = ConvertedSong_End;
						NewTracksLocation += 0x10;
						// If on DreadZone Station, and first song, add 0x20 instead of 0x20
						// This fixes an offset bug.
						if (a == 0 && b == 0)
						{
							b += 0x28;
						}
						else
						{
							b += 0x20;
						}
						AddedTracks++;
					}
					while (*(u32*)(Songs + b) != 0);
				}
				else
				{
					Offset -= 0x18;
					a--;
				}
			}
			else
			{
				a--;
			}
		}

		// Zero out stack to finish the job.
		memset((u32*)Stack, 0, 0x800);

		FinishedConvertingTracks = 1;
		DPRINTF("AddedTracks: %d\n", AddedTracks);
	};

  Loading = 2;
}

/*
 * NAME :		runGameStartMessager
 * 
 * DESCRIPTION :
 * 
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void runGameStartMessager(void)
{
	GameSettings * gameSettings = gameGetSettings();
	if (!gameSettings)
		return;

	// in staging
	if (uiGetActive() == UI_ID_GAME_LOBBY)
	{
		// check if game started
		if (!sentGameStart && gameSettings->GameLoadStartTime > 0)
		{
			// check if host
			if (gameAmIHost())
			{
				netSendCustomAppMessage(NET_DELIVERY_CRITICAL, netGetLobbyServerConnection(), NET_LOBBY_CLIENT_INDEX, CUSTOM_MSG_ID_GAME_LOBBY_STARTED, 0, gameSettings);
			}

      // request latest scavenger hunt settings
      scavHuntQueryForRemoteSettings();

      // get server datetime
      void* connection = netGetLobbyServerConnection();
      if (connection) netSendCustomAppMessage(NET_DELIVERY_CRITICAL, connection, NET_LOBBY_CLIENT_INDEX, CUSTOM_MSG_ID_CLIENT_REQUEST_SERVER_DATE_TIME, 0, NULL);

#if DEBUG
			redownloadCustomModeBinaries = 1;
#endif

			sentGameStart = 1;
		}
	}
	else
	{
		sentGameStart = 0;
	}
}

int checkStateCondition(const PlayerStateCondition_t * condition, int localState, int remoteState)
{
	switch (condition->Type)
	{
		case PLAYERSTATECONDITION_REMOTE_EQUALS: // check remote is and local isn't
		{
			return condition->StateId == remoteState;
		}
		case PLAYERSTATECONDITION_LOCAL_EQUALS: // check local is and remote isn't
		{
			return condition->StateId == localState;
		}
		case PLAYERSTATECONDITION_LOCAL_OR_REMOTE_EQUALS: // check local or remote is
		{
			return condition->StateId == remoteState || condition->StateId == localState;
		}
	}

	return 0;
}

/*
 * NAME :		runPlayerPositionSmooth
 * 
 * DESCRIPTION :
 * 
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void runPlayerPositionSmooth(void)
{
  static VECTOR smoothVelocity[GAME_MAX_PLAYERS];
  static int applySmoothVelocityForFrames[GAME_MAX_PLAYERS];

  Player ** players = playerGetAll();
	int i;

  for (i = 0; i < GAME_MAX_PLAYERS; ++i)
  {
    Player* p = players[i];
    if (p && !playerIsLocal(p) && p->PlayerMoby)
    {
      // determine if the player has strayed too far from the remote player's position
      // it is critical that we run this each frame, regardless if we're already smoothing the player's velocity
      // in case something has changed, we want to recalcuate the smooth velocity instantly
      // instead of waiting for the (possibly) 10 smoothing frames to complete
      if (p->pNetPlayer && p->pNetPlayer->pNetPlayerData) {
        VECTOR dt, rPos, lPos = {0,0,1,0};
        vector_copy(dt, (float*)((u32)p + 0x3a40));
        vector_copy(rPos, (float*)((u32)p + 0x3a20));

        // apply when remote player's simulated position
        // is more than 2 units from the received position
        // at the time of receipt
        // meaning, this value only updates when receivedSyncPos (0x3a20) is updated
        // not every tick
        float dist = vector_length(dt);
        if (dist > 2) {
          
          // reset syncPosDifference
          // since it updates every 4 frames (I think)
          // and we don't want to run this 4 times in a row on the same data
          vector_write((float*)((u32)p + 0x3a40), 0);

          // we want to apply this delta over multiple frames for the smoothest result
          // however there are cases where we want to teleport
          // in cases where the player has fallen under the ground on our screen
          // we want to teleport them back up, since gravity will counteract our interpolation
          int applyOverTicks = 10;
          rPos[2] += 1;
          vector_add(lPos, lPos, p->PlayerPosition);
          if (dt[2] > 0 && (dt[2] / dist) > 0.8 && CollLine_Fix(lPos, rPos, 2, p->PlayerMoby, 0)) {
            applyOverTicks = 1;
          }

          // indicate number of ticks to apply additives over
          // scale dt by number of ticks to add
          // so that we add exactly dt over those frames
          applySmoothVelocityForFrames[i] = applyOverTicks;
          vector_scale(smoothVelocity[i], dt, 1.0 / applyOverTicks);
        }
      }

      // apply adds
      if (applySmoothVelocityForFrames[i] > 0) {
        vector_add(p->PlayerPosition, p->PlayerPosition, smoothVelocity[i]);
        vector_copy(p->PlayerMoby->Position, p->PlayerPosition);
        --applySmoothVelocityForFrames[i];
      }
    }
  }
}

/*
 * NAME :		runPlayerStateSync
 * 
 * DESCRIPTION :
 * 
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void runPlayerStateSync(void)
{
	const int stateForceCount = sizeof(stateForceRemoteConditions) / sizeof(PlayerStateCondition_t);
	const int stateSkipCount = sizeof(stateSkipRemoteConditions) / sizeof(PlayerStateCondition_t);
	int gameTime = gameGetTime();
	Player ** players = playerGetAll();
	int i,j;

	if (!isInGame() || !config.enablePlayerStateSync)
		return;

	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
	{
		Player* p = players[i];
		if (p && !playerIsLocal(p))
		{
			// get remote state
			int localState = p->PlayerState;
			int remoteState = *(int*)((u32)p + 0x3a80);
			if (RemoteStateTimeStart[i].CurrentRemoteState != remoteState)
			{
				RemoteStateTimeStart[i].CurrentRemoteState = remoteState;
				RemoteStateTimeStart[i].TimeRemoteStateLastChanged = gameTime;
			}
			int remoteStateTicks = ((gameTime - RemoteStateTimeStart[i].TimeRemoteStateLastChanged) / 1000) * 60;

			// force onto local state
			PlayerVTable* vtable = playerGetVTable(p);
			if (!playerIsDead(p) && vtable && remoteState != localState)
			{
				int pStateTimer = p->timers.state;
				int skip = 0;

				// iterate each condition
				// if one is true, skip to the next player
				for (j = 0; j < stateSkipCount; ++j)
				{
					const PlayerStateCondition_t* condition = &stateSkipRemoteConditions[j];
					if (pStateTimer >= condition->TicksSince)
					{
						if (checkStateCondition(condition, localState, remoteState))
						{
							//DPRINTF("%d skipping remote player %08x (%d) state (%d) timer:%d\n", j, (u32)p, p->PlayerId, remoteState, pStateTimer);
							skip = 1;
							break;
						}
					}
				}

				// go to next player
				if (skip)
					continue;

				// iterate each condition
				// if one is true, then force the remote state onto the local player
				for (j = 0; j < stateForceCount; ++j)
				{
					const PlayerStateCondition_t* condition = &stateForceRemoteConditions[j];
					if (pStateTimer >= condition->TicksSince
							&& (condition->MaxTicks <= 0 || remoteStateTicks < condition->MaxTicks)
							&& (gameTime - RemoteStateTimeStart[i].TimeLastRemoteStateForced) > 500)
					{
						if (checkStateCondition(condition, localState, remoteState))
						{
							if (condition->MaxTicks > 0 && remoteState != condition->StateId) {
								//DPRINTF("%d changing remote player %08x (%d) state ticks to %d (from %d) state:%d\n", j, (u32)p, p->PlayerId, condition->MaxTicks, p->timers.state, localState);
								p->timers.state = condition->MaxTicks;
							} else {
								//DPRINTF("%d changing remote player %08x (%d) state to %d (from %d) timer:%d\n", j, (u32)p, p->PlayerId, remoteState, localState, pStateTimer);
								vtable->UpdateState(p, remoteState, 1, 0, 1);
								p->timers.state = remoteStateTicks;
							}
							RemoteStateTimeStart[i].TimeLastRemoteStateForced = gameTime;
							break;
						}
					}
				}
			}
		}
	}
}

/*
 * NAME :		onGameStartMenuBack
 * 
 * DESCRIPTION :
 * 			Called when the user selects 'Back' in the in game start menu
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void onGameStartMenuBack(long a0)
{
	// call start menu back callback
	((void (*)(long))0x00560E30)(a0);

	// open config
	if (netGetLobbyServerConnection())
		configMenuEnable();
}

/*
 * NAME :		patchStartMenuBack_Hook
 * 
 * DESCRIPTION :
 * 			Changes the BACK button text to our patch config text.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Troy "Agent Moose" Pruitt
 */
void patchStartMenuBack_Hook(long a0, u64 a1, u64 a2, u8 a3)
{
    // Force Start Menu to swap "Back" Option with "Patch Config"
    /*
        a0: Unknown
        a1: String
        a2: Function Pointer
        a3: Unknown
    */
    ((void (*)(long, const char*, void*, u8))0x005600F8)(a0, patchConfigStr, &onGameStartMenuBack, a3);
}

/*
 * NAME :		hookedProcessLevel
 * 
 * DESCRIPTION :
 * 		 	Function hook that the game will invoke when it is about to start processing a newly loaded level.
 * 			The argument passed is the address of the function to call to start processing the level.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
u64 hookedProcessLevel()
{
  // enable singleplayer music
  runEnableSingleplayerMusic();

	u64 r = ((u64 (*)(void))0x001579A0)();

	// Start at the first game module
	GameModule * module = GLOBAL_GAME_MODULES_START;

  // increase wait for players to 45 seconds
  POKE_U32(0x0021E1E8, 45 * 60);

  // fade from black delay
  // if this is not increased, and a player takes more than 2000 ticks to load
  // players waiting for them to load will load in with a black screen
  if (*(u16*)0x005ce238 == 2000) {
    POKE_U16(0x005ce238, 120 * 60);
  }

	// call gamerules level load
	grLoadStart();

	// pass event to modules
	while (module->Entrypoint)
	{
		if (module->State > GAMEMODULE_OFF)
			module->Entrypoint(module, &patchStateContainer, GAMEMODULE_LOAD);

		++module;
	}

	return r;
}

/*
 * NAME :		patchProcessLevel
 * 
 * DESCRIPTION :
 * 			Installs hook at where the game starts the process a newly loaded level.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void patchProcessLevel(void)
{
	// jal hookedProcessLevel
	*(u32*)0x00157D38 = 0x0C000000 | (u32)&hookedProcessLevel / 4;
}

/*
 * NAME :		sendGameDataBlock
 * 
 * DESCRIPTION :
 * 			Sends a block of data over to the server as a GameDataBlock.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
int sendGameDataBlock(short offset, char endOfList, void* buffer, int size)
{
	int i = 0;
	struct GameDataBlock data;

	data.Offset = offset;

	// send in chunks
	while (i < size)
	{
		short len = (size - i);
		if (len > sizeof(data.Payload)) {
			len = sizeof(data.Payload);
	    data.EndOfList = 0;
    } else {
	    data.EndOfList = endOfList;
    }

		data.Length = len;
		memcpy(data.Payload, (char*)buffer + i, len);
		netSendCustomAppMessage(NET_DELIVERY_CRITICAL, netGetLobbyServerConnection(), NET_LOBBY_CLIENT_INDEX, CUSTOM_MSG_ID_CLIENT_SEND_GAME_DATA, sizeof(struct GameDataBlock), &data);
		i += len;
		data.Offset += len;
	}

	// return written bytes
	return i;
}

/*
 * NAME :		sendGameData
 * 
 * DESCRIPTION :
 * 			Sends all game data, including stats, to the server when the game ends.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void sendGameData(void)
{
  GameOptions* gameOptions = gameGetOptions();
  GameSettings* gameSettings = gameGetSettings();
  GameData* gameData = gameGetData();
	short offset = 0;
  int hasCustomGameData = patchStateContainer.CustomGameStatsSize > 0;
  int header[] = {
    0x1337C0DE,
    PATCH_GAME_STATS_VERSION
  };

	// ensure in game/staging
	if (!gameSettings)
		return;
	
	DPRINTF("sending stats... ");
	offset += sendGameDataBlock(offset, 0, header, sizeof(header));
	offset += sendGameDataBlock(offset, 0, gameData, sizeof(GameData)); uiRunCallbacks();
	offset += sendGameDataBlock(offset, 0, &patchStateContainer.GameSettingsAtStart, sizeof(GameSettings)); uiRunCallbacks();
	offset += sendGameDataBlock(offset, 0, gameSettings, sizeof(GameSettings)); uiRunCallbacks();
	offset += sendGameDataBlock(offset, 0, gameOptions, sizeof(GameOptions)); uiRunCallbacks();
	offset += sendGameDataBlock(offset, !hasCustomGameData, &patchStateContainer.GameStateUpdate, sizeof(UpdateGameStateRequest_t));
  if (hasCustomGameData) {
    uiRunCallbacks();
	  offset += sendGameDataBlock(offset, 1, &patchStateContainer.CustomGameStats, patchStateContainer.CustomGameStatsSize);
  }
  
	DPRINTF("done.\n");
}

/*
 * NAME :		processSendGameData
 * 
 * DESCRIPTION :
 * 			Logic that determines when to send game data to the server.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
int processSendGameData(void)
{
	static int state = 0;
  GameSettings* gameSettings = gameGetSettings();
	int send = 0;

	// ensure in game/staging
	if (!gameSettings)
		return 0;
	
	if (isInGame())
	{
		// move game settings
		if (state == 0)
		{
			memcpy(&patchStateContainer.GameSettingsAtStart, gameSettings, sizeof(GameSettings));
			state = 1;
		}

		// game has ended
		if (state == 1 && gameGetFinishedExitTime() && gameAmIHost())
			send = 1;
	}
	else if (state > 0)
	{
		// host leaves the game
		if (state == 1 && gameAmIHost())
			send = 1;

		state = 0;
	}

	// 
	if (send)
		state = 2;

	return send;
}

/*
 * NAME :		runFpsCounter
 * 
 * DESCRIPTION :
 * 			Displays an fps counter on the top right screen.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void runFpsCounter(void)
{
	char buf[64];
	static int lastGameTime = 0;
	static int tickCounter = 0;

	if (!isInGame())
		return;

	// initialize time
	if (tickCounter == 0 && lastGameTime == 0)
		lastGameTime = gameGetTime();
	
	// update fps every 60 frames
	++tickCounter;
	if (tickCounter >= 60)
	{
		int currentTime = gameGetTime();
		lastFps = tickCounter / ((currentTime - lastGameTime) / (float)TIME_SECOND);
		lastGameTime = currentTime;
		tickCounter = 0;
	}

	// render if enabled
	if (config.enableFpsCounter)
	{
		if (averageRenderTimeMs > 0) {
			snprintf(buf, 64, "EE: %.1fms GS: %.1fms FPS: %.2f", averageUpdateTimeMs, averageRenderTimeMs, lastFps);
		} else {
			snprintf(buf, 64, "FPS: %.2f", lastFps);
		}
		
		gfxScreenSpaceText(SCREEN_WIDTH - 5, 5, 0.75, 0.75, 0x80FFFFFF, buf, -1, 2);
	}
}

void runFastLoad(void)
{
  if (interopData.Client == CLIENT_TYPE_NORMAL) return; // only DZO/EMU
  if (!MapLoaderState.Enabled) return; // only custom maps

  GameSettings* gs = gameGetSettings();
  if (!gs) return;

  if (gs->GameStartTime < 0 && isSceneLoading()) {
    POKE_U32(0x006868A0, 0);
    POKE_U32(0x006865b0, 0);
    POKE_U32(0x005B0354, 0);
    POKE_U32(0x005B047C, 0);
    POKE_U32(0x005B04B4, 0);
    POKE_U32(0x0015B118, 0); // disable timebanditshack
    //POKE_U32(0x006859fc, 0x24020001);
  }
  
  // re-enable timebanditshack
  if (!isSceneLoading()) {
    POKE_U32(0x0015B118, 0xACC40218); 
  }
}

int hookCheckHostStartGame(void* a0)
{
	GameSettings* gs = gameGetSettings();

	// call base
	int v0 = ((int (*)(void*))0x00757660)(a0);

	// success
	if (v0) {

		// verify we have map
		if (mapOverrideResponse < 0) {
			showNoMapPopup = 1;
			netSendCustomAppMessage(NET_DELIVERY_CRITICAL, netGetLobbyServerConnection(), NET_LOBBY_CLIENT_INDEX, CUSTOM_MSG_ID_REQUEST_MAP_OVERRIDE, 0, NULL);
			return 0;
		}

    // if survival
    // verify we have the latest maps
    if (gameConfig.customModeId == CUSTOM_MODE_SURVIVAL && mapsLocalGlobalVersion != mapsRemoteGlobalVersion) {
      readLocalGlobalVersion();
      if (mapsLocalGlobalVersion != mapsRemoteGlobalVersion) {
        showNeedLatestMapsPopup = 1;
        return 0;
      }
    }

		// wait for download to finish
		if (dlIsActive) {
			showMiscPopup = 1;
			strncpy(miscPopupTitle, "System", 32);
			strncpy(miscPopupBody, "Please wait for the download to finish.", 64);
			return 0;
		}

		// if training, verify we're the only player in the lobby
		if (gameConfig.customModeId == CUSTOM_MODE_TRAINING && gs && gs->PlayerCount != 1) {
			showMiscPopup = 1;
			strncpy(miscPopupTitle, "Training", 32);
			strncpy(miscPopupBody, "Too many players to start.", 64);
			return 0;
		}
	}

	return v0;
}

/*
 * NAME :		runCheckGameMapInstalled
 * 
 * DESCRIPTION :
 * 
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void runCheckGameMapInstalled(void)
{
	int i;
	GameSettings* gs = gameGetSettings();
	if (!gs || !isInMenus())
		return;

	// install start game hook
	if (*(u32*)0x00759580 == 0x0C1D5D98)
		*(u32*)0x00759580 = 0x0C000000 | ((u32)&hookCheckHostStartGame >> 2);

	int clientId = gameGetMyClientId();
  for (i = 1; i < GAME_MAX_PLAYERS; ++i)
  {
    if (gs->PlayerClients[i] == clientId && gs->PlayerStates[i] == 6)
    {
      // need map
      if (mapOverrideResponse < 0)
      {
        gameSetClientState(i, 0);
        showNoMapPopup = 1;
        netSendCustomAppMessage(NET_DELIVERY_CRITICAL, netGetLobbyServerConnection(), NET_LOBBY_CLIENT_INDEX, CUSTOM_MSG_ID_REQUEST_MAP_OVERRIDE, 0, NULL);
      }
      else if (gameConfig.customModeId == CUSTOM_MODE_SURVIVAL && mapsLocalGlobalVersion != mapsRemoteGlobalVersion) {
        readLocalGlobalVersion();
        if (mapsLocalGlobalVersion != mapsRemoteGlobalVersion) {
          gameSetClientState(i, 0);
          showNeedLatestMapsPopup = 1;
        }
      }
    }
  }
}

/*
 * NAME :		processGameModules
 * 
 * DESCRIPTION :
 * 
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void processGameModules()
{
	// Start at the first game module
	GameModule * module = GLOBAL_GAME_MODULES_START;

	// Game settings
	GameSettings * gamesettings = gameGetSettings();

	// Iterate through all the game modules until we hit an empty one
	while (module->Entrypoint)
	{
		// Ensure we have game settings
		if (gamesettings)
		{
			// Check the module is enabled
			if (module->State > GAMEMODULE_OFF)
			{
				// If in game, run game entrypoint
				if (isInGame())
				{
					// Check if the game hasn't ended
					// We also give the module a second after the game has ended to
					// do some end game logic
					if (!gameHasEnded() || gameGetTime() < (gameGetFinishedExitTime() + TIME_SECOND))
					{
						// Invoke module
            module->Entrypoint(module, &patchStateContainer, GAMEMODULE_GAME_FRAME);
					}
				}
				else if (isInMenus())
				{
					// Invoke lobby module if still active
          module->Entrypoint(module, &patchStateContainer, GAMEMODULE_LOBBY);
				}
			}

		}
		// If we aren't in a game then try to turn the module off
		// ONLY if it's temporarily enabled
		else if (module->State == GAMEMODULE_TEMP_ON)
		{
			module->State = GAMEMODULE_OFF;
		}
		else if (module->State == GAMEMODULE_ALWAYS_ON)
		{
			// Invoke lobby module if still active
			if (isInMenus())
			{
				module->Entrypoint(module, &patchStateContainer, GAMEMODULE_LOBBY);
			}
		}

		++module;
	}
}

/*
 * NAME :		processGameModulesUpdate
 * 
 * DESCRIPTION :
 * 
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void processGameModulesUpdate()
{
	// Start at the first game module
	GameModule * module = GLOBAL_GAME_MODULES_START;

	// check we're in game
  if (!isInGame() || !gameGetSettings()) return;
  if (gameHasEnded() && gameGetTime() > (gameGetFinishedExitTime() + TIME_SECOND)) return;

	// Iterate through all the game modules until we hit an empty one
	while (module->Entrypoint)
	{
    // Check the module is enabled
    if (module->State > GAMEMODULE_OFF)
    {
      // Invoke module
      module->Entrypoint(module, &patchStateContainer, GAMEMODULE_GAME_UPDATE);
    }

		++module;
	}
}

/*
 * NAME :		onSetTeams
 * 
 * DESCRIPTION :
 * 			Called when the server requests the client to change the lobby's teams.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
int onSetTeams(void * connection, void * data)
{
	int i, j;
  ChangeTeamRequest_t request;
	u32 seed;
	char teamByClientId[GAME_MAX_PLAYERS];

	// move message payload into local
	memcpy(&request, data, sizeof(ChangeTeamRequest_t));

	// move seed
	memcpy(&seed, &request.Seed, 4);

	//
	memset(teamByClientId, -1, sizeof(teamByClientId));

#if DEBUG
	printf("pool size: %d\npool: ", request.PoolSize);
	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
		printf("%d=%d,", i, request.Pool[i]);
	printf("\n");
#endif

	// get game settings
	GameSettings* gameSettings = gameGetSettings();
	if (gameSettings)
	{
		for (i = 0; i < GAME_MAX_PLAYERS; ++i)
		{
			int clientId = gameSettings->PlayerClients[i];
			if (clientId >= 0)
			{
				int teamId = teamByClientId[clientId];
				if (teamId < 0)
				{
					if (request.PoolSize == 0)
					{
						teamId = 0;
					}
					else
					{
						// psuedo random
						sha1(&seed, 4, &seed, 4);

						// get pool index from rng
						int teamPoolIndex = seed % request.PoolSize;

						// set team
						teamId = request.Pool[teamPoolIndex];

						DPRINTF("pool info pid:%d poolIndex:%d poolSize:%d team:%d\n", i, teamPoolIndex, request.PoolSize, teamId);

						// remove element from pool
						if (request.PoolSize > 0)
						{
							for (j = teamPoolIndex+1; j < request.PoolSize; ++j)
								request.Pool[j-1] = request.Pool[j];
							request.PoolSize -= 1;

#if DEBUG
							printf("pool after shift ");
							for (j = 0; j < request.PoolSize; ++j)
								printf("%d=%d ", j, request.Pool[j]);
							printf("\n");
#endif
						}
					}

					// set client id team
					teamByClientId[clientId] = teamId;
				}

				// set team
				DPRINTF("setting pid:%d to %d\n", i, teamId);
				gameSettings->PlayerTeams[i] = teamId;
			}
		}
	}

	return sizeof(ChangeTeamRequest_t);
}

/*
 * NAME :		onSetLobbyClientPatchConfig
 * 
 * DESCRIPTION :
 * 			Called when the server broadcasts a lobby client's patch config.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
int onSetLobbyClientPatchConfig(void * connection, void * data)
{
	int i;
  SetLobbyClientPatchConfigRequest_t request;
	GameSettings* gs = gameGetSettings();

	// move message payload into local
	memcpy(&request, data, sizeof(SetLobbyClientPatchConfigRequest_t));

	if (request.PlayerId >= 0 && request.PlayerId < GAME_MAX_PLAYERS) {
		DPRINTF("recieved %d config\n", request.PlayerId);

		
		memcpy(&lobbyPlayerConfigs[request.PlayerId], &request.Config, sizeof(PatchConfig_t));
	}

	return sizeof(SetLobbyClientPatchConfigRequest_t);
}

/*
 * NAME :		padMappedPadHooked
 * 
 * DESCRIPTION :
 * 			Replaces game's built in Pad_MappedPad function
 *      which remaps input masks.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
/*
int padMappedPadHooked(int padMask, int a1)
{
  if (config.enableSingleTapChargeboot && padMask == 8) return 0x3; // R1 -> L2 or R2 (cboot)

  // not third person
  if (*(char*)(0x00171de0 + a1) != 0) {
    if (padMask == 0x08) return 1; // R1 -> L2 (cboot)
    if (padMask < 9) {
      if (padMask == 4) return 1; // L1 -> L2 (ADS)
    } else {
      if (padMask == 0x10 && config.enableSingleTapChargeboot) return 0x10; // Triangle -> Triangle (quick select)
      if (padMask == 0x10) return 0x12; // Triangle -> Triangle or R2 (quick select)
      if (padMask == 0x40) return 0x44; // Cross -> Cross or L1 (jump)
    }
  }

  return padMask;
}
*/

/*
 * NAME :		forceLobbyNameOverrides
 * 
 * DESCRIPTION :
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void forceLobbyNameOverrides(void)
{
  hasPendingLobbyNameOverrides = 0;
  GameSettings* gs = gameGetSettings();
  if (!gs) return;

  int i;
  int locals[GAME_MAX_PLAYERS];
  memset(locals, 0, sizeof(locals));
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    int accountId = patchStateContainer.LobbyNameOverrides.AccountIds[i];
    int j;
    for (j = 0; j < GAME_MAX_PLAYERS; ++j) {
      if (gs->PlayerAccountIds[j] == accountId) {
        if (locals[i] == 0) {
          strncpy(gs->PlayerNames[j], patchStateContainer.LobbyNameOverrides.Names[i], 16);
        }
        locals[i]++;
      }
    }
  }
}

/*
 * NAME :		onServerDateTimeResponseRemote
 * 
 * DESCRIPTION :
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void onServerDateTimeResponseRemote(void* connection, void* data)
{
  ServerDateTimeMessage_t msg;
  memcpy(&msg, data, sizeof(ServerDateTimeMessage_t));
  
  interopData.Month = msg.Month;
}

/*
 * NAME :		onServerSetLobbyNameOverridesRemote
 * 
 * DESCRIPTION :
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void onServerSetLobbyNameOverridesRemote(void* connection, void* data)
{
  memcpy(&patchStateContainer.LobbyNameOverrides, data, sizeof(SetNameOverridesMessage_t));
  hasPendingLobbyNameOverrides = 1;
}

/*
 * NAME :		onSetRanks
 * 
 * DESCRIPTION :
 * 			Called when the server requests the client to change the lobby's ranks.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
int onSetRanks(void * connection, void * data)
{
	int i, j;

	// move message payload into local
	memcpy(&lastSetRanksRequest, data, sizeof(ServerSetRanksRequest_t));

  // 
  if (gameConfig.customModeId == CUSTOM_MODE_SURVIVAL && !hasShownSurvivalPrestigeMessage) {
    for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
      if (lastSetRanksRequest.AccountIds[i] == gameGetMyAccountId() && lastSetRanksRequest.Ranks[i] >= 10000) {
        hasShownSurvivalPrestigeMessage = 1;
      }
    }
  }

	return sizeof(ServerSetRanksRequest_t);
}

/*
 * NAME :		getCustomGamemodeRankNumber
 * 
 * DESCRIPTION :
 * 			Returns the player rank number for the given player based off the current gamemode or custom gamemode.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
int getCustomGamemodeRankNumber(int offset)
{
	GameSettings* gs = gameGetSettings();
	int pid = offset / 4;
	float rank = gs->PlayerRanks[pid];
	int i;

	if (gameConfig.customModeId != CUSTOM_MODE_NONE && lastSetRanksRequest.Enabled)
	{
		for (i = 0; i < GAME_MAX_PLAYERS; ++i)
		{
			if (lastSetRanksRequest.AccountIds[i] == gs->PlayerAccountIds[pid])
			{
				rank = lastSetRanksRequest.Ranks[i];
				break;
			}
		}
	}

#if COMP
  if (rank < 1000) return 0x759d;         // Avenger
  else if (rank < 2000) return 0x759e;    // Crusader
  else if (rank < 3000) return 0x75a0;    // Liberator
  else if (rank < 4000) return 0x75a1;    // Marauder
  return 0x75a2;                          // Vindicator
#endif

	return ((int (*)(float))0x0077B8A0)(rank);
}

/*
 * NAME :		patchStagingRankNumber
 * 
 * DESCRIPTION :
 * 			Patches the staging menu's player rank menu elements to read the rank from our custom function.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void patchStagingRankNumber(void)
{
	if (!isInMenus())
		return;
	
#if COMP
  POKE_U32(0x0075AC48, 0x00408021);
  POKE_U32(0x0075ac68, 0);
#endif

	HOOK_JAL(0x0075AC3C, &getCustomGamemodeRankNumber);
	POKE_U32(0x0075AC40, 0x0060202D);
}

//--------------------------------------------------------------------------
void* searchGetSearchResultPtr(int idx)
{
  u32 p1 = *(u32*)0x002233a4;
  if (p1 > 0) {
    u32 p2 = *(u32*)(p1 + 0x44);
    if (p2 > 0) {
      int count = *(int*)(p2 + 0x40);
      u32 ptrs = *(u32*)(p2 + 0x4C);

      if (idx < count && count > 0 && ptrs > 0) {
        u32 ptr = *(u32*)(ptrs + idx*4);
        if (ptr > 0) {
          return (void*)ptr;
        }
      }
    }
  }

  return NULL;
}

//--------------------------------------------------------------------------
char* searchGetMapNameFromMapId(int mapId)
{
  int idx;

	// pointer to b6 ball moby is stored in $v0
	asm volatile (
		"move %0, $s1"
		: : "r" (idx)
	);

  char* map = ((char* (*)(int))0x00764330)(mapId);
  void* ptr = searchGetSearchResultPtr(idx / 4);
  if (ptr) {
    int len = strlen((char*)(ptr + 0x60));
    char* customName = (char*)(ptr + 0x60 + len + 2);
    if (customName[0])
      return customName;
  }

  return map;
}

//--------------------------------------------------------------------------
char* searchGetMapNameFromMapId2(int mapId, u64 a1, u64 a2, char* gameName)
{
  int idx = 0;
  void* ptr = 0;

  while ((ptr = searchGetSearchResultPtr(idx))) {
    char* name = (char*)(ptr + 0x60);
    if (strncmp(name, gameName, 0x10) == 0) {
      int len = strlen(name);
      char* customName = (char*)(ptr + 0x60 + len + 2);
      if (customName[0])
        return customName;
      
      break;
    }

    ++idx;
  }
  
  return ((char* (*)(int))0x00764330)(mapId);
}

//--------------------------------------------------------------------------
char* searchGetModeNameFromModeId2(int modeId)
{
  int idx = 0;
  void* ptr = 0;
  char* gameName;

	asm volatile (
		"move %0, $s4"
		: : "r" (gameName)
	);

  char* modeName = ((char* (*)(int))0x00764B80)(modeId);
  while ((ptr = searchGetSearchResultPtr(idx))) {
    char* name = (char*)(ptr + 0x60);
    if (strncmp(name, gameName, 0x10) == 0) {
      int len = strlen((char*)(ptr + 0x60));
      int customMode = *(u8*)(ptr + 0x60 + len + 1);
      if (customMode) {
        int j;
        for (j = 0; j < dataCustomModes.count; ++j) {
          if (dataCustomModes.items[j].value == customMode) {
            return dataCustomModes.items[j].name;
          }
        }
      }
      
      break;
    }

    ++idx;
  }
  
  return modeName;
}

//--------------------------------------------------------------------------
void searchSetModeName(UiTextElement_t* element, char* str)
{
  int idx;
  
	// pointer to b6 ball moby is stored in $v0
	asm volatile (
		"move %0, $s3"
		: : "r" (idx)
	);

  void* ptr = searchGetSearchResultPtr(idx);
  if (ptr) {
    int len = strlen((char*)(ptr + 0x60));
    int customMode = *(u8*)(ptr + 0x60 + len + 1);
    if (customMode) {
      int j;
      for (j = 0; j < dataCustomModes.count; ++j) {
        if (dataCustomModes.items[j].value == customMode) {
          str = dataCustomModes.items[j].name;
          break;
        }
      }
    }
  }

  return ((void (*)(UiTextElement_t*, char*))0x00715330)(element, str);
}

//--------------------------------------------------------------------------
void runShowCustomMapModeInSearch(void)
{
  if (!isInMenus()) return;

  HOOK_JAL(0x00735410, &searchGetMapNameFromMapId);
  HOOK_JAL(0x00735500, &searchSetModeName);
  HOOK_JAL(0x00735524, &searchSetModeName);
  HOOK_JAL(0x00737CBC, &searchGetMapNameFromMapId2);
  HOOK_JAL(0x00737CF4, &searchGetModeNameFromModeId2);
}

/*
 */
#if SCR_PRINT

#define SCRPRINT_RINGSIZE (16)
#define SCRPRINT_BUFSIZE  (96)
int scrPrintTop = 0, scrPrintBottom = 0;
int scrPrintStepTicker = 60 * 5;
char scrPrintRingBuf[SCRPRINT_RINGSIZE][SCRPRINT_BUFSIZE];

int scrPrintHook(int fd, char* buf, int len)
{
	// add to our ring buf
	int n = len < SCRPRINT_BUFSIZE ? len : SCRPRINT_BUFSIZE;
	memcpy(scrPrintRingBuf[scrPrintTop], buf, n);
	scrPrintRingBuf[scrPrintTop][n] = 0;
	scrPrintTop = (scrPrintTop + 1) % SCRPRINT_RINGSIZE;

	// loop bottom to end of ring
	if (scrPrintBottom == scrPrintTop)
		scrPrintBottom = (scrPrintTop + 1) % SCRPRINT_RINGSIZE;

	return ((int (*)(int, char*, int))0x00127168)(fd, buf, len);
}

void handleScrPrint(void)
{
	int i = scrPrintTop;
	float x = 15, y = SCREEN_HEIGHT - 15;

	// hook
	*(u32*)0x00123D38 = 0x0C000000 | ((u32)&scrPrintHook >> 2);

	if (scrPrintTop != scrPrintBottom)
	{
		// draw
		do
		{
			i--;
			if (i < 0)
				i = SCRPRINT_RINGSIZE - 1;

			gfxScreenSpaceText(x+1, y+1, 0.7, 0.7, 0x80000000, scrPrintRingBuf[i], -1, 0);
			gfxScreenSpaceText(x, y, 0.7, 0.7, 0x80FFFFFF, scrPrintRingBuf[i], -1, 0);

			y -= 12;
		}
		while (i != scrPrintBottom);

		// move bottom to top
		if (scrPrintStepTicker == 0)
		{
			scrPrintBottom = (scrPrintBottom + 1) % SCRPRINT_RINGSIZE;
			scrPrintStepTicker = 60 * 5;
		}
		else
		{
			scrPrintStepTicker--;
		}
	}
}
#endif

/*
 * NAME :		drawHook
 * 
 * DESCRIPTION :
 * 			Logs time it takes to render a frame.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void drawHook(u64 a0)
{
	static int renderTimeCounterMs = 0;
	static int frames = 0;
	static long ticksIntervalStarted = 0;

	long t0 = timerGetSystemTime();
	((void (*)(u64))0x004c3240)(a0);
	long t1 = timerGetSystemTime();

	renderTimeMs = (t1-t0) / SYSTEM_TIME_TICKS_PER_MS;

	renderTimeCounterMs += renderTimeMs;
	frames++;

	// update every 500 ms
	if ((t1 - ticksIntervalStarted) > (SYSTEM_TIME_TICKS_PER_MS * 500))
	{
		averageRenderTimeMs = renderTimeCounterMs / (float)frames;
		renderTimeCounterMs = 0;
		frames = 0;
		ticksIntervalStarted = t1;
	}
}

/*
 * NAME :		updatePad
 * 
 * DESCRIPTION :
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void updatePad(struct PAD* pad, u8* rdata, int size, u32 a3)
{
  // need to run this before UpdatePad
  runSingletapChargeboot(pad, rdata);

	((void (*)(struct PAD*, u8*, int, u32))pad->RawPadInputCallback)(pad, rdata, size, a3);
}

/*
 * NAME :		updateHook
 * 
 * DESCRIPTION :
 * 			Logs time it takes to run an update.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void updateHook(void)
{
	static int updateTimeCounterMs = 0;
	static int frames = 0;
	static long ticksIntervalStarted = 0;

  // trigger config menu update
  if (playerGetNumLocals() > 1)
    onConfigGameMenu();

	long t0 = timerGetSystemTime();
	((void (*)(void))0x005986b0)();
	long t1 = timerGetSystemTime();

	updateTimeMs = (t1-t0) / SYSTEM_TIME_TICKS_PER_MS;

	updateTimeCounterMs += updateTimeMs;
	frames++;

	// update every 500 ms
	if ((t1 - ticksIntervalStarted) > (SYSTEM_TIME_TICKS_PER_MS * 500))
	{
		averageUpdateTimeMs = updateTimeCounterMs / (float)frames;
		updateTimeCounterMs = 0;
		frames = 0;
		ticksIntervalStarted = t1;
	}
}

/*
 * NAME :		onCustomModeDownloadInitiated
 * 
 * DESCRIPTION :
 * 			
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
int onCustomModeDownloadInitiated(void * connection, void * data)
{
	DPRINTF("requested mode binaries started %d\n", dlIsActive);
	redownloadCustomModeBinaries = 0;
	return 0;
}

/*
 * NAME :		sendClientType
 * 
 * DESCRIPTION :
 * 			
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void sendClientType(void)
{
  static int sendCounter = -1;
  
  ClientSetClientTypeRequest_t msg;

  // if the client type is 0 (normal)
  // then it is possible we're on an emulator
  // so let's check if we are on an emu
  if (interopData.Client == CLIENT_TYPE_NORMAL && !hasSonyMACAddress())
    interopData.Client = CLIENT_TYPE_PCSX2;

  // get client type
  msg.ClientType = interopData.Client;
  int accountId = gameGetMyAccountId();
  void* lobbyConnection = netGetLobbyServerConnection();

  // get mac address
  getMACAddress(msg.mac);

  // if we're not logged in, update last account id to -1
  if (accountId < 0) {
    lastAccountId = accountId;
  }

  // if we're not connected to the server
  // which can happen as we transition from lobby to game server
  // or vice-versa, or when we've logged out/disconnected
  // reset sendCounter, indicating it's time to recheck if we need to send the client type
  if (!lobbyConnection) {
    sendCounter = -1;
    return;
  }

  if (sendCounter > 0) {
    // tick down until we hit 0
    --sendCounter;
  } else if (sendCounter < 0) {

    // if relogged in
    // or client type has changed
    // trigger send in 60 ticks
    if (lobbyConnection && accountId > 0 && (lastAccountId != accountId || lastClientType != msg.ClientType)) {
      sendCounter = 60;
    }

  } else {

    lastClientType = msg.ClientType;
    lastAccountId = accountId;

    // keep trying to send until it succeeds
    if (netSendCustomAppMessage(NET_DELIVERY_CRITICAL, lobbyConnection, NET_LOBBY_CLIENT_INDEX, CUSTOM_MSG_ID_CLIENT_SET_CLIENT_TYPE, sizeof(ClientSetClientTypeRequest_t), &msg) != 0) {
      // failed, wait another second
      sendCounter = 60;
    } else {
      // success
      sendCounter = -1;
      DPRINTF("sent %d %d\n", gameGetTime(), msg.ClientType);
    }
  }

}

/*
 * NAME :		runPayloadDownloadRequester
 * 
 * DESCRIPTION :
 * 			
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void runPayloadDownloadRequester(void)
{
	GameModule * module = GLOBAL_GAME_MODULES_START;
	GameSettings* gs = gameGetSettings();
	if (!gs) {
		
    if (dlIsActive == 201) {
      dlIsActive = 0;
    }
    
    return;
  }

	// don't make any requests when the config menu is active
	if (gameAmIHost() && isConfigMenuActive)
		return;

	if (!dlIsActive) {
		// redownload when set mode doesn't match downloaded one
		if (!redownloadCustomModeBinaries && gameConfig.customModeId && (module->State == 0 || module->ModeId != gameConfig.customModeId)) {
			redownloadCustomModeBinaries = 1;
			DPRINTF("mode id %d != %d\n", gameConfig.customModeId, module->ModeId);
		}

		// redownload when set map doesn't match downloaded one
		// if (!redownloadCustomModeBinaries && module->State && module->MapId != gameConfig.customMapId) {
		// 	redownloadCustomModeBinaries = 1;
		// 	DPRINTF("map id %d != %d\n", gameConfig.customMapId, module->MapId);
		// }
    if (!redownloadCustomModeBinaries && patchStateContainer.SelectedCustomMapChanged) {
      redownloadCustomModeBinaries = 1;
    }

    // redownload if training mode changed
    if (!redownloadCustomModeBinaries && module->State && module->ModeId == CUSTOM_MODE_TRAINING && gameConfig.trainingConfig.type != module->Arg3) {
			redownloadCustomModeBinaries = 1;
			DPRINTF("training type id %d != %d\n", gameConfig.trainingConfig.type, module->Arg3);
    }

		// disable when module id doesn't match mode
		// unless mode is forced (negative mode)
		if (redownloadCustomModeBinaries == 1 || (module->State && !gameConfig.customModeId && module->ModeId >= 0)) {
			DPRINTF("disabling module mode:%d\n", module->ModeId);
			module->State = 0;
			memset((void*)(u32)0x000F0000, 0, 0xF000);
		}

		if (redownloadCustomModeBinaries == 1) {
			netSendCustomAppMessage(NET_DELIVERY_CRITICAL, netGetLobbyServerConnection(), NET_LOBBY_CLIENT_INDEX, CUSTOM_MSG_REQUEST_CUSTOM_MODE_PATCH, 0, &dlIsActive);
			redownloadCustomModeBinaries = 2;
			DPRINTF("requested mode binaries\n");
		}
	}

  patchStateContainer.SelectedCustomMapChanged = 0;
}

/*
 * NAME :		onBeforeVSync
 * 
 * DESCRIPTION :
 * 			
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void* onBeforeVSync(void)
{
  int hasDmeConnection = netGetDmeServerConnection() != 0;

  // manually call INetUpdate before and after vsync
  // to reduce latency for processing network messages
  // only works in game (with dme connection)
  if (hasDmeConnection) INetUpdate();

  // sceGsGetGParam
  return ((void* (*)(void))0x001384e0)();
}

#if PINGTEST
int onReceivePing(void* connection, void* data)
{
  struct PingRequest request;
  int clientId = gameGetMyClientId();

  memcpy(&request, data, sizeof(request));

  if (request.SourceClientId == clientId) {
    DPRINTF("client:%d rtt:%lld\n", request.ReturnedFromClientId, (timerGetSystemTime() - request.Time) / SYSTEM_TIME_TICKS_PER_MS);
  } else {
    request.ReturnedFromClientId = clientId;
    netSendCustomAppMessage(0, connection, request.SourceClientId, 150, sizeof(request), &request);
  }

  return sizeof(struct PingRequest);
}

void runLagTestLogic(void)
{
  struct PingRequest request;

  netInstallCustomMsgHandler(150, &onReceivePing);

  void* dmeConnection = netGetDmeServerConnection();
  if (dmeConnection && padGetButtonDown(0, PAD_UP | PAD_L1) > 0) {
    request.Time = timerGetSystemTime();
    request.SourceClientId = gameGetMyClientId();
    netBroadcastCustomAppMessage(0, dmeConnection, 150, sizeof(request), &request);
  }
}
#endif

/*
 * NAME :		onOnlineMenu
 * 
 * DESCRIPTION :
 * 			Called every ui update in menus.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
void onOnlineMenu(void)
{
  static int hasDevGameConfig = 0;
  char buf[64];

	// call normal draw routine
	((void (*)(void))0x00707F28)();

  // let the user read the EULA/Announcements without a big download bar appearing over it
  if (uiGetPointer(UI_MENU_ID_ONLINE_AGREEMENT_PAGE_1) == uiGetActivePointer()) return;
  if (uiGetPointer(UI_MENU_ID_ONLINE_AGREEMENT_PAGE_2) == uiGetActivePointer()) return;

	// 
	lastMenuInvokedTime = gameGetTime();

	//
	if (!hasInitialized)
	{
		padEnableInput();
		onConfigInitialize();
    PATCH_DZO_INTEROP_FUNCS = 0;
		memset(lobbyPlayerConfigs, 0, sizeof(lobbyPlayerConfigs));
		memset(&voteToEndState, 0, sizeof(voteToEndState));
		hasInitialized = 1;
	}

	// 
	if (hasInitialized == 1)
	{
		uiShowOkDialog("System", "Patch has been successfully loaded.");
		hasInitialized = 2;
	}

	// map loader
	onMapLoaderOnlineMenu();

#if COMP
	// run comp patch logic
	runCompMenuLogic();
#endif

  // banner
  bannerDraw();

	// settings
	onConfigOnlineMenu();

  // check if dev item is newly enabled
  // and we're host
  if (!netGetDmeServerConnection()) {
    hasDevGameConfig = 0;
  }
  else if (!isConfigMenuActive) {
    int lastHasDevConfig = hasDevGameConfig;
    hasDevGameConfig = gameConfig.drFreecam != 0;
    if (gameAmIHost() && hasDevGameConfig && !lastHasDevConfig) {
      uiShowOkDialog("Warning", "By enabling a dev rule this game will not count towards stats.");
    }
  }

  if (hasShownSurvivalPrestigeMessage == 1) {
    uiShowOkDialog("Survival", "You have reached the max rank. You may prestige using the !prestige command.");
    hasShownSurvivalPrestigeMessage = 2;
  }

	if (showNoMapPopup)
	{
		if (mapOverrideResponse == -1)
		{
			uiShowOkDialog("Custom Maps", "You have not installed the map modules.");
		}
		else
		{
#if MAPDOWNLOADER
			sprintf(buf, "Would you like to download the map now?");
			
			//uiShowOkDialog("Custom Maps", buf);
			if (uiShowYesNoDialog("Required Map Update", buf) == 1)
			{
				ClientInitiateMapDownloadRequest_t msg = {
					.MapId = (int)gameConfig.customMapId
				};
				netSendCustomAppMessage(NET_DELIVERY_CRITICAL, netGetLobbyServerConnection(), NET_LOBBY_CLIENT_INDEX, CUSTOM_MSG_ID_CLIENT_INITIATE_DOWNLOAD_MAP_REQUEST, sizeof(ClientInitiateMapDownloadRequest_t), &msg);
			}
#else
      int i;
      sprintf(buf, "Please install %s to play.", MapLoaderState.MapName);
			uiShowOkDialog("Custom Maps", buf);
#endif
		}

		showNoMapPopup = 0;
	}

	// 
	if (showNeedLatestMapsPopup)
	{
    sprintf(buf, "Please download the latest custom maps to play. %d %d", mapsLocalGlobalVersion, mapsRemoteGlobalVersion);
    uiShowOkDialog("Custom Maps", buf);

		showNeedLatestMapsPopup = 0;
	}

	if (showMiscPopup)
	{
		uiShowOkDialog(miscPopupTitle, miscPopupBody);
		showMiscPopup = 0;
	}
}

/*
 * NAME :		main
 * 
 * DESCRIPTION :
 * 			Applies all patches and modules.
 * 
 * NOTES :
 * 
 * ARGS : 
 * 
 * RETURN :
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
int main (void)
{
	int i;
  static Moby* mpMoby = NULL;
	
	// Call this first
	dlPreUpdate();

  //
  //if (padGetButtonDown(0, PAD_L1 | PAD_UP) > 0) {
  //  POKE_U32(0x0036D664, gameGetTime() + (TIME_SECOND * 1020));
  //}

	// auto enable pad input to prevent freezing when popup shows
	if (lastMenuInvokedTime > 0 && gameGetTime() - lastMenuInvokedTime > TIME_SECOND)
	{
		padEnableInput();
		lastMenuInvokedTime = 0;
	}

	//
	if (!hasInitialized)
	{
		DPRINTF("patch loaded\n");
		onConfigInitialize();
		hasInitialized = 1;

		if (isInGame()) {
			uiShowPopup(0, "Patch has been successfully loaded.");
			hasInitialized = 2;
		}
	}

  // write patch pointers
  PATCH_INTEROP = &interopData;

	// invoke exception display installer
	if (*(u32*)EXCEPTION_DISPLAY_ADDR != 0)
	{
		if (!hasInstalledExceptionHandler)
		{
			((void (*)(void))EXCEPTION_DISPLAY_ADDR)();
			hasInstalledExceptionHandler = 1;
		}
		
		// change display to match progressive scan resolution
		if (IS_PROGRESSIVE_SCAN)
		{
			*(u16*)(EXCEPTION_DISPLAY_ADDR + 0x9F4) = 0x0083;
			*(u16*)(EXCEPTION_DISPLAY_ADDR + 0x9F8) = 0x210E;
		}
		else
		{
			*(u16*)(EXCEPTION_DISPLAY_ADDR + 0x9F4) = 0x0183;
			*(u16*)(EXCEPTION_DISPLAY_ADDR + 0x9F8) = 0x2278;
		}
	}

#if SCR_PRINT
	handleScrPrint();
#endif

	// install net handlers
	netInstallCustomMsgHandler(CUSTOM_MSG_ID_SERVER_REQUEST_TEAM_CHANGE, &onSetTeams);
	netInstallCustomMsgHandler(CUSTOM_MSG_ID_SERVER_SET_LOBBY_CLIENT_PATCH_CONFIG_REQUEST, &onSetLobbyClientPatchConfig);
	netInstallCustomMsgHandler(CUSTOM_MSG_ID_SERVER_SET_RANKS, &onSetRanks);
	netInstallCustomMsgHandler(CUSTOM_MSG_RESPONSE_CUSTOM_MODE_PATCH, &onCustomModeDownloadInitiated);
	netInstallCustomMsgHandler(CUSTOM_MSG_CLIENT_READY, &onClientReadyRemote);
	netInstallCustomMsgHandler(CUSTOM_MSG_PLAYER_VOTED_TO_END, &onClientVoteToEndRemote);
	netInstallCustomMsgHandler(CUSTOM_MSG_VOTE_TO_END_STATE_UPDATED, &onClientVoteToEndStateUpdateRemote);
	netInstallCustomMsgHandler(CUSTOM_MSG_ID_SERVER_SET_LOBBY_NAME_OVERRIDES, &onServerSetLobbyNameOverridesRemote);
  netInstallCustomMsgHandler(CUSTOM_MSG_ID_SERVER_DATE_TIME_RESPONSE, &onServerDateTimeResponseRemote);
	netInstallCustomMsgHandler(CUSTOM_MSG_B6_BALL_FIRED, &onB6BallFiredRemote);

  // dzo cosmetics are sent by dzo clients
  // the dzo patch handles it for us
  // but for non-dzo clients we need to handle it so that the size is properly returned
  if (interopData.Client != CLIENT_TYPE_DZO) {
	  netInstallCustomMsgHandler(CUSTOM_MSG_DZO_COSMETICS_UPDATE, &onSetPlayerDzoCosmeticsRemote);
  }

  // banner
  bannerTick();

	// Run map loader
	runMapLoader();

#if PINGTEST
  runLagTestLogic();
#endif

  // install our vsync hook
  if (isUnloading) POKE_U32(0x00138d7c, 0x0C04E138);
  else HOOK_JAL(0x00138d7c, &onBeforeVSync);

  // force to 5 ms
  //patchAggTime(5);

#if COMP
	// run comp patch logic
	runCompLogic();
#endif

#if TEST
	// run test patch logic
	runTestLogic();
#endif
  
#if LEVELHOP
  {
    if (padGetButtonDown(0, PAD_L1 | PAD_UP) > 0) {
      int mapId = 48;

      //POKE_U32(0x0021de80, 4);
      POKE_U32(0x0021e6a4, 6);
      POKE_U32(0x005A90F4, 0x24020001);
      //POKE_U32(0x00220250, 1);

      strncpy(MapLoaderState.MapName, "Orxon", sizeof(MapLoaderState.MapName));
      strncpy(MapLoaderState.MapFileName, "survival v2 mf", sizeof(MapLoaderState.MapFileName));
      MapLoaderState.Enabled = 1;
      MapLoaderState.CheckState = 0;
      MapLoaderState.MapId = mapId;
      MapLoaderState.LoadingFd = -1;
      MapLoaderState.LoadingFileSize = -1;

      ((void (*)(int mapId, int bSave, int missionId))0x004e2410)(mapId, 1, -1);
      
      DPRINTF("load %d\n", mapId);
    }
  }
#endif

  //
  sendClientType();

	// 
	runCheckGameMapInstalled();

	// Run game start messager
	runGameStartMessager();

#if SCAVENGER_HUNT
  // scavenger hunt
  scavHuntRun();
#endif

  if (config.enableFastLoad) {
    runFastLoad();
  }

  // enable 4 player splitscreen
  //extraLocalsRun();

  // enable in game scoreboard
  igScoreboardRun();

  // enable in game quick chat
  quickChatRun();

  // old lag fixes
  if (!gameConfig.grNewPlayerSync) {
    runPlayerPositionSmooth();
    //runPlayerStateSync();
    runCorrectPlayerChargebootRotation();
  }

	// 
	runFpsCounter();

  // 
  runVoteToEndLogic();

	// Run add singleplayer music
  runEnableSingleplayerMusic();

	// detects when to download a new custom mode patch
	runPayloadDownloadRequester();

  // sends client ready state to others in lobby when we load the level
  // ensures our local copy of who is ready is reset when loading a new lobby
  runClientReadyMessager();

	// Patch camera speed
	patchCameraSpeed();

	// Patch announcements
	patchAnnouncements();

	// Patch create game settings load
	patchGameSettingsLoad();

	// Patch populate create game
	patchPopulateCreateGame();

	// Patch save create game settings
	patchCreateGame();

	// Patch frame skip
	patchFrameSkip();

  // Toggle aim assist
  patchAimAssist();

	// Patch shots to be less laggy
	patchWeaponShotLag();

	// Ensures that player's don't lag through the flag
	runFlagPickupFix();

  // 
  //runHealthPickupFix();

  //
  patchFlagCaptureMessage();

	// Patch state update to run more optimized
	patchStateUpdate();

	// Patch radar scale
	patchRadarScale();

	// Patch process level call
	patchProcessLevel();

	// Patch kill stealing
	patchKillStealing();

	// Patch resurrect weapon ordering
	patchResurrectWeaponOrdering();

	// Patch camera shake
	patchCameraShake();

  //
  patchFusionReticule();

  //
  patchCycleOrder();

	// 
	//patchWideStats();

  // 
  patchComputePoints();

  // 
  patchFov();

	//
	patchLevelOfDetail();

	// 
	patchStateContainer.UpdateCustomGameStats = processSendGameData();

	// 
	patchStateContainer.UpdateGameState = runSendGameUpdate();

	// config update
	onConfigUpdate();

	// in game stuff
	if (isInGame())
	{
		// hook render function
		HOOK_JAL(0x004A84B0, &updateHook);
		HOOK_JAL(0x004A9C10, &updateHook);
		HOOK_JAL(0x004C3A94, &drawHook);
		HOOK_JAL(0x004A9A48, &drawHook);
    HOOK_JAL(0x005281F0, &updatePad);
      
    // fix weird overflow caused by player sync
    // also randomly (rarely) triggered by other things too
    POKE_U32(0x004BAD64, 0x00412023);
    POKE_U32(0x004b8078, 0x00412023);
    POKE_U32(0x004b8084, 0x00612023);
    POKE_U32(0x004b80a0, 0x00622023);

    // allow local flinching before remote flinch for chargebooting targets
    POKE_U32(0x005E1C94, 0);

    // immediately join global chat when game ends
    if (gameHasEnded()) {
      voiceEnableGlobalChat(1);
    }

    // prevents wrench lag
    // by patching 1 frame where mag shot won't stop player when cbooting
    //POKE_U16(0x003EF658, 0x0017);

    // hook Pad_MappedPad
    //HOOK_J(0x005282d8, &padMappedPadHooked);
    //POKE_U32(0x005282dc, 0);

    // disable guber wait for dispatchTime
    POKE_U32(0x00611518, 0x24040000);

    // find and hook multiplayer moby
    if (!mpMoby) {
      mpMoby = mobyFindNextByOClass(mobyListGetStart(), 0x106A);
      if (mpMoby) {
        mpMoby->PUpdate = &onMobyUpdate;
      }
    } else if (isUnloading && mpMoby) {
      mpMoby->PUpdate = NULL;
    }

		// reset when in game
		hasSendReachedEndScoreboard = 0;

	#if DEBUG
		if (padGetButtonDown(0, PAD_L3 | PAD_R3) > 0)
		{
			gameEnd(0);
		}
	#endif

		//
		grGameStart();

		// this lets guber events that are < 5 seconds old be processed (original is 1.2 seconds)
		//GADGET_EVENT_MAX_TLL = 5 * TIME_SECOND;

		// put hacker ray in weapon select
		GameSettings * gameSettings = gameGetSettings();
		if (gameSettings && gameSettings->GameRules == GAMERULE_CQ)
		{
			// put hacker ray in weapon select
			*(u32*)0x0038A0DC = WEAPON_ID_HACKER_RAY;

			// disable/enable press circle to equip hacker ray
			*(u32*)0x005DE870 = config.disableCircleToHackerRay ? 0x24040000 : 0x00C0202D;
		}

    // increase cboot max slope
    POKE_U16(0x00608CD0, 0x3F40);

		// close config menu on transition to lobby
		if (lastGameState != 1)
			configMenuDisable();

		// Hook game start menu back callback
		if (*(u32*)0x005605D4 == 0x0C15803E)
		{
			*(u32*)0x005605D4 = 0x0C000000 | ((u32)&patchStartMenuBack_Hook / 4);
		}

		// patch red and brown as last two color codes
		*(u32*)0x00391978 = COLOR_CODE_EX1;
		*(u32*)0x0039197C = COLOR_CODE_EX2;

		// if survivor is enabled then set the respawn time to -1
		GameOptions* gameOptions = gameGetOptions();
	  if (gameOptions && gameOptions->GameFlags.MultiplayerGameFlags.Survivor)
			gameOptions->GameFlags.MultiplayerGameFlags.RespawnTime = 0xFF;

    // trigger config menu update
    if (playerGetNumLocals() == 1)
      onConfigGameMenu();

#if MAPEDITOR
		// trigger map editor update
		onMapEditorGameUpdate();
#endif

    // run net update after vsync when in game
    INetUpdate();

		lastGameState = 1;
	}
	else if (isInMenus())
	{
		// render ms isn't important in the menus so assume best case scenario of 0ms
		renderTimeMs = 0;
		averageRenderTimeMs = 0;
		updateTimeMs = 0;
		averageUpdateTimeMs = 0;

    mpMoby = NULL;
    
		//
		grLobbyStart();
    runShowCustomMapModeInSearch();
		patchStagingRankNumber();

    // enable selecting any vehicle for all maps
    HOOK_J(0x00764e48, &getMapVehiclesEnabledTable);
    POKE_U32(0x00764e4c, 0);
    
    // tick player sync
    // normally is only called in game
    // this lets it handle out of game logic
    playerSyncTick();

		// Hook menu loop
		if (*(u32*)0x00594CBC == 0)
			*(u32*)0x00594CB8 = 0x0C000000 | ((u32)(&onOnlineMenu) / 4);

		// send patch game config on create game
		GameSettings * gameSettings = gameGetSettings();
		if (gameSettings && gameSettings->GameLoadStartTime < 0)
		{
			// if host and just entered staging, send patch game config
			if (gameAmIHost() && !isInStaging)
			{
				// copy over last game config as host
				memcpy(&gameConfig, &gameConfigHostBackup, sizeof(PatchGameConfig_t));
        patchStateContainer.SelectedCustomMapChanged = selectedMapIdHostBackup != patchStateContainer.SelectedCustomMapId;
        patchStateContainer.SelectedCustomMapId = selectedMapIdHostBackup;

				// send
				configTrySendGameConfig();
			}

      if (gameAmIHost()) {
        gfxScreenSpaceText(SCREEN_WIDTH - 20, 0, 0.7, 0.7, 0x80FFFFFF, "Press START to configure custom game rules.", -1, 2);
      }

      if (hasPendingLobbyNameOverrides) forceLobbyNameOverrides();

			// try and apply ranks
			/*
			if (hasSetRanks && hasSetRanks != gameSettings->PlayerCount)
			{
				for (i = 0; i < GAME_MAX_PLAYERS; ++i)
				{
					int accountId = lastSetRanksRequest.AccountIds[i];
					if (accountId >= 0)
					{
						for (j = 0; j < GAME_MAX_PLAYERS; ++j)
						{
							if (gameSettings->PlayerAccountIds[j] == accountId)
							{
								gameSettings->PlayerRanks[j] = lastSetRanksRequest.Ranks[i];
							}
						}
					}
				}

				hasSetRanks = gameSettings->PlayerCount;
			}
			*/

			isInStaging = 1;
		}
		else
		{
			isInStaging = 0;
			//hasSetRanks = 0;
		}

		// send game reached end scoreboard
		if (!hasSendReachedEndScoreboard)
		{
			if (gameSettings && gameSettings->GameStartTime > 0 && uiGetActive() == 0x15C && gameAmIHost())
			{
				hasSendReachedEndScoreboard = 1;
				netSendCustomAppMessage(NET_DELIVERY_CRITICAL, netGetLobbyServerConnection(), NET_LOBBY_CLIENT_INDEX, CUSTOM_MSG_ID_GAME_LOBBY_REACHED_END_SCOREBOARD, 0, NULL);
			}
			else if (!gameSettings)
			{
				// disable if not in a lobby (must be in a lobby to send)
				hasSendReachedEndScoreboard = 1;
			}
		}

		// patch server hostname
		/*
		if (0)
		{
			char * muisServerHostname = (char*)0x001B1ECD;
			char * serverHostname = (char*)0x004BF4F0;
			if (!gameSettings && strlen(muisServerHostname) > 0)
			{
				for (i = 0; i < 32; ++i)
				{
					char c = muisServerHostname[i];
					if (c < 0x20)
						c = '.';
					serverHostname[i] = c;
				}
			}
		}
		*/

		// patch red and brown as last two color codes
		*(u32*)0x004C8A68 = COLOR_CODE_EX1;
		*(u32*)0x004C8A6C = COLOR_CODE_EX2;
		
		// close config menu on transition to lobby
		if (lastGameState != 0)
			configMenuDisable();

		lastGameState = 0;
	}

	// Process spectate
	if (config.enableSpectate)
		processSpectate();
  else
    PATCH_POINTERS_SPECTATE = 0;

  // Process freecam
  if (isInGame() && gameConfig.drFreecam) {
	  processFreecam();
  } else {
    resetFreecam();
  }

	// Process game modules
	processGameModules();

	//
	if (patchStateContainer.UpdateGameState) {
		patchStateContainer.UpdateGameState = 0;
		netSendCustomAppMessage(NET_DELIVERY_CRITICAL, netGetLobbyServerConnection(), NET_LOBBY_CLIENT_INDEX, CUSTOM_MSG_ID_CLIENT_SET_GAME_STATE, sizeof(UpdateGameStateRequest_t), &patchStateContainer.GameStateUpdate);
	}

	// 
	if (patchStateContainer.UpdateCustomGameStats) {
		patchStateContainer.UpdateCustomGameStats = 0;
		sendGameData();
	}

	// Call this last
	dlPostUpdate();

	return 0;
}
