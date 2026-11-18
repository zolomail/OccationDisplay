#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <ctime>

enum class WeatherCondition : uint8_t {
    Unknown = 0,
    Sunny,
    Cloudy,
    Rainy,
    Snow,
    Hail
};

namespace Weather {

namespace Detail {
    inline std::atomic<WeatherCondition> s_condition{WeatherCondition::Unknown};
    inline std::atomic<int>              s_wmo_code{-1};
    inline std::atomic<float>            s_temperature{0.0f};
    inline std::atomic<bool>             s_is_day{true};
    inline std::atomic<bool>             s_valid{false};
    inline bool                          s_geo_resolved = false;
    inline float                         s_lat          = 56.15f; // Default fallback (Denmark)
    inline float                         s_lon          = 10.21f;
    inline String                        s_city         = "Default";

    inline WeatherCondition mapWmoCode(int code)
    {
        if (code == 0 || code == 1) {
            return WeatherCondition::Sunny;
        }
        if (code == 2 || code == 3 || code == 45 || code == 48) {
            return WeatherCondition::Cloudy;
        }
        if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) {
            return WeatherCondition::Rainy;
        }
        if ((code >= 71 && code <= 77) || code == 85 || code == 86) {
            return WeatherCondition::Snow;
        }
        if (code == 95 || code == 96 || code == 99) {
            return WeatherCondition::Hail;
        }
        return WeatherCondition::Unknown;
    }

    // Performs a lightweight HTTP/1.0 GET request and returns the response body
    inline bool httpGetBody(const char* host, const String& path, char* outBuf, size_t outSize)
    {
        WiFiClient client;
        client.setTimeout(5);
        if (!client.connect(host, 80)) {
            return false;
        }

        client.printf("GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", path.c_str(), host);

        uint32_t start = millis();
        while (!client.available() && client.connected()) {
            if (millis() - start > 5000) {
                client.stop();
                return false;
            }
            vTaskDelay(pdMS_TO_TICKS(20));
        }

        // Skip HTTP headers until blank line (\r\n\r\n)
        bool headersEnded = client.find("\r\n\r\n");
        if (!headersEnded) {
            client.stop();
            return false;
        }

        size_t idx = 0;
        while ((client.connected() || client.available()) && idx < outSize - 1) {
            if (client.available()) {
                outBuf[idx++] = static_cast<char>(client.read());
            } else {
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        }
        outBuf[idx] = '\0';
        client.stop();
        return idx > 0;
    }

    inline void resolveLocationFromIp()
    {
        char body[160];
        if (!httpGetBody("ip-api.com", "/csv/?fields=status,city,lat,lon", body, sizeof(body))) {
            return;
        }
        // ip-api CSV always returns fields in canonical order: "success,Aarhus,56.1567,10.2153\n"
        if (strncmp(body, "success,", 8) != 0) {
            return;
        }
        char* p_city = body + 8;
        char* nl = strpbrk(p_city, "\r\n");
        if (nl) *nl = '\0';

        // Parse from the right so city names cannot shift lat/lon columns
        char* last_comma = strrchr(p_city, ',');
        if (!last_comma) return;
        *last_comma = '\0';
        const char* p_lon = last_comma + 1;

        char* prev_comma = strrchr(p_city, ',');
        if (!prev_comma) return;
        *prev_comma = '\0';
        const char* p_lat = prev_comma + 1;

        char* end_lat = nullptr;
        char* end_lon = nullptr;
        const float lat = strtof(p_lat, &end_lat);
        const float lon = strtof(p_lon, &end_lon);
        if (end_lat != p_lat && end_lon != p_lon && (lat != 0.0f || lon != 0.0f)) {
            s_lat          = lat;
            s_lon          = lon;
            s_city         = p_city;
            s_geo_resolved = true;
            Serial.printf("Weather location resolved: %s (%.4f, %.4f)\n", s_city.c_str(), s_lat, s_lon);
        }
    }

    inline bool fetchOpenMeteo()
    {
        String path = "/v1/forecast?latitude=" + String(s_lat, 4) +
                      "&longitude=" + String(s_lon, 4) +
                      "&current=weather_code,temperature_2m,is_day";

        char body[512];
        if (!httpGetBody("api.open-meteo.com", path, body, sizeof(body))) {
            return false;
        }

        // Locate the "current":{...} object at the end of the JSON response
        const char* curObj = strstr(body, "\"current\":{");
        if (!curObj) {
            return false;
        }

        const char* codePtr = strstr(curObj, "\"weather_code\":");
        const char* tempPtr = strstr(curObj, "\"temperature_2m\":");
        const char* dayPtr  = strstr(curObj, "\"is_day\":");
        if (!codePtr || !tempPtr) {
            return false;
        }

        const int   code   = static_cast<int>(strtol(codePtr + 15, nullptr, 10));
        const float temp   = strtof(tempPtr + 17, nullptr);
        const bool  is_day = dayPtr ? (strtol(dayPtr + 9, nullptr, 10) != 0) : true;

        s_wmo_code.store(code, std::memory_order_relaxed);
        s_temperature.store(temp, std::memory_order_relaxed);
        s_is_day.store(is_day, std::memory_order_relaxed);
        s_condition.store(mapWmoCode(code), std::memory_order_relaxed);
        s_valid.store(true, std::memory_order_release);

        Serial.printf("Weather updated: WMO=%d, %.1f C, is_day=%d\n", code, temp, is_day ? 1 : 0);
        return true;
    }

    inline void weatherTask(void*)
    {
        constexpr uint32_t POLL_INTERVAL_MS = 15UL * 60UL * 1000UL; // 15 minutes
        constexpr uint32_t RETRY_INTERVAL_MS = 30UL * 1000UL;       // 30 seconds on failure

        for (;;) {
            if (WiFi.status() == WL_CONNECTED) {
                if (!s_geo_resolved) {
                    resolveLocationFromIp();
                }
                if (fetchOpenMeteo()) {
                    vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
                    continue;
                }
                vTaskDelay(pdMS_TO_TICKS(RETRY_INTERVAL_MS));
            } else {
                vTaskDelay(pdMS_TO_TICKS(2000));
            }
        }
    }
} // namespace Detail

inline void begin()
{
    xTaskCreatePinnedToCore(
        Detail::weatherTask,
        "weather_task",
        4096,
        nullptr,
        1,
        nullptr,
        0 // Run on Core 0 (Wi-Fi/system core) so Core 1 matrix rendering never stutters
    );
}

inline WeatherCondition condition()
{
    return Detail::s_condition.load(std::memory_order_relaxed);
}

inline float temperature()
{
    return Detail::s_temperature.load(std::memory_order_relaxed);
}

inline int wmoCode()
{
    return Detail::s_wmo_code.load(std::memory_order_relaxed);
}

inline bool isValid()
{
    return Detail::s_valid.load(std::memory_order_acquire);
}

inline bool isDay()
{
    if (isValid()) {
        return Detail::s_is_day.load(std::memory_order_relaxed);
    }
    const time_t now = time(nullptr);
    tm t{};
    localtime_r(&now, &t);
    return (t.tm_hour >= 7 && t.tm_hour < 20);
}

// Returns current lunar phase (0..7):
//   0 = New Moon, 1 = Waxing Crescent, 2 = First Quarter, 3 = Waxing Gibbous,
//   4 = Full Moon, 5 = Waning Gibbous, 6 = Last Quarter, 7 = Waning Crescent
inline uint8_t moonPhase()
{
    constexpr int64_t REF_NEW_MOON_UTC = 947182440LL; // 2000-01-06 18:14:00 UTC
    constexpr int64_t SYNODIC_MONTH_S  = 2551443LL;   // 29.530588853 days in seconds

    const int64_t now = static_cast<int64_t>(time(nullptr));
    if (now < REF_NEW_MOON_UTC) {
        return 4; // Default to Full Moon before NTP sync
    }
    const int64_t elapsed   = now - REF_NEW_MOON_UTC;
    const int64_t cycle_sec = ((elapsed % SYNODIC_MONTH_S) + SYNODIC_MONTH_S) % SYNODIC_MONTH_S;
    return static_cast<uint8_t>(((cycle_sec + (SYNODIC_MONTH_S / 16)) * 8LL / SYNODIC_MONTH_S) & 7);
}

inline const char* moonPhaseName(uint8_t phase = moonPhase())
{
    switch (phase & 7) {
        case 0:  return "New Moon";
        case 1:  return "Waxing Crescent";
        case 2:  return "First Quarter";
        case 3:  return "Waxing Gibbous";
        case 4:  return "Full Moon";
        case 5:  return "Waning Gibbous";
        case 6:  return "Last Quarter";
        default: return "Waning Crescent";
    }
}

inline const String& city()
{
    return Detail::s_city;
}

inline const char* conditionName(WeatherCondition c = condition())
{
    switch (c) {
        case WeatherCondition::Sunny:  return "Sunny";
        case WeatherCondition::Cloudy: return "Cloudy";
        case WeatherCondition::Rainy:  return "Rainy";
        case WeatherCondition::Snow:   return "Snow";
        case WeatherCondition::Hail:   return "Hail";
        case WeatherCondition::Unknown:
        default:                       return "Unknown";
    }
}

} // namespace Weather
