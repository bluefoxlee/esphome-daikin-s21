#pragma once

#include <cstdint>
#include "esphome/core/string_ref.h"

namespace esphome {
namespace daikin_s21 {

enum class DaikinFanMode : uint8_t {
  Auto = 'A',
  Silent = 'B',
  Speed1 = '3',
  Speed2 = '4',
  Speed3 = '5',
  Speed4 = '6',
  Speed5 = '7',
};

static constexpr DaikinFanMode supported_daikin_fan_modes[] = {
  DaikinFanMode::Auto,
  DaikinFanMode::Silent,
  DaikinFanMode::Speed1,
  DaikinFanMode::Speed2,
  DaikinFanMode::Speed3,
  DaikinFanMode::Speed4,
  DaikinFanMode::Speed5,
};

constexpr StringRef daikin_fan_mode_to_string_ref(const DaikinFanMode mode) {
  switch (mode) {
    case DaikinFanMode::Auto:
    default:
      return StringRef::from_lit("Automatic");
    case DaikinFanMode::Silent:
      return StringRef::from_lit("Silent");
    case DaikinFanMode::Speed1:
      return StringRef::from_lit("1");
    case DaikinFanMode::Speed2:
      return StringRef::from_lit("2");
    case DaikinFanMode::Speed3:
      return StringRef::from_lit("3");
    case DaikinFanMode::Speed4:
      return StringRef::from_lit("4");
    case DaikinFanMode::Speed5:
      return StringRef::from_lit("5");
  }
}

constexpr DaikinFanMode string_to_daikin_fan_mode(const std::string &mode) {
  for (const auto supported_mode : supported_daikin_fan_modes) {
    if (daikin_fan_mode_to_string_ref(supported_mode) == mode) {
      return supported_mode;
    }
  }
  return DaikinFanMode::Auto;
}

}  // namespace daikin_s21
}  // namespace esphome
