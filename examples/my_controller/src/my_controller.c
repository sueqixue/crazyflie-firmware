/**
 * ,---------,       ____  _ __
 * |  ,-^-,  |      / __ )(_) /_______________ _____  ___
 * | (  O  ) |     / __  / / __/ ___/ ___/ __ `/_  / / _ \
 * | / ,--´  |    / /_/ / / /_/ /__/ /  / /_/ / / /_/  __/
 *    +------`   /_____/_/\__/\___/_/   \__,_/ /___/\___/
 *
 * Crazyflie control firmware
 *
 * Copyright (C) 2019 Bitcraze AB
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, in version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Modified based on push.c
 */
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
// #include <stdio.h>

#include "app.h"

#include "FreeRTOS.h"
#include "task.h"

#include "controller.h"
#include "controller_pid.h"

#include "commander.h"

#include "FreeRTOS.h"
#include "task.h"

#include "debug.h"
#define DEBUG_MODULE "MYCONTROLLER"

#include "log.h"
#include "param.h"
#include "crtp_commander_high_level.h"

/*
static void setHoverSetpointV(setpoint_t *setpoint, float vx, float vy, float vz, float yawrate)
{
  setpoint->mode.x = modeVelocity;
  setpoint->mode.y = modeVelocity;
  setpoint->mode.z = modeVelocity;
  
  setpoint->velocity.x = vx;
  setpoint->velocity.y = vy;
  setpoint->velocity.z = vz;

  setpoint->mode.yaw = modeVelocity;
  setpoint->attitudeRate.yaw = yawrate;

  setpoint->velocity_body = true;
}
*/

static void setHoverSetpointP(setpoint_t *setpoint, float vx, float vy, float z, float yawrate)
{
  setpoint->mode.x = modeVelocity;
  setpoint->mode.y = modeVelocity;
  setpoint->mode.z = modeAbs;
  
  setpoint->velocity.x = vx;
  setpoint->velocity.y = vy;
  setpoint->position.z = z;

  setpoint->mode.yaw = modeVelocity;
  setpoint->attitudeRate.yaw = yawrate;

  setpoint->velocity_body = true;
}
typedef enum {
    idle,
    lowUnlock,
    unlocked,
    stopping,
    hovering,
    p_controling,
    v_controling                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      
} State;

static State state = idle;

// static bool sleeping = true;

static const uint16_t unlockThLow = 100;
static const uint16_t unlockThHigh = 300;
static const uint16_t stoppedTh = 500;

static const float velMax = 1.0f;
static const uint16_t radius = 300;

static const float height_sp = 0.3f;

#define MAX(a,b) ((a>b)?a:b)
#define MIN(a,b) ((a<b)?a:b)

void appMain() {
  static setpoint_t setpoint;

  DEBUG_PRINT("\n\n\n\nWaiting for activation ...\n");
  vTaskDelay(M2T(5000));
  
  logVarId_t idUp = logGetVarId("range", "up");
  // logVarId_t idLeft = logGetVarId("range", "left");
  // logVarId_t idRight = logGetVarId("range", "right");
  // logVarId_t idFront = logGetVarId("range", "front");
  // logVarId_t idBack = logGetVarId("range", "back");

  // float factor = velMax/radius;

  // Taking off
  DEBUG_PRINT("Taking off ...\n");
  // int t_err = crtpCommanderHighLevelTakeoffWithVelocity(1.0, 0.1, false);
  // DEBUG_PRINT("t_err=%d\n", t_err);
  for (int i = 0; i < 30; i++) {
    DEBUG_PRINT("i=%d\n", i);
    // setHoverSetpointV(&setpoint, 0.0f,  0.0f, 0.02f, 0.0f);
    float height = 0.02 * i;
    setHoverSetpointP(&setpoint, 0.0f,  0.0f, (double) height, 0.0f);
    vTaskDelay(M2T(10));
  }

  // Landing
  DEBUG_PRINT("Landing ...\n");
  for (int i = 0; i < 30; i++) {
    float height = 0.6 - 0.02 * i;
    setHoverSetpointP(&setpoint, 0.0f,  0.0f, (double) height, 0.0f);
    commanderSetSetpoint(&setpoint, 3);
    vTaskDelay(M2T(10));
  }

  // Hovering
  while (1) {
    vTaskDelay(M2T(10));

    uint16_t up = logGetUint(idUp);
    
    // DEBUG_PRINT("up=%i\n", up);

    if (state == unlocked) {
      uint16_t up_o = radius - MIN(up, radius);
      float height = height_sp - up_o/1000.0f;

      DEBUG_PRINT("up_o=%i, height=%f\n", up_o, (double)height);

      if (1) {
        setHoverSetpointP(&setpoint, 0, 0, (double)height, 0);
        commanderSetSetpoint(&setpoint, 3);
      }

      if (height < 0.1f) {
        state = stopping;
        DEBUG_PRINT("X\n");
      }
    } else {
      if (state == idle) {
        memset(&setpoint, 0, sizeof(setpoint_t));
        commanderSetSetpoint(&setpoint, 3);
      }
    }
    
    /*
    if (state == unlocked) {
      DEBUG_PRINT("Unlocked ...\n");
      uint16_t left = logGetUint(idLeft);
      uint16_t right = logGetUint(idRight);
      uint16_t front = logGetUint(idFront);
      uint16_t back = logGetUint(idBack);

      uint16_t left_o = radius - MIN(left, radius);
      uint16_t right_o = radius - MIN(right, radius);
      float l_comp = (-1) * left_o * factor;
      float r_comp = right_o * factor;
      float velSide = r_comp + l_comp;

      uint16_t front_o = radius - MIN(front, radius);
      uint16_t back_o = radius - MIN(back, radius);
      float f_comp = (-1) * front_o * factor;
      float b_comp = back_o * factor;
      float velFront = b_comp + f_comp;

      uint16_t up_o = radius - MIN(up, radius);
      float height = height_sp - up_o/1000.0f;

      DEBUG_PRINT("l=%i, r=%i, lo=%f, ro=%f, vel=%f\n", left_o, right_o, l_comp, r_comp, velSide);
      DEBUG_PRINT("f=%i, b=%i, fo=%f, bo=%f, vel=%f\n", front_o, back_o, f_comp, b_comp, velFront);
      DEBUG_PRINT("u=%i, d=%i, height=%f\n", up_o, height);

      if (1) {
        setHoverSetpoint(&setpoint, velFront, velSide, height, 0);
        commanderSetSetpoint(&setpoint, 3);
      }

      if (height < 0.1f) {
        state = stopping;
        DEBUG_PRINT("X\n");
      }

    } else {

      if (state == stopping && up > stoppedTh) {
        DEBUG_PRINT("Stopping ...\n");
        DEBUG_PRINT("%i", up);
        state = idle;
        DEBUG_PRINT("S\n");
      }

      if (up < unlockThLow && state == idle && up > 0.001f) {
        DEBUG_PRINT("Waiting for hand to be removed!\n");
        state = lowUnlock;
      }

      if (up > unlockThHigh && state == lowUnlock) {
        DEBUG_PRINT("Unlocked!\n");
        state = unlocked;
      }

      if (state == idle || state == stopping) {
        DEBUG_PRINT("Idle or stopping...\n");
        memset(&setpoint, 0, sizeof(setpoint_t));
        commanderSetSetpoint(&setpoint, 3);
      }
    }
    */
  }
}

void controllerOutOfTreeInit() {
  // Initialize your controller data here...
  // Call the PID controller instead in this example to make it possible to fly
  controllerPidInit();
}

bool controllerOutOfTreeTest() {
  // Always return true
  return true;
}

void controllerOutOfTree(control_t *control, const setpoint_t *setpoint, const sensorData_t *sensors, const state_t *state, const uint32_t tick) {
  // Implement your controller here...

  // Call the PID controller instead in this example to make it possible to fly
  controllerPid(control, setpoint, sensors, state, tick);
}
