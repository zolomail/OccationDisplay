#pragma once

#include <Arduino.h>
#include <ctime>
#include <utility>
#include <vector>
#include "Scene.h"

struct OccasionEntry {
    String label;
    bool   recurring{false};

    // DateRange (month 1..12, day 1..31, hour 0..23, min 0..59)
    uint8_t start_month{1};
    uint8_t start_day{1};
    uint8_t start_hour{0};
    uint8_t start_min{0};
    uint8_t end_month{12};
    uint8_t end_day{31};
    uint8_t end_hour{23};
    uint8_t end_min{59};

    // Reoccuring (weekdays_mask bit i = tm_wday i: 0=Sun..6=Sat)
    uint8_t weekdays_mask{0x7F};
    uint8_t from_hour{0};
    uint8_t from_min{0};
    uint8_t to_hour{23};
    uint8_t to_min{59};

    SceneId scene_id{SceneId::NormalDays};
    std::vector<std::pair<String, String>> params;
};

namespace Occasions {

void begin();
void resetToDefaults();

std::vector<OccasionEntry> getAll();
void setAll(std::vector<OccasionEntry> entries);

// Evaluates entries in priority order (index 0 = highest).
// Falls back to SceneId::NormalDays with empty params if none match.
SceneId resolveActive(const tm& t, std::vector<std::pair<String, String>>& out_params);

String toJson();
bool   fromJson(const String& json);

} // namespace Occasions
