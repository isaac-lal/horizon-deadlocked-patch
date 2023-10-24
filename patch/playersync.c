#include <libdl/game.h>
#include <libdl/utils.h>
#include <libdl/player.h>
#include <libdl/net.h>
#include <libdl/stdio.h>
#include <libdl/math.h>
#include <libdl/math3d.h>
#include <libdl/radar.h>
#include <libdl/graphics.h>

#include "config.h"
#include "include/config.h"

#define CMD_BUFFER_SIZE           (64)

typedef struct PlayerSyncStateUpdateUnpacked
{
  VECTOR Position;
  VECTOR Rotation;
  VECTOR CameraPosition;
  int GameTime;
  float CameraYaw;
  float CameraPitch;
  float Health;
  short NoInput;
  u8 MoveX;
  u8 MoveY;
  u8 State;
  char StateId;
  char PlayerIdx;
  char Valid;
  u8 CmdId;
} PlayerSyncStateUpdateUnpacked_t;

typedef struct PlayerSyncPlayerData
{
  VECTOR LastReceivedPosition;
  int LastNetTime;
  int LastState;
  int LastStateTime;
  int TicksSinceLastUpdate;
  char LastStateId;
  char Pad[32];
  u8 StateUpdateCmdId;
  PlayerSyncStateUpdateUnpacked_t StateUpdates[CMD_BUFFER_SIZE];
} PlayerSyncPlayerData_t;


// config
extern PatchConfig_t config;

// game config
extern PatchGameConfig_t gameConfig;

PlayerSyncPlayerData_t playerSyncDatas[GAME_MAX_PLAYERS];

//--------------------------------------------------------------------------
int playerSyncCmdDelta(int fromCmdId, int toCmdId)
{
  int delta = toCmdId - fromCmdId;
  if (delta < -(CMD_BUFFER_SIZE/2))
    delta += CMD_BUFFER_SIZE;
  else if (delta > (CMD_BUFFER_SIZE/2))
    delta -= CMD_BUFFER_SIZE;

  return delta;
}

//--------------------------------------------------------------------------
float playerSyncLerpAngleIfDelta(float from, float to, float lerpAmount, float minAngleBetween)
{
  float dt = clampAngle(from - to);
  //if (dt < minAngleBetween) return to;

  return lerpfAngle(from, to, lerpAmount);
}

//--------------------------------------------------------------------------
int playerSyncHandlePlayerPadHook(Player* player)
{
  int padIdx = 0;

  // enable pad
  if (!player->IsLocal) {

    // get player sync data
    PlayerSyncPlayerData_t* data = &playerSyncDatas[player->PlayerId];
  
    // process input
    ((void (*)(struct PAD*))0x00527e08)(player->Paddata);

    // update pad
    ((void (*)(struct PAD*, void*, int))0x00527510)(player->Paddata, data->Pad, 0x14);
  }

  int result = ((int (*)(Player*))0x0060cec0)(player);

  return result;
}

//--------------------------------------------------------------------------
void playerSyncHandlePlayerState(Player* player)
{
  MATRIX m, mInv;
  VECTOR dt;
  int i;
  if (!player || player->IsLocal || !player->PlayerMoby) return;

  PlayerSyncPlayerData_t* data = &playerSyncDatas[player->PlayerId];
  PlayerSyncStateUpdateUnpacked_t* stateCurrent = &data->StateUpdates[data->StateUpdateCmdId];
  if (!stateCurrent->Valid) return;
  
  PlayerVTable* vtable = playerGetVTable(player);
  float tPos = 0.15;
  float tRot = 0.15;
  int padIdx = 0;

  // reset pad
  data->Pad[2] = 0xFF;
  data->Pad[3] = 0xFF;

  // set no input
  if (data->TicksSinceLastUpdate == 0) {
    player->timers.noInput = stateCurrent->NoInput;
  }

  // extrapolate
  if (data->TicksSinceLastUpdate > 0 && !playerIsDead(player)) {
    vector_add(stateCurrent->Position, stateCurrent->Position, player->Velocity);
  }

  // snap position
  vector_subtract(dt, data->LastReceivedPosition, player->PlayerPosition);
  if (vector_sqrmag(dt) > (5*5)) {
    vector_copy(player->PlayerPosition, data->LastReceivedPosition);
    vector_copy(stateCurrent->Position, data->LastReceivedPosition);
  }

  // lerp position
  vector_lerp(player->PlayerPosition, player->PlayerPosition, stateCurrent->Position, tPos);
  vector_copy(player->PlayerMoby->Position, player->PlayerPosition);
  vector_copy(player->RemoteHero.receivedSyncPos, player->PlayerPosition);
  vector_copy(player->RemoteHero.posAtSyncFrame, player->PlayerPosition);
  vector_copy(player->pNetPlayer->pNetPlayerData->vPosition, player->PlayerPosition);

  // lerp rotation
  //printf("%f %f %f\n", stateCurrent->Rotation[0], stateCurrent->Rotation[1], stateCurrent->Rotation[2]);
  player->PlayerRotation[0] = playerSyncLerpAngleIfDelta(player->PlayerRotation[0], stateCurrent->Rotation[0], tRot, 2/255.0);
  player->PlayerRotation[1] = playerSyncLerpAngleIfDelta(player->PlayerRotation[1], stateCurrent->Rotation[1], tRot, 2/255.0);
  player->PlayerRotation[2] = playerSyncLerpAngleIfDelta(player->PlayerRotation[2], stateCurrent->Rotation[2], tRot, 2/255.0);
  vector_copy(player->RemoteHero.receivedSyncRot, player->PlayerRotation);

  // lerp camera position
  vector_lerp(player->CameraPos, player->CameraPos, stateCurrent->CameraPosition, tPos);
  vector_copy(&player->CameraMatrix[12], player->CameraPos);
  vector_copy((float*)((u32)player + 0x2cd0), player->CameraPos);

  // lerp camera rotations
  player->CameraYaw.Value = lerpfAngle(player->CameraYaw.Value, stateCurrent->CameraYaw, tRot);
  player->CameraPitch.Value = lerpfAngle(player->CameraPitch.Value, stateCurrent->CameraPitch, tRot);

  // compute matrix
  matrix_unit(m);
  matrix_rotate_y(m, m, player->CameraPitch.Value);
  matrix_rotate_z(m, m, player->CameraYaw.Value);
  vector_copy(player->CameraForward, &m[0]);
  vector_copy(player->CameraDir, player->CameraForward);
  vector_copy((float*)((u32)player + 0x590 + 0x40), player->CameraForward);

  // compute inv matrix
  matrix_unit(mInv);
  matrix_rotate_y(mInv, mInv, clampAngle(player->CameraPitch.Value + MATH_PI));
  matrix_rotate_z(mInv, mInv, clampAngle(player->CameraYaw.Value + MATH_PI));
  memcpy((void*)((u32)player + 0x2cf0), mInv, sizeof(VECTOR)*3);

  // set camera rotation
  VECTOR camRot;
  camRot[1] = player->CameraPitch.Value;
  camRot[2] = player->CameraYaw.Value;
  camRot[0] = 0;
  vector_copy((float*)((u32)player + 0x2ce0), camRot);

  // set health
  player->Health = stateCurrent->Health;

  // set net camera rotation
  if (player->pNetPlayer) {
    vector_pack(camRot, player->pNetPlayer->padMessageElems[padIdx].msg.cameraRot);
    //player->pNetPlayer->padMessageElems[padIdx].msg.playerPos
  }

  // set joystick
  float moveX = (stateCurrent->MoveX - 127) / 128.0;
  float moveY = (stateCurrent->MoveY - 127) / 128.0;
  float mag = sqrtf((moveX*moveX) + (moveY*moveY));
  float ang = atan2f(moveY, moveX);
  *(float*)((u32)player + 0x2e08) = mag;
  *(float*)((u32)player + 0x2e0c) = ang;
  *(float*)((u32)player + 0x2e38) = mag;
  *(float*)((u32)player + 0x0120) = moveX;
  *(float*)((u32)player + 0x0124) = -moveY;
  *(float*)((u32)player + 0x17b0) = -moveY;

  // 
  player->RemoteHero.stateAtSyncFrame = player->PlayerState;
  player->RemoteHero.receivedState = stateCurrent->State;

  struct tNW_Player* netPlayer = player->pNetPlayer;
  if (netPlayer) {
    netPlayer->padMessageElems[padIdx].msg.pad_data[4] = 0x7F;
    netPlayer->padMessageElems[padIdx].msg.pad_data[5] = 0x7F;
    netPlayer->padMessageElems[padIdx].msg.pad_data[6] = stateCurrent->MoveX;
    netPlayer->padMessageElems[padIdx].msg.pad_data[7] = stateCurrent->MoveY;
  }

  // update state
  if (stateCurrent->StateId != data->LastStateId) {
    int skip = 0;
    int forced = 0;
    int playerState = player->PlayerState;

    // from
    switch (playerState)
    {
      case PLAYER_STATE_VEHICLE:
      {
        // leave vehicle
        if (stateCurrent->State != PLAYER_STATE_VEHICLE && player->Vehicle) {
          player->Vehicle->justExited = 1;
          if (player->Vehicle->pDriver == player) player->Vehicle->pDriver = 0;
          if (player->Vehicle->pPassenger == player) player->Vehicle->pPassenger = 0;
          player->InVehicle = 0;
          player->Vehicle = 0;
        }
        break;
      }
      case PLAYER_STATE_SWING:
      {
        // if we're swinging, don't force out of it
        skip = 1;
        break;
      }
      case PLAYER_STATE_GET_HIT:
      {
        // if we're getting hit, don't force out of it
        skip = 1;
        break;
      }
      case PLAYER_STATE_DEATH:
      case PLAYER_STATE_DROWN:
      case PLAYER_STATE_DEATH_FALL:
      case PLAYER_STATE_DEATHSAND_SINK:
      case PLAYER_STATE_LAVA_DEATH:
      case PLAYER_STATE_DEATH_NO_FALL:
      case PLAYER_STATE_WAIT_FOR_RESURRECT:
      {
        if (stateCurrent->State != playerState) {
          player->PlayerState = 0;
          forced = 1;
        }
        break;
      }
    }

    // to
    switch (stateCurrent->State)
    {
      case PLAYER_STATE_JUMP_ATTACK:
      case PLAYER_STATE_COMBO_ATTACK:
      {
        //DPRINTF("wrench %d: pstate:%d pstatetime:%d lasttime:%d\n", gameGetTime(), playerState, player->timers.state, data->LastStateTime);
        skip = 1;
        if (stateCurrent->State == playerState && player->timers.state < data->LastStateTime) {
          data->LastStateId = stateCurrent->StateId;
          data->LastState = stateCurrent->State;
        } else if ((player->timers.state % 10) != 0) {
          data->Pad[3] &= ~0x80;
        }
        break;
      }
      case PLAYER_STATE_GET_HIT:
      {
        skip = 1;
        break;
      }
      case PLAYER_STATE_SWING:
      {
        data->Pad[3] &= ~0x08;
        skip = 1;
        break;
      }
    }

    if (!skip) {
      DPRINTF("%d new state %d (from %d)\n", player->PlayerId, stateCurrent->State, player->PlayerState);
      vtable->UpdateState(player, stateCurrent->State, 1, 1, 1);
      data->LastStateId = stateCurrent->StateId;
      data->LastState = stateCurrent->State;
    } else {
      data->LastStateTime = player->timers.state;
    }
  }

  //
  if (player->pNetPlayer && player->pNetPlayer->pNetPlayerData) {
    player->pNetPlayer->pNetPlayerData->lastKeepAlive = data->LastNetTime;
    player->pNetPlayer->pNetPlayerData->timeStamp = data->LastNetTime;
    player->pNetPlayer->pNetPlayerData->hitPoints = stateCurrent->Health;
  }
  data->TicksSinceLastUpdate += 1;
}

//--------------------------------------------------------------------------
int playerSyncOnReceivePlayerState(void* connection, void* data)
{
  if (!isInGame()) return sizeof(PlayerSyncStateUpdatePacked_t);
  if (!gameConfig.grNewPlayerSync) return sizeof(PlayerSyncStateUpdatePacked_t);

  PlayerSyncStateUpdatePacked_t msg;
  PlayerSyncStateUpdateUnpacked_t unpacked;
  memcpy(&msg, data, sizeof(msg));

  // unpack
  memcpy(unpacked.Position, msg.Position, sizeof(float) * 3);
  memcpy(unpacked.CameraPosition, msg.CameraPosition, sizeof(float) * 3);
  memcpy(unpacked.Rotation, msg.Rotation, sizeof(float) * 3);
  unpacked.GameTime = msg.GameTime;
  unpacked.CameraYaw = msg.CameraYaw / 10240.0;
  unpacked.CameraPitch = msg.CameraPitch / 10240.0;
  unpacked.NoInput = msg.NoInput;
  unpacked.Health = msg.Health;
  unpacked.MoveX = msg.MoveX;
  unpacked.MoveY = msg.MoveY;
  unpacked.State = msg.State;
  unpacked.StateId = msg.StateId;
  unpacked.PlayerIdx = msg.PlayerIdx;
  unpacked.CmdId = msg.CmdId;
  unpacked.Valid = 1;

  // move into buffer
  memcpy(&playerSyncDatas[msg.PlayerIdx].StateUpdates[msg.CmdId], &unpacked, sizeof(unpacked));
  int cmdDt = playerSyncCmdDelta(playerSyncDatas[unpacked.PlayerIdx].StateUpdateCmdId, unpacked.CmdId);
  //DPRINTF("%d => %d (%d)\n", playerSyncDatas[unpacked.PlayerIdx].StateUpdateCmdId, unpacked.CmdId, cmdDt);
  if (cmdDt > 0) {
    playerSyncDatas[unpacked.PlayerIdx].LastNetTime = unpacked.GameTime;
    playerSyncDatas[unpacked.PlayerIdx].StateUpdateCmdId = unpacked.CmdId;
    playerSyncDatas[unpacked.PlayerIdx].TicksSinceLastUpdate = 0;
    vector_copy(playerSyncDatas[unpacked.PlayerIdx].LastReceivedPosition, unpacked.Position);
  }

  //DPRINTF("recv player sync state %d time:%d dt:%d\n", msg.PlayerIdx, msg.GameTime, msg.GameTime - gameGetTime());
  return sizeof(PlayerSyncStateUpdatePacked_t);
}

//--------------------------------------------------------------------------
void playerSyncBroadcastPlayerState(Player* player)
{
  PlayerSyncStateUpdatePacked_t msg;
  void * connection = netGetDmeServerConnection();
  if (!connection || !player || !player->IsLocal) return;

  PlayerSyncPlayerData_t* data = &playerSyncDatas[player->PlayerId];

  // set cur frame
  player->LocalHero.UNK_LOCALHERO[0x1EE] = 0;

  // detect state change
  if (data->LastState != player->PlayerState || player->timers.state < data->LastStateTime) {
    data->LastState = player->PlayerState;
    data->LastStateId = (data->LastStateId + 1) % 256;
  }
  data->LastStateTime = player->timers.state;

  memcpy(msg.Position, player->PlayerPosition, sizeof(float) * 3);
  memcpy(msg.CameraPosition, player->CameraPos, sizeof(float) * 3);
  memcpy(msg.Rotation, player->PlayerRotation, sizeof(float) * 3);
  msg.GameTime = data->LastNetTime = gameGetTime();
  msg.PlayerIdx = player->PlayerId;
  msg.CameraYaw = (short)(player->CameraYaw.Value * 10240.0);
  msg.CameraPitch = (short)(player->CameraPitch.Value * 10240.0);
  msg.NoInput = player->timers.noInput;
  msg.Health = (short)player->Health;
  msg.MoveX = ((struct PAD*)player->Paddata)->rdata[6];
  msg.MoveY = ((struct PAD*)player->Paddata)->rdata[7];
  msg.State = player->PlayerState;
  msg.StateId = data->LastStateId;
  msg.CmdId = data->StateUpdateCmdId = (data->StateUpdateCmdId + 1) % CMD_BUFFER_SIZE;
  
  netBroadcastCustomAppMessage(0, connection, CUSTOM_MSG_PLAYER_SYNC_STATE_UPDATE, sizeof(msg), &msg);
}

//--------------------------------------------------------------------------
int playerSyncDisablePlayerStateUpdates(void)
{
  return 0;
}

//--------------------------------------------------------------------------
void playerSyncTick(void)
{
  int i;
  if (!isInGame()) {
    memset(playerSyncDatas, 0, sizeof(playerSyncDatas));
    return;
  }

  // net
  netInstallCustomMsgHandler(CUSTOM_MSG_PLAYER_SYNC_STATE_UPDATE, &playerSyncOnReceivePlayerState);

  if (!gameConfig.grNewPlayerSync) return;
  
  // hooks
  HOOK_JAL(0x0060eb80, &playerSyncDisablePlayerStateUpdates);
  HOOK_JAL(0x0060684c, &playerSyncHandlePlayerPadHook);

  // player link always healthy
  POKE_U32(0x005F7BDC, 0x24020001);

  // player updates
  Player** players = playerGetAll();
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    playerSyncBroadcastPlayerState(players[i]);
    playerSyncHandlePlayerState(players[i]);
  }
}
