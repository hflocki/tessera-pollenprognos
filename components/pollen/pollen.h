#pragma once
#include <cmath>
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/smart_display/plugin_api.h"

namespace esphome::pollen {

// The allergens, in the order of the season (the same order as ALLERGENS in __init__.py).
static constexpr int ALLERGENS = 8;
// Today, tomorrow and the day after.
static constexpr int DAYS = 3;

// The plugin: one tile type, "forecast". It holds the Home Assistant sensors plugin.yaml reads, three per allergen.
class Pollen : public Component, public tessera::Plugin {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_sensor(int allergen, int day, sensor::Sensor *s) {
    if (allergen >= 0 && allergen < ALLERGENS && day >= 0 && day < DAYS) sensors_[allergen][day] = s;
  }
  // The level of an allergen on a day, 0 to 3 in steps of 0.5; NaN while it has none (no sensor, unavailable).
  float level(int allergen, int day) const {
    const sensor::Sensor *s = sensors_[allergen][day];
    return s && s->has_state() ? s->state : NAN;
  }

 protected:
  sensor::Sensor *sensors_[ALLERGENS][DAYS]{};
};

}  // namespace esphome::pollen
