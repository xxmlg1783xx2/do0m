/*
========================================================================

                           D O O M  R e t r o
         The classic, refined DOOM source port. For Windows PC.

========================================================================

  Copyright © 1993-2022 by id Software LLC, a ZeniMax Media company.
  Copyright © 2013-2022 by Brad Harding <mailto:brad@doomretro.com>.

  DOOM Retro is a fork of Chocolate DOOM. For a list of credits, see
  <https://github.com/bradharding/doomretro/wiki/CREDITS>.

  This file is a part of DOOM Retro.

  DOOM Retro is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the
  Free Software Foundation, either version 3 of the license, or (at your
  option) any later version.

  DOOM Retro is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with DOOM Retro. If not, see <https://www.gnu.org/licenses/>.

  DOOM is a registered trademark of id Software LLC, a ZeniMax Media
  company, in the US and/or other countries, and is used without
  permission. All other trademarks are the property of their respective
  holders. DOOM Retro is in no way affiliated with nor endorsed by
  id Software.

========================================================================
*/

#include "c_console.h"
#include "i_gamecontroller.h"
#include "m_config.h"
#include "m_misc.h"

dboolean                    gp_analog = gp_analog_default;
float                       gp_deadzone_left = gp_deadzone_left_default;
float                       gp_deadzone_right = gp_deadzone_right_default;
dboolean                    gp_invertyaxis = gp_invertyaxis_default;
int                         gp_rumble_barrels = gp_rumble_barrels_default;
int                         gp_rumble_damage = gp_rumble_damage_default;
int                         gp_rumble_weapons = gp_rumble_weapons_default;
int                         gp_sensitivity_horizontal = gp_sensitivity_horizontal_default;
int                         gp_sensitivity_vertical = gp_sensitivity_vertical_default;
dboolean                    gp_swapthumbsticks = gp_swapthumbsticks_default;
int                         gp_thumbsticks = gp_thumbsticks_default;

static SDL_Joystick         *joystick;
static SDL_GameController   *gamecontroller;

int                         gamecontrollerbuttons = 0;
short                       gamecontrollerthumbLX = 0;
short                       gamecontrollerthumbLY = 0;
short                       gamecontrollerthumbRX = 0;
short                       gamecontrollerthumbRY = 0;
float                       gamecontrollerhorizontalsensitivity;
float                       gamecontrollerverticalsensitivity;
short                       gamecontrollerleftdeadzone;
short                       gamecontrollerrightdeadzone;

int                         barrelrumbletics = 0;
int                         damagerumbletics = 0;
int                         weaponrumbletics = 0;
int                         idlerumblestrength;
int                         restorerumblestrength;

void I_InitGameController(void)
{
    SDL_SetHintWithPriority(SDL_HINT_JOYSTICK_HIDAPI_JOY_CONS, "1", SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1", SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE, "1", SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1", SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_LINUX_JOYSTICK_DEADZONES, "1", SDL_HINT_OVERRIDE);

    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) < 0)
        C_Warning(1, "Controller support couldn't be initialized.");
    else
    {
        for (int i = 0, numjoysticks = SDL_NumJoysticks(); i < numjoysticks; i++)
            if ((joystick = SDL_JoystickOpen(i)) && SDL_IsGameController(i))
            {
                gamecontroller = SDL_GameControllerOpen(i);
                break;
            }

        if (!gamecontroller)
            SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
        else
        {
            const char  *name = SDL_GameControllerName(gamecontroller);

            if (*name)
                C_Output("A controller called \"%s\" is connected.", name);
            else
                C_Output("A controller is connected.");

            if (gp_rumble_barrels || gp_rumble_damage || gp_rumble_weapons)
            {
                if (SDL_GameControllerRumble(gamecontroller, 0, 0, 0) == -1)
                    C_Warning(1, "This controller doesn't support rumble.");
            }

            I_SetGameControllerLeftDeadZone();
            I_SetGameControllerRightDeadZone();
            I_SetGameControllerHorizontalSensitivity();
            I_SetGameControllerVerticalSensitivity();
        }
    }
}

void I_ShutdownGameController(void)
{
    if (!gamecontroller)
        return;

    SDL_GameControllerClose(gamecontroller);
    gamecontroller = NULL;

    SDL_JoystickClose(joystick);
    joystick = NULL;

    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
}

void I_GameControllerRumble(int strength)
{
    static int  currentstrength;

    if (!strength || (lasteventtype == ev_gamecontroller && (strength == idlerumblestrength || strength >= currentstrength)))
    {
        currentstrength = MIN(strength, UINT16_MAX);
        SDL_GameControllerRumble(gamecontroller, strength, strength, 600000);
    }
}

void I_UpdateGamepadRumble(void)
{
    if (weaponrumbletics && !--weaponrumbletics && !damagerumbletics && !barrelrumbletics)
        I_GameControllerRumble(idlerumblestrength);
    else if (damagerumbletics && !--damagerumbletics && !barrelrumbletics)
        I_GameControllerRumble(idlerumblestrength);
    else if (barrelrumbletics && !--barrelrumbletics)
        I_GameControllerRumble(idlerumblestrength);
}

void I_StopGameControllerRumble(void)
{
    SDL_GameControllerRumble(gamecontroller, 0, 0, 0);
}

void I_SetGameControllerHorizontalSensitivity(void)
{
    gamecontrollerhorizontalsensitivity = (!gp_sensitivity_horizontal ? 0.0f :
        4.0f * gp_sensitivity_horizontal / gp_sensitivity_horizontal_max + 0.2f);
}

void I_SetGameControllerVerticalSensitivity(void)
{
    gamecontrollerverticalsensitivity = (!gp_sensitivity_vertical ? 0.0f :
        4.0f * gp_sensitivity_vertical / gp_sensitivity_vertical_max + 0.2f);
}

void I_SetGameControllerLeftDeadZone(void)
{
    gamecontrollerleftdeadzone = (short)(gp_deadzone_left * SHRT_MAX / 100.0f);
}

void I_SetGameControllerRightDeadZone(void)
{
    gamecontrollerrightdeadzone = (short)(gp_deadzone_right * SHRT_MAX / 100.0f);
}