/**
* File: backpackLightAnimation.h
*
* Authors: Kevin M. Karol
* Created: 6/21/18
*
* Description: Definitions for building/parsing backpack lights
*
* Copyright: Anki, Inc. 2018
* 
**/


#ifndef __Anki_Cozmo_BackpackLights_BackpackLightAnimation_H__
#define __Anki_Cozmo_BackpackLights_BackpackLightAnimation_H__

#include "clad/types/ledTypes.h"
#include "clad/robotInterface/messageEngineToRobot.h"
#include "coretech/common/shared/types.h"
#include <array>

namespace Json {
  class Value;
}

namespace Anki {
namespace Vector {
namespace Anim {

namespace BackpackLightAnimation {

  // Native light parameters with optional timing for groups of synchronized pulses.
  struct BackpackAnimation {
    RobotInterface::SetBackpackLights lights;

    // Leave both fields at zero for continuous looping.
    u32 pulseCount = 0;
    u32 pausePeriod_ms = 0;

    bool IsInPause(TimeStamp_t elapsed_ms) const;
  };
  
  bool DefineFromJSON(const Json::Value& jsonDef, BackpackAnimation& outAnim);

} // namespace BackpackLightAnimation

} // namespace Anim
} // namespace Vector
} // namespace Anki

#endif // __Anki_Cozmo_BackpackLights_BackpackLightAnimation_H__

