/**
* File: backpackLightAnimation.cpp
*
* Authors: Kevin M. Karol
* Created: 6/21/18
*
* Description: Definitions for building/parsing backpack lights
*
* Copyright: Anki, Inc. 2018
*
**/

#include "cozmoAnim/backpackLights/animBackpackLightAnimation.h"
#include "coretech/common/engine/jsonTools.h"

namespace Anki {
namespace Vector {
namespace Anim {
namespace BackpackLightAnimation {

bool BackpackAnimation::IsInPause(TimeStamp_t elapsed_ms) const
{
  if (pulseCount == 0 || pausePeriod_ms == 0) {
    return false;
  }

  const auto& light = lights.lights[0];
  const u64 pulsePeriod_ms = light.transitionOnPeriod_ms + light.onPeriod_ms +
                            light.transitionOffPeriod_ms + light.offPeriod_ms;
  const u64 activePeriod_ms = pulsePeriod_ms * pulseCount;
  return activePeriod_ms > 0 &&
         (elapsed_ms % (activePeriod_ms + pausePeriod_ms)) >= activePeriod_ms;
}

bool DefineFromJSON(const Json::Value& jsonDef, BackpackAnimation& outAnim)
{
  outAnim.pulseCount = 0;
  outAnim.pausePeriod_ms = 0;

#define ARRAY_TO_LIGHTSTATE(arr, lightState, lightStateField) \
  {                                                           \
    for(int i = 0; i < arr.size(); ++i) {                     \
      lightState[i].lightStateField = arr[i];                 \
    }                                                         \
  }

  // Json definitions have an individual array for each field but
  // we actually store it as a single array of a struct containing all fields
  // so we need to convert between the two
  bool res = true;
  std::array<u32, (int)LEDId::NUM_BACKPACK_LEDS> arr;
  res &= JsonTools::GetColorValuesToArrayOptional(jsonDef, "onColors", arr, true);
  ARRAY_TO_LIGHTSTATE(arr, outAnim.lights.lights, onColor);
  
  res &= JsonTools::GetColorValuesToArrayOptional(jsonDef, "offColors", arr, true);
  ARRAY_TO_LIGHTSTATE(arr, outAnim.lights.lights, offColor);

  res &= JsonTools::GetArrayOptional(jsonDef, "onPeriod_ms", arr);
  ARRAY_TO_LIGHTSTATE(arr, outAnim.lights.lights, onPeriod_ms);

  res &= JsonTools::GetArrayOptional(jsonDef, "offPeriod_ms", arr);
  ARRAY_TO_LIGHTSTATE(arr, outAnim.lights.lights, offPeriod_ms);

  res &= JsonTools::GetArrayOptional(jsonDef, "transitionOnPeriod_ms", arr);
  ARRAY_TO_LIGHTSTATE(arr, outAnim.lights.lights, transitionOnPeriod_ms);

  res &= JsonTools::GetArrayOptional(jsonDef, "transitionOffPeriod_ms", arr);
  ARRAY_TO_LIGHTSTATE(arr, outAnim.lights.lights, transitionOffPeriod_ms);

  res &= JsonTools::GetArrayOptional(jsonDef, "offset", arr);
  ARRAY_TO_LIGHTSTATE(arr, outAnim.lights.lights, offset_ms);

  if (res && (jsonDef.isMember("pulseCount") || jsonDef.isMember("pausePeriod_ms"))) {
    const auto& pulseCount = jsonDef["pulseCount"];
    const auto& pausePeriod = jsonDef["pausePeriod_ms"];
    if (!pulseCount.isUInt() || !pausePeriod.isUInt() ||
        pulseCount.asUInt() == 0 || pausePeriod.asUInt() == 0) {
      return false;
    }

    // All lights must share a pulse period and start together so the pause follows complete pulses.
    u32 pulsePeriod_ms = 0;
    for (size_t i = 0; i < (size_t)LEDId::NUM_BACKPACK_LEDS; ++i) {
      const auto& light = outAnim.lights.lights[i];
      const u32 period_ms = light.transitionOnPeriod_ms + light.onPeriod_ms +
                            light.transitionOffPeriod_ms + light.offPeriod_ms;
      if (period_ms == 0 || light.offset_ms != 0 || (i > 0 && period_ms != pulsePeriod_ms)) {
        return false;
      }
      pulsePeriod_ms = period_ms;
    }

    outAnim.pulseCount = pulseCount.asUInt();
    outAnim.pausePeriod_ms = pausePeriod.asUInt();
  }

  return res;

  #undef ARRAY_TO_LIGHTSTATE
}

} // namespace BackpackLightAnimation
} // namespace Anim
} // namespace Vector
} // namespace Anki
