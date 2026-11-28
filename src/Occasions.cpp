#include "Occasions.h"

#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace {

constexpr char PREFS_NS[] = "occasions";

SemaphoreHandle_t          s_mutex = nullptr;
std::vector<OccasionEntry> s_entries;

void ensureMutex()
{
    if (!s_mutex) {
        s_mutex = xSemaphoreCreateMutex();
    }
}

OccasionEntry makeDateEntry(
    const char* label,
    uint8_t sm, uint8_t sd, uint8_t sh, uint8_t smin,
    uint8_t em, uint8_t ed, uint8_t eh, uint8_t emin,
    SceneId sid)
{
    OccasionEntry e;
    e.label       = String(label).substring(0, 24);
    e.recurring   = false;
    e.start_month = sm;
    e.start_day   = sd;
    e.start_hour  = sh;
    e.start_min   = smin;
    e.end_month   = em;
    e.end_day     = ed;
    e.end_hour    = eh;
    e.end_min     = emin;
    e.scene_id    = sid;
    return e;
}

std::vector<OccasionEntry> buildDefaultEntries()
{
    std::vector<OccasionEntry> list;
    list.reserve(10);
    list.push_back(makeDateEntry("New Year",      1,  1, 0, 0,  1,  1, 11, 59, SceneId::NewYearsMidnight));
    list.push_back(makeDateEntry("January",       1,  1, 0, 0,  1, 31, 23, 59, SceneId::JanuaryDays));
    list.push_back(makeDateEntry("Valentines",    2, 14, 0, 0,  2, 14, 23, 59, SceneId::Aquatic));
    list.push_back(makeDateEntry("St Patrick's Day", 3, 17, 0, 0, 3, 17, 23, 59, SceneId::StPatricksDay));
    {
        OccasionEntry easter;
        easter.label             = "Easter";
        easter.easter_offset     = true;
        easter.easter_start_days = -3;
        easter.easter_end_days   = 1;
        easter.scene_id          = SceneId::Easter;
        list.push_back(std::move(easter));
    }
    list.push_back(makeDateEntry("Halloween",    10, 16, 0, 0, 11,  1,  7, 59, SceneId::Halloween));
    list.push_back(makeDateEntry("November Snow",11,  1, 0, 0, 11, 30, 23, 59, SceneId::JustSnow));
    list.push_back(makeDateEntry("Christmas Eve",12, 24, 0, 0, 12, 24, 23, 59, SceneId::ChristmasEve));
    list.push_back(makeDateEntry("New Years Eve",12, 31, 0, 0, 12, 31, 23, 59, SceneId::NewYearsDay));
    list.push_back(makeDateEntry("Christmas",    12,  1, 0, 0, 12, 31, 23, 59, SceneId::ChristmasDays));
    return list;
}

void getEasterSunday(int year, uint8_t& month, uint8_t& day)
{
    const int a = year % 19;
    const int b = year / 100;
    const int c = year % 100;
    const int d = b / 4;
    const int e = b % 4;
    const int f = (b + 8) / 25;
    const int g = (b - f + 1) / 3;
    const int h = (19 * a + b - d - g + 15) % 30;
    const int i = c / 4;
    const int k = c % 4;
    const int l = (32 + 2 * e + 2 * i - h - k) % 7;
    const int m = (a + 11 * h + 22 * l) / 451;
    const int n = h + l - 7 * m + 114;
    month = static_cast<uint8_t>(n / 31);
    day   = static_cast<uint8_t>((n % 31) + 1);
}

void resolveEasterOffsetDate(int year, int8_t offset_days, uint8_t& out_month, uint8_t& out_day)
{
    uint8_t em = 3, ed = 21;
    getEasterSunday(year, em, ed);
    tm t = {};
    t.tm_year  = year - 1900;
    t.tm_mon   = em - 1;
    t.tm_mday  = ed + offset_days;
    t.tm_hour  = 12;
    t.tm_isdst = -1;
    mktime(&t);
    out_month = static_cast<uint8_t>(t.tm_mon + 1);
    out_day   = static_cast<uint8_t>(t.tm_mday);
}

uint32_t packDateTime(uint8_t m, uint8_t d, uint8_t h, uint8_t min)
{
    return (static_cast<uint32_t>(m) << 20) |
           (static_cast<uint32_t>(d) << 12) |
           (static_cast<uint32_t>(h) << 6)  |
            static_cast<uint32_t>(min);
}

bool matchesEntry(const OccasionEntry& e, const tm& t)
{
    const uint8_t month = static_cast<uint8_t>(t.tm_mon + 1);
    const uint8_t day   = static_cast<uint8_t>(t.tm_mday);
    const uint8_t hour  = static_cast<uint8_t>(t.tm_hour);
    const uint8_t min   = static_cast<uint8_t>(t.tm_min);

    uint8_t sm = e.start_month;
    uint8_t sd = e.start_day;
    uint8_t em = e.end_month;
    uint8_t ed = e.end_day;
    if (e.easter_offset) {
        const int year = t.tm_year + 1900;
        resolveEasterOffsetDate(year, e.easter_start_days, sm, sd);
        resolveEasterOffsetDate(year, e.easter_end_days, em, ed);
    }

    const uint32_t cur   = packDateTime(month, day, hour, min);
    const uint32_t start = packDateTime(sm, sd, e.start_hour, e.start_min);
    const uint32_t end   = packDateTime(em, ed, e.end_hour, e.end_min);
    const bool in_range = (start <= end) ? (cur >= start && cur <= end) : (cur >= start || cur <= end);
    if (!in_range) {
        return false;
    }

    if (!e.recurring) {
        return true;
    }

    const int wday = t.tm_wday & 7;
    const int prev_wday = (wday + 6) % 7;
    const uint16_t cur_m  = static_cast<uint16_t>(hour) * 60 + min;
    const uint16_t from_m = static_cast<uint16_t>(e.from_hour) * 60 + e.from_min;
    const uint16_t to_m   = static_cast<uint16_t>(e.to_hour) * 60 + e.to_min;

    if (from_m <= to_m) {
        return ((e.weekdays_mask & (1U << wday)) != 0) && cur_m >= from_m && cur_m <= to_m;
    }
    return (((e.weekdays_mask & (1U << wday)) != 0) && cur_m >= from_m) ||
           (((e.weekdays_mask & (1U << prev_wday)) != 0) && cur_m <= to_m);
}

String jsonEsc(const String& s)
{
    String out;
    out.reserve(s.length() + 4);
    for (size_t i = 0; i < s.length(); ++i) {
        const uint8_t c = static_cast<uint8_t>(s[i]);
        if (c == '\\') out += "\\\\";
        else if (c == '"') out += "\\\"";
        else if (c < 0x20 || c >= 0x7F) {
            char buf[7];
            snprintf(buf, sizeof(buf), "\\u%04x", c);
            out += buf;
        } else {
            out += static_cast<char>(c);
        }
    }
    return out;
}

String serializeEntry(const OccasionEntry& e)
{
    String json;
    json.reserve(192);
    json += "{\"label\":\"" + jsonEsc(e.label) + "\"";
    json += ",\"recurring\":" + String(e.recurring ? 1 : 0);
    json += ",\"easter_offset\":" + String(e.easter_offset ? 1 : 0);
    json += ",\"esd\":" + String(e.easter_start_days);
    json += ",\"eed\":" + String(e.easter_end_days);
    json += ",\"sm\":" + String(e.start_month);
    json += ",\"sd\":" + String(e.start_day);
    json += ",\"sh\":" + String(e.start_hour);
    json += ",\"smin\":" + String(e.start_min);
    json += ",\"em\":" + String(e.end_month);
    json += ",\"ed\":" + String(e.end_day);
    json += ",\"eh\":" + String(e.end_hour);
    json += ",\"emin\":" + String(e.end_min);
    json += ",\"wd\":" + String(e.weekdays_mask);
    json += ",\"fh\":" + String(e.from_hour);
    json += ",\"fmin\":" + String(e.from_min);
    json += ",\"th\":" + String(e.to_hour);
    json += ",\"tmin\":" + String(e.to_min);
    json += ",\"scene_id\":" + String(static_cast<uint8_t>(e.scene_id));
    json += ",\"params\":{";
    for (size_t j = 0; j < e.params.size(); ++j) {
        if (j > 0) json += ",";
        json += "\"" + jsonEsc(e.params[j].first) + "\":\"" + jsonEsc(e.params[j].second) + "\"";
    }
    json += "}}";
    return json;
}

String serializeLocked()
{
    String json;
    json.reserve(1024);
    json += "[";
    for (size_t i = 0; i < s_entries.size(); ++i) {
        if (i > 0) json += ",";
        json += serializeEntry(s_entries[i]);
    }
    json += "]";
    return json;
}

void saveToPrefsLocked()
{
    Preferences prefs;
    prefs.begin(PREFS_NS, false);
    prefs.clear();
    prefs.putUShort("count", static_cast<uint16_t>(s_entries.size()));
    for (size_t i = 0; i < s_entries.size(); ++i) {
        const String key = "e" + String(i);
        prefs.putString(key.c_str(), serializeEntry(s_entries[i]));
    }
    prefs.end();
}

// Minimal JSON parser for our own occasion array schema
struct JsonCursor {
    const char* p;
    const char* end;

    void skipWs() {
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) ++p;
    }
    bool consume(char ch) {
        skipWs();
        if (p < end && *p == ch) { ++p; return true; }
        return false;
    }
    bool parseString(String& out) {
        skipWs();
        if (p >= end || *p != '"') return false;
        ++p;
        out = "";
        while (p < end && *p != '"') {
            if (*p == '\\' && p + 1 < end) {
                ++p;
                if (*p == 'u' && p + 4 < end) {
                    char hex[5] = { p[1], p[2], p[3], p[4], 0 };
                    out += static_cast<char>(strtoul(hex, nullptr, 16) & 0xFF);
                    p += 5;
                } else {
                    out += *p++;
                }
            } else {
                out += *p++;
            }
        }
        if (p >= end || *p != '"') return false;
        ++p;
        return true;
    }
    bool parseInt(long& out) {
        skipWs();
        if (p >= end) return false;
        char* endptr = nullptr;
        out = strtol(p, &endptr, 10);
        if (endptr == p) return false;
        p = endptr;
        return true;
    }
    bool skipValue() {
        skipWs();
        if (p >= end) return false;
        if (*p == '"') {
            String tmp;
            return parseString(tmp);
        }
        if (*p == '{') {
            ++p;
            if (consume('}')) return true;
            while (true) {
                String k;
                if (!parseString(k) || !consume(':') || !skipValue()) return false;
                if (consume(',')) continue;
                return consume('}');
            }
        }
        if (*p == '[') {
            ++p;
            if (consume(']')) return true;
            while (true) {
                if (!skipValue()) return false;
                if (consume(',')) continue;
                return consume(']');
            }
        }
        while (p < end && *p != ',' && *p != '}' && *p != ']' && !isspace(static_cast<unsigned char>(*p))) {
            ++p;
        }
        return true;
    }
};

uint8_t clampU8(long v, long lo, long hi)
{
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return static_cast<uint8_t>(v);
}

bool parseJsonArray(const String& json, std::vector<OccasionEntry>& out)
{
    JsonCursor c{ json.c_str(), json.c_str() + json.length() };
    if (!c.consume('[')) return false;
    out.clear();
    if (c.consume(']')) return true;

    while (true) {
        if (!c.consume('{')) return false;
        OccasionEntry e;
        if (!c.consume('}')) {
            while (true) {
                String key;
                if (!c.parseString(key) || !c.consume(':')) return false;
                if (key == "label") {
                    String s;
                    if (!c.parseString(s)) return false;
                    e.label = s.substring(0, 24);
                } else if (key == "params") {
                    if (!c.consume('{')) return false;
                    if (!c.consume('}')) {
                        while (true) {
                            String pk, pv;
                            if (!c.parseString(pk) || !c.consume(':') || !c.parseString(pv)) return false;
                            e.params.emplace_back(pk, pv);
                            if (c.consume(',')) continue;
                            if (c.consume('}')) break;
                            return false;
                        }
                    }
                } else {
                    long v = 0;
                    if (!c.parseInt(v)) {
                        if (!c.skipValue()) return false;
                    } else {
                        if      (key == "recurring" || key == "type") e.recurring = (v == 1);
                        else if (key == "easter_offset") e.easter_offset = (v == 1);
                        else if (key == "esd")      e.easter_start_days = static_cast<int8_t>(std::max(-30L, std::min(60L, v)));
                        else if (key == "eed")      e.easter_end_days   = static_cast<int8_t>(std::max(-30L, std::min(60L, v)));
                        else if (key == "sm")       e.start_month   = clampU8(v, 1, 12);
                        else if (key == "sd")       e.start_day     = clampU8(v, 1, 31);
                        else if (key == "sh")       e.start_hour    = clampU8(v, 0, 23);
                        else if (key == "smin")     e.start_min     = clampU8(v, 0, 59);
                        else if (key == "em")       e.end_month     = clampU8(v, 1, 12);
                        else if (key == "ed")       e.end_day       = clampU8(v, 1, 31);
                        else if (key == "eh")       e.end_hour      = clampU8(v, 0, 23);
                        else if (key == "emin")     e.end_min       = clampU8(v, 0, 59);
                        else if (key == "wd")       e.weekdays_mask = clampU8(v, 0, 127);
                        else if (key == "fh")       e.from_hour     = clampU8(v, 0, 23);
                        else if (key == "fmin")     e.from_min      = clampU8(v, 0, 59);
                        else if (key == "th")       e.to_hour       = clampU8(v, 0, 23);
                        else if (key == "tmin")     e.to_min        = clampU8(v, 0, 59);
                        else if (key == "scene_id") e.scene_id      = static_cast<SceneId>(clampU8(v, 0, static_cast<long>(SceneId::None) - 1));
                    }
                }
                if (c.consume(',')) continue;
                if (c.consume('}')) break;
                return false;
            }
        }
        out.push_back(std::move(e));
        if (c.consume(',')) continue;
        if (c.consume(']')) break;
        return false;
    }
    return true;
}

} // namespace

namespace Occasions {

void begin()
{
    ensureMutex();
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool loaded = false;
    Preferences prefs;
    if (prefs.begin(PREFS_NS, true)) {
        if (prefs.isKey("count")) {
            const uint16_t cnt = prefs.getUShort("count", 0);
            String arr = "[";
            for (uint16_t i = 0; i < cnt; ++i) {
                if (i > 0) arr += ",";
                arr += prefs.getString(("e" + String(i)).c_str(), "{}");
            }
            arr += "]";
            std::vector<OccasionEntry> parsed;
            if (parseJsonArray(arr, parsed)) {
                s_entries = std::move(parsed);
                loaded = true;
            }
        }
        prefs.end();
    }
    if (!loaded) {
        s_entries = buildDefaultEntries();
        saveToPrefsLocked();
    }
    xSemaphoreGive(s_mutex);
}

void resetToDefaults()
{
    ensureMutex();
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_entries = buildDefaultEntries();
    saveToPrefsLocked();
    xSemaphoreGive(s_mutex);
}

std::vector<OccasionEntry> getAll()
{
    ensureMutex();
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    std::vector<OccasionEntry> copy = s_entries;
    xSemaphoreGive(s_mutex);
    return copy;
}

void setAll(std::vector<OccasionEntry> entries)
{
    ensureMutex();
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_entries = std::move(entries);
    saveToPrefsLocked();
    xSemaphoreGive(s_mutex);
}

SceneId resolveActive(const tm& t, std::vector<std::pair<String, String>>& out_params)
{
    ensureMutex();
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (const OccasionEntry& e : s_entries) {
        if (matchesEntry(e, t)) {
            out_params = e.params;
            const SceneId sid = e.scene_id;
            xSemaphoreGive(s_mutex);
            return sid;
        }
    }
    out_params.clear();
    xSemaphoreGive(s_mutex);
    return SceneId::NormalDays;
}

String toJson()
{
    ensureMutex();
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    String json = serializeLocked();
    xSemaphoreGive(s_mutex);
    return json;
}

bool fromJson(const String& json)
{
    std::vector<OccasionEntry> parsed;
    if (!parseJsonArray(json, parsed)) {
        return false;
    }
    setAll(std::move(parsed));
    return true;
}

} // namespace Occasions
