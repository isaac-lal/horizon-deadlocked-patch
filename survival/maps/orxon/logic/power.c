/***************************************************
 * FILENAME :		power.c
 * 
 * DESCRIPTION :
 * 		Handles logic for power.
 * 		
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */

#include <tamtypes.h>

#include <libdl/dl.h>
#include <libdl/player.h>
#include <libdl/pad.h>
#include <libdl/time.h>
#include <libdl/net.h>
#include <libdl/game.h>
#include <libdl/string.h>
#include <libdl/math.h>
#include <libdl/math3d.h>
#include <libdl/stdio.h>
#include <libdl/gamesettings.h>
#include <libdl/dialog.h>
#include <libdl/patch.h>
#include <libdl/ui.h>
#include <libdl/graphics.h>
#include <libdl/color.h>
#include <libdl/utils.h>
#include "module.h"
#include "messageid.h"
#include "../../shared/include/maputils.h"

int powerTimeOff = -1;
int powerForcedOn = 0;

//--------------------------------------------------------------------------
void powerUpdateMobies(int powerOn)
{
  Moby* m = mobyListGetStart();
  Moby* mEnd = mobyListGetEnd();

  while (m < mEnd)
  {
    if (!mobyIsDestroyed(m))
    {
      switch (m->OClass)
      {
        case MOBY_ID_JUMP_PAD:
        {
          m->State = powerOn ? 3 : 1;
          break;
        }
        case MOBY_ID_PICKUP_PAD:
        {
          u32 v = *(u32*)m->PVar;
          if (powerOn) {
            *(u32*)m->PVar = v &= 0xFF;
          } else {
            *(u32*)m->PVar = v |= 0x10000000;
          }
          break;
        }
      }
    }

    ++m;
  }
}

//--------------------------------------------------------------------------
void powerOnMysteryBoxActivatePower(void)
{
  powerForcedOn = 1;
  powerTimeOff = gameGetTime() + 5*TIME_MINUTE;

  pushSnack(-1, "Power supercharged", 60);
}

//--------------------------------------------------------------------------
void powerNodeUpdate(Moby* moby)
{
  static int initialized = 0;
  if (!moby || !moby->PVar)
    return;

  // default to power off
  if (!initialized) {

    // disable jump pad from setting jump height to 0 when deactivated
    POKE_U32(0x004257D8, 0);
    POKE_U32(0x00425930, 0);

    // disable pickup pad not being pickupable in CQ
    POKE_U32(0x00446638, 0);

    //
    powerUpdateMobies(0);
    initialized = 1;
  }

  // get bolt crank moby from node pvars
  Moby* boltCrank = *(Moby**)((u32)moby->PVar + 0xC);
  if (!boltCrank || !boltCrank->PVar)
    return;

  // get team from bolt crank pvars
  // if team is 10 then power is waiting to be activated
  // otherwise power is active and we want to turn it off after a period of time
  int *team = (int*)((u32)boltCrank->PVar + 4);
  if (*team == 10) {
    if (powerForcedOn && powerTimeOff >= 0) {
      powerUpdateMobies(1);
      Player* lp = playerGetFromSlot(0);
      if (lp)
        *team = lp->Team;
      else
        *team = 0;
    }
    else {
      powerTimeOff = -1;
    }
  } else if (powerTimeOff >= 0) {
    int timeSincePowerOn = gameGetTime() - powerTimeOff;
    if (timeSincePowerOn > 0) {
      powerForcedOn = 0;
      *team = 10;
      powerUpdateMobies(0);
      pushSnack(-1, "Power's out", 60);
    }
  } else {
    powerTimeOff = gameGetTime() + TIME_SECOND*90;
    powerUpdateMobies(1);
    pushSnack(-1, "Power activated!", 60);
  }
}