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

#include "engine/components/backpackLights/engineBackpackLightAnimation.h"
#include "coretech/common/engine/jsonTools.h"

namespace Anki {
namespace Vector {
namespace BackpackLightAnimation {

const BackpackAnimation& GetGoHomeAnimation()
{
  static const BackpackAnimation animation = {
    .onColors               = {{0x0040FF00, 0x0040FF00, 0x0040FF00}},
    .offColors              = {{0x00000800, 0x00000800, 0x00000800}},
    .onPeriod_ms            = {{30, 30, 30}},
    .offPeriod_ms           = {{180, 180, 180}},
    .transitionOnPeriod_ms  = {{30, 30, 30}},
    .transitionOffPeriod_ms = {{120, 120, 120}},
    .offset                 = {{0, 120, 240}}
  };
  return animation;
}


bool DefineFromJSON(const Json::Value& jsonDef, BackpackAnimation& outAnim)
{  
  bool res = true;
  res &= JsonTools::GetColorValuesToArrayOptional(jsonDef, "onColors",  outAnim.onColors, true);
  res &= JsonTools::GetColorValuesToArrayOptional(jsonDef, "offColors", outAnim.offColors, true);
  res &= JsonTools::GetArrayOptional(jsonDef, "onPeriod_ms",            outAnim.onPeriod_ms);
  res &= JsonTools::GetArrayOptional(jsonDef, "offPeriod_ms",           outAnim.offPeriod_ms);
  res &= JsonTools::GetArrayOptional(jsonDef, "transitionOnPeriod_ms",  outAnim.transitionOnPeriod_ms);
  res &= JsonTools::GetArrayOptional(jsonDef, "transitionOffPeriod_ms", outAnim.transitionOffPeriod_ms);
  res &= JsonTools::GetArrayOptional(jsonDef, "offset",                 outAnim.offset);
  return res;
}

} // namespace BackpackLightAnimation
} // namespace Vector
} // namespace Anki
