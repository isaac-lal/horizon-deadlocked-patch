/***************************************************
 * FILENAME :		module.h
 * 
 * DESCRIPTION :
 * 		Modules are a way to optimize the limited memory available on the PS2.
 *      Modules are dynamically loaded by the server and dynamically invoked by the patcher.
 *      This header file contains the relevant structues for patch modules.
 * 
 * NOTES :
 * 		Each offset is determined per app id.
 * 		This is to ensure compatibility between versions of Deadlocked/Gladiator.
 * 		
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */

#ifndef _MODULE_H_
#define _MODULE_H_

#include <tamtypes.h>
#include <libdl/gamesettings.h>
#include "config.h"
#include "messageid.h"

// Forward declarations
struct GameModule;
struct PatchStateContainer;

/*
 * NAME :		GameModuleState
 * 
 * DESCRIPTION :
 * 			Contains the different states for a game module.
 *          The state will define how the patcher handles the module.
 * 
 * NOTES :
 * 
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
typedef enum GameModuleState
{
    /*
     * Module is not active.
     */
    GAMEMODULE_OFF,

    /*
     * The module will be invoked when the game/lobby starts and
     * it will be set to 'OFF' when the game/lobby ends.
     */
    GAMEMODULE_TEMP_ON,

    /*
     * The module will always be invoked as long as the player
     * is in a game.
     */
    GAMEMODULE_ALWAYS_ON
} GameModuleState;

/*
 * NAME :		GameModuleContext
 * 
 * DESCRIPTION :
 * 			
 * 
 * NOTES :
 * 
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
typedef enum GameModuleContext
{
    GAMEMODULE_LOBBY,
    GAMEMODULE_LOAD,
    GAMEMODULE_GAME_FRAME,
    GAMEMODULE_GAME_UPDATE,
} GameModuleContext;

/*
 * NAME :		ModuleStart
 * 
 * DESCRIPTION :
 * 			Defines the function pointer for all module entrypoints.
 *          Modules will provide a pointer to their entrypoint that will match
 *          this type.
 * 
 * NOTES :
 * 
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
typedef void (*ModuleStart)(struct GameModule * module, struct PatchStateContainer * patchStateContainer, enum GameModuleContext context);

/*
 * NAME :		ReadExtraData_f
 * 
 * DESCRIPTION :
 * 			Function that reads the custom map extra data from the usb drive for the current game mode.
 *      Returns 1 on success, 0 on failure.
 * 
 * NOTES :
 * 
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
typedef int (*ReadExtraData_f)(void* dst, int len);


/*
 * NAME :		GameModule
 * 
 * DESCRIPTION :
 * 			A game module is a dynamically loaded and invoked program.
 *          It contains a state and an entrypoint pointer.
 * 
 * NOTES :
 *          Game modules are only executed while the player is in a game.
 * 
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */
typedef struct GameModule
{
    /*
     * State of the module.
     */
    char State;

    /*
     * Respective custom game mode id.
     */
    char ModeId;

    /*
     * Argument 2, can be anything related to the custom mode.
     */
    char Arg2;

    /*
     * Argument 3, can be anything related to the custom mode.
     */
    char Arg3;

    /*
     * Entrypoint of module to be invoked by the patch.
     */
    ModuleStart Entrypoint;

} GameModule;


typedef struct UpdateGameStateRequest {
	char TeamsEnabled;
  char PADDING;
  short Version;
	int RoundNumber;
	int TeamScores[GAME_MAX_PLAYERS];
	char ClientIds[GAME_MAX_PLAYERS];
	char Teams[GAME_MAX_PLAYERS];
} UpdateGameStateRequest_t;

typedef struct CustomGameModeStats
{
  u8 Payload[1024 * 6];
} __attribute__((aligned(16))) CustomGameModeStats_t;

typedef struct PatchStateContainer
{
  PatchConfig_t* Config;
  PatchGameConfig_t* GameConfig;
  int UpdateGameState;
  UpdateGameStateRequest_t GameStateUpdate;
  int UpdateCustomGameStats;
  CustomGameModeStats_t CustomGameStats;
  GameSettings GameSettingsAtStart;
  int CustomGameStatsSize;
  int ClientsReadyMask;
  int AllClientsReady;
  int VoteToEndPassed;
  int HalfTimeState;
  int OverTimeState;
  SetNameOverridesMessage_t LobbyNameOverrides;
  int SelectedCustomMapId;
  int SelectedCustomMapChanged;
  ReadExtraData_f ReadExtraDataFunc;
} PatchStateContainer_t;


#endif // _MODULE_H_
