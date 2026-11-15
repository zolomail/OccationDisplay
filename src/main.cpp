#include <Arduino.h>
#include <Wire.h>
#include <ctime>
#include <memory>

#include <WiFi.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

// Custom Scene, Service & Effect Headers
#include "Scene.h"
#include "Weather.h"
#include "WebUi.h"
#include "effects/ClearScreen.h"
#include "effects/SnowScape.h"
#include "effects/Stars.h"
#include "effects/FunkyLine.h"
#include "effects/Present.h"
#include "effects/FlyingText.h"
#include "effects/FireWorks.h"
#include "effects/ShowTime.h"
#include "effects/Bitmap.h"
#include "effects/ShootingStar.h"
#include "effects/Circle.h"
#include "effects/ScrollText.h"
#include "effects/Blood.h"
#include "effects/BubblesInt.h"
#include "effects/Flames.h"
#include "effects/WeatherOverlay.h"

#include "bitmaps/ufo.h"
#include "bitmaps/pumpkin.h"
#include "bitmaps/ghost.h"
#include "bitmaps/champagne.h"

namespace {
// =========================================================================
// Hardware & Timing Configuration (ESP32-S3 SuperMini)
// =========================================================================
namespace Config
{
    constexpr uint8_t  LED_PIN        = 48;

    constexpr uint16_t PANEL_RES_X   = 64;
    constexpr uint16_t PANEL_RES_Y   = 32;
    constexpr uint16_t PANEL_CHAIN   = 2; // 2 chained panels = 128x32

    constexpr int8_t   PIN_R1        = 44; // Routed to RX (GPIO 44)
    constexpr int8_t   PIN_G1        = 13;
    constexpr int8_t   PIN_B1        = 1;
    constexpr int8_t   PIN_R2        = 2;
    constexpr int8_t   PIN_G2        = 12;
    constexpr int8_t   PIN_B2        = 3;

    constexpr int8_t   PIN_A         = 4;
    constexpr int8_t   PIN_B         = 11;
    constexpr int8_t   PIN_C         = 5;
    constexpr int8_t   PIN_D         = 10;
    constexpr int8_t   PIN_E         = -1; // GND (1/16 scan)

    constexpr int8_t   PIN_CLK       = 6;
    constexpr int8_t   PIN_LAT       = 9;
    constexpr int8_t   PIN_OE        = 7;

    constexpr uint8_t  BRIGHTNESS    = 32;
    constexpr uint32_t FRAME_TIME_MS = 40; // ~25 FPS

    // POSIX timezone string for Central European Time with automatic DST
    constexpr char     TZ_INFO[]     = "CET-1CEST,M3.5.0,M10.5.0/3";
    constexpr char     NTP_SERVER[]  = "pool.ntp.org";
}

// =========================================================================
// Compile-Time Color Palette
// =========================================================================
namespace Colors {
    constexpr uint16_t BLACK  = color565(0, 0, 0);
    constexpr uint16_t WHITE  = color565(255, 255, 255);
    constexpr uint16_t RED    = color565(255, 0, 0);
    constexpr uint16_t ORANGE = color565(255, 128, 0);
    constexpr uint16_t YELLOW = color565(255, 255, 0);
    constexpr uint16_t GREEN  = color565(0, 255, 0);
    constexpr uint16_t CYAN   = color565(0, 255, 255);
    constexpr uint16_t BLUE   = color565(0, 0, 255);
    constexpr uint16_t PURPLE = color565(255, 0, 255);
    constexpr uint16_t GREY   = color565(127, 127, 127);
}

// =========================================================================
// Application State
// =========================================================================
std::unique_ptr<MatrixPanel_I2S_DMA> dma_display;
tm current_time{};

std::shared_ptr<ScrollText> scroller;
std::unique_ptr<FlyingText> ip_overlay;
IPAddress                   cached_ip;

// =========================================================================
// On-Demand Scene Management
// =========================================================================
enum class SceneId : uint8_t {
    None,
    Welcome,
    Connecting,
    SetupAp,
    WifiUnknown,
    WifiStopped,
    Test,
    NewYearsMidnight,
    JanuaryDays,
    Aquatic,
    Halloween,
    JustSnow,
    ChristmasEve,
    Birthday,
    NewYearsDay,
    ChristmasDays,
    NormalDays,
    FlamingHot
};

Scene   active_scene;
SceneId active_scene_id = SceneId::None;

Scene createScene(SceneId id, MatrixPanel_I2S_DMA& display)
{
    const int current_year = (current_time.tm_year >= 120) ? (current_time.tm_year + 1900) : 2020;

    switch (id)
    {
        case SceneId::Connecting:
            return {
                std::make_shared<ClearScreen>(display),
                std::make_shared<FlyingText>(display, "Connecting...", -1, -1, 0, 0, Colors::ORANGE, TINY_WIDE_FONT)
            };

        case SceneId::Welcome:
            return {
                std::make_shared<ClearScreen>(display),
                std::make_shared<FlyingText>(display, "*********************", 2, 0, 0, 0, Colors::WHITE),
                std::make_shared<FlyingText>(display, "*                   *", 2, 8, 0, 0, Colors::WHITE),
                std::make_shared<FlyingText>(display, "-OCCATION DISPLAY-", -1, -1, 0, 0, Colors::RED),
                std::make_shared<FlyingText>(display, "*                   *", 2, 16, 0, 0, Colors::WHITE),
                std::make_shared<FlyingText>(display, "*********************", 2, 24, 0, 0, Colors::WHITE)
            };

        case SceneId::SetupAp:
        {
            Scene scene = {
                std::make_shared<ClearScreen>(display),
                std::make_shared<FlyingText>(display, String("SSID : ") + WebUi::apSsid(), 8, 13, 0, 0, Colors::WHITE),
                std::make_shared<FlyingText>(display, String("Password : ") + WebUi::apPassword(), 8, 19, 0, 0, Colors::WHITE),
                std::make_shared<FlyingText>(display, "Config : 192.168.4.1", 8, 25, 0, 0, Colors::WHITE)
            };
            if (WebUi::wifiError().length() > 0)
            {
                scene.push_back(std::make_shared<FlyingText>(display, WebUi::wifiError(), -1, 2, 0, 0, Colors::RED, TINY_WIDE_FONT));
            }
            return scene;
        }
        case SceneId::WifiUnknown:
            return {
                std::make_shared<ClearScreen>(display),
                std::make_shared<FlyingText>(display, "Unknown SSID", -1, 28, 0, 0, Colors::RED, TINY_WIDE_FONT)
            };

        case SceneId::WifiStopped:
            return {
                std::make_shared<ClearScreen>(display),
                std::make_shared<FlyingText>(display, "WiFi Stopped", -1, 28, 0, 0, Colors::RED, TINY_WIDE_FONT)
            };

        case SceneId::Test:
            return {
                std::make_shared<ClearScreen>(display),
                std::make_shared<Stars>(display),
                std::make_shared<FunkyLine>(display, 0, 3, color565(255, 0, 0)),
                std::make_shared<FunkyLine>(display, 5, 3, color565(255, 100, 0)),
                std::make_shared<FunkyLine>(display, 10, 3, color565(255, 200, 100)),
                std::make_shared<FunkyLine>(display, 15, 3, color565(255, 255, 200)),
                std::make_shared<FunkyLine>(display, 20, 3, color565(255, 255, 255)),
                std::make_shared<FlyingText>(display, "*", 20, 10, -2, -1, Colors::RED),
                std::make_shared<FlyingText>(display, "+", 100, 20, 1, -2, Colors::GREEN),
                std::make_shared<FlyingText>(display, "o", 80, 24, 2, -2, Colors::CYAN),
                std::make_shared<ShowTime>(display, current_time, -1, -1, 0, 0, Colors::BLUE, NORMAL_FONT, true),
                std::make_shared<Circle>(display),
                std::make_shared<Circle>(display, 80, 20, 12, Colors::BLUE),
                std::make_shared<ScrollText>(
                    display,
                    "\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2A\x2B\x2C\x2D\x2E\x2F"
                    "\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3A\x3B\x3C\x3D\x3E\x3F"
                    "\x40\x41\x42\x43\x44\x45\x46\x47\x48\x49\x4A\x4B\x4C\x4D\x4E\x4F"
                    "\x50\x51\x52\x53\x54\x55\x56\x57\x58\x59\x5A\x5B\x5C\x5D\x5E\x5F"
                    "\x60\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6A\x6B\x6C\x6D\x6E\x6F"
                    "\x70\x71\x72\x73\x74\x75\x76\x77\x78\x79\x7A\x7B\x7C\x7D\x7E\x7F"
                    "\x80\x81    ",
                    22, -5, Colors::RED, TINY_WIDE_FONT)
                };

        case SceneId::NormalDays:
            return {
                std::make_shared<ClearScreen>(display),
                scroller,
                std::make_shared<ShowTime>(display, current_time, -1, -1, 0, 0, Colors::BLUE, NORMAL_FONT, true),
                std::make_shared<WeatherOverlay>(display, 0, 6, Colors::CYAN)
            };

        case SceneId::JustSnow:
            return {
                std::make_shared<ClearScreen>(display),
                scroller,
                std::make_shared<SnowScape>(display, 50, 10),
                std::make_shared<WeatherOverlay>(display, 0, 6, Colors::CYAN)
            };

        case SceneId::Aquatic:
            return {
                std::make_shared<ClearScreen>(display),
                std::make_shared<ShowTime>(display, current_time, -1, -1, 0, 0, Colors::ORANGE, TINY_WIDE_FONT, true),
                std::make_shared<BubblesInt>(display, 16)
            };

        case SceneId::FlamingHot:
            return {
                std::make_shared<ClearScreen>(display),
                std::make_shared<Flames>(display, 5)
            };

        case SceneId::ChristmasDays:
            return {
                std::make_shared<ClearScreen>(display),
                std::make_shared<SnowScape>(display),
                std::make_shared<ShowTime>(display, current_time, -1, -1, 0, 0, Colors::BLUE, NORMAL_FONT, true)
            };

        case SceneId::ChristmasEve:
            return {
                std::make_shared<ClearScreen>(display),
                std::make_shared<Present>(display, -20, 5, 5, Colors::GREEN, Colors::RED),
                std::make_shared<Present>(display, -10, 7, 3, Colors::CYAN, Colors::RED),
                std::make_shared<Present>(display, -15, 7, 5, Colors::WHITE, Colors::YELLOW),
                std::make_shared<Present>(display, -35, 11, 9, Colors::PURPLE, Colors::GREEN),
                std::make_shared<SnowScape>(display),
                std::make_shared<FlyingText>(display, "Merry Christmas", 0, 0, 1, 1, Colors::RED)
            };

        case SceneId::JanuaryDays:
            return {
                std::make_shared<ClearScreen>(display),
                std::make_shared<Stars>(display, 30),
                std::make_shared<ShootingStar>(display, -1000),
                std::make_shared<SnowScape>(display, 20, 4),
                std::make_shared<ShowTime>(display, current_time, -1, -1, 0, 0, Colors::BLUE, NORMAL_FONT, true),
                scroller,
                std::make_shared<Bitmap>(display, *ufo_bitmap, 16, 10, -1, -1, 8, 0, ufo_mask)
            };

        case SceneId::Halloween:
            return {
                std::make_shared<ClearScreen>(display),
                std::make_shared<ShowTime>(display, current_time, -1, 27, 0, 0, Colors::RED, TINY_WIDE_FONT, true),
                std::make_shared<ScrollText>(display, "!!!HAPPY HALLOWEEN!!! --- \x81\x81\x81 --- ", -1, -10, Colors::ORANGE, TINY_WIDE_FONT),
                std::make_shared<Bitmap>(display, *pumpkin_bitmap, 19, 18, -1, -1, 16, 8, pumpkin_mask),
                std::make_shared<Bitmap>(display, *pumpkin_bitmap, 19, 18, 100, -1, -15, -7, pumpkin_mask),
                std::make_shared<Bitmap>(display, *ghost_bitmap, 33, 27, -3, -1, 26, 1, ghost_mask),
                std::make_shared<Blood>(display)
            };

        case SceneId::Birthday:
        {
            const int age = current_year - 2016;
            return {
                std::make_shared<ClearScreen>(display),
                std::make_shared<SnowScape>(display),
                std::make_shared<Present>(display, -20, 5, 5, Colors::GREEN, Colors::RED),
                std::make_shared<Present>(display, -10, 7, 3, Colors::CYAN, Colors::RED),
                std::make_shared<Present>(display, -15, 7, 5, Colors::WHITE, Colors::YELLOW),
                std::make_shared<Present>(display, -35, 11, 9, Colors::PURPLE, Colors::GREEN),
                std::make_shared<FlyingText>(display, "Happy birthday", 20, 10, -2, -1, Colors::RED),
                std::make_shared<FlyingText>(display, "ERIK", 0, 0, 1, 1, Colors::BLUE),
                std::make_shared<FlyingText>(display, String(age) + " \x86r", 64, 16, -1, 1, Colors::CYAN)
            };
        }

        case SceneId::NewYearsDay:
            return {
                std::make_shared<ClearScreen>(display),
                std::make_shared<Stars>(display),
                std::make_shared<FireWorks>(display),
                std::make_shared<FlyingText>(display, "Happy New Year", -1, 5, 0, 0, Colors::BLUE),
                std::make_shared<FlyingText>(display, String(current_year + 1), -1, 13, 0, 0, Colors::WHITE),
                std::make_shared<ShowTime>(display, current_time, -1, 21, 0, 0, Colors::CYAN)
            };

        case SceneId::NewYearsMidnight:
            return {
                std::make_shared<ClearScreen>(display),
                std::make_shared<Stars>(display),
                std::make_shared<Bitmap>(display, *newyear_bitmap, 34, 32),
                std::make_shared<FireWorks>(display, -200),
                std::make_shared<FireWorks>(display, -200),
                std::make_shared<FireWorks>(display, -200),
                std::make_shared<FlyingText>(display, "Happy New Year", 24, 12, 0, 0, Colors::BLUE)
            };

        case SceneId::None:
        default:
            return {};
    }
}

void renderScene(SceneId id)
{
    if (id != active_scene_id) {
        active_scene.clear(); // Release existing scene's RAM before allocating the next
        active_scene    = createScene(id, *dma_display);
        active_scene_id = id;
    }
    active_scene.render();
}

SceneId selectDateScene(const tm& t)
{
    const int month = t.tm_mon + 1;
    const int day   = t.tm_mday;
    const int hours = t.tm_hour;

    if (month == 1 && day == 1 && hours < 12) return SceneId::NewYearsMidnight;
    if (month == 1)                           return SceneId::JanuaryDays;
    if (month == 2 && day == 14)              return SceneId::Aquatic;
    if ((month == 10 && day > 15) ||
        (month == 11 && day == 1 && hours < 8)) return SceneId::Halloween;
    if (month == 11)                          return SceneId::JustSnow;
    if (month == 12 && day == 24)             return SceneId::ChristmasEve;
    if (month == 12 && day == 30)             return SceneId::Birthday;
    if (month == 12 && day == 31)             return SceneId::NewYearsDay;
    if (month == 12)                          return SceneId::ChristmasDays;

    return SceneId::NormalDays;
}

SceneId checkWifiState()
{
    switch (WiFi.status())
    {
        case WL_CONNECTED:      return SceneId::None;
        case WL_IDLE_STATUS:    return SceneId::Welcome;
        case WL_NO_SSID_AVAIL:
            WebUi::startAccessPoint("Couldn't find SSID");
            active_scene_id = SceneId::None;
            return SceneId::SetupAp;

        case WL_CONNECT_FAILED:
            WebUi::startAccessPoint("Bad password!");
            active_scene_id = SceneId::None;
            return SceneId::SetupAp;

        case WL_CONNECTION_LOST:
            WebUi::startAccessPoint("Connection lost!");
            active_scene_id = SceneId::None;
            return SceneId::SetupAp;

        case WL_DISCONNECTED:   return SceneId::Connecting;
        case WL_NO_SHIELD:
        case WL_SCAN_COMPLETED: return SceneId::WifiUnknown;
        case WL_STOPPED:        return SceneId::WifiStopped;
    }
    return SceneId::None;
}

} // namespace

// =========================================================================
// Setup
// =========================================================================
void setup()
{
    Serial.begin(115200);
    delay(1000);
    pinMode(Config::LED_PIN, OUTPUT);
    pinMode(Config::PIN_R1, OUTPUT);

    // Initialize time structure
    const time_t initial_epoch = 0;
    localtime_r(&initial_epoch, &current_time);

    // Configure HUB75 DMA Display
    HUB75_I2S_CFG mxconfig(
        Config::PANEL_RES_X,
        Config::PANEL_RES_Y,
        Config::PANEL_CHAIN
    );

    mxconfig.gpio.r1  = Config::PIN_R1;
    mxconfig.gpio.g1  = Config::PIN_G1;
    mxconfig.gpio.b1  = Config::PIN_B1;
    mxconfig.gpio.r2  = Config::PIN_R2;
    mxconfig.gpio.g2  = Config::PIN_G2;
    mxconfig.gpio.b2  = Config::PIN_B2;
    mxconfig.gpio.a   = Config::PIN_A;
    mxconfig.gpio.b   = Config::PIN_B;
    mxconfig.gpio.c   = Config::PIN_C;
    mxconfig.gpio.d   = Config::PIN_D;
    mxconfig.gpio.e   = Config::PIN_E;
    mxconfig.gpio.lat = Config::PIN_LAT;
    mxconfig.gpio.oe  = Config::PIN_OE;
    mxconfig.gpio.clk = Config::PIN_CLK;

    mxconfig.double_buff      = true;
    mxconfig.min_refresh_rate = 120;
    mxconfig.clkphase         = false;
    mxconfig.latch_blanking   = 2;
//    mxconfig.i2sspeed         = HUB75_I2S_CFG::HZ_10M;

    dma_display.reset(new MatrixPanel_I2S_DMA(mxconfig));
    dma_display->begin();
    dma_display->setBrightness8(Config::BRIGHTNESS);
    dma_display->fillScreen(Colors::BLACK);

    if (psramFound()) {
        Serial.printf("PSRAM size: %lu\n", ESP.getPsramSize());
    } else {
        Serial.println("PSRAM not detected!");
    }

    // Create shared scroller instance; WebUi::begin() will load persisted text into it
    scroller = std::make_shared<ScrollText>(
        *dma_display,
        "",
        -1, -10, Colors::RED, FontId::TINY_WIDE_FONT
    );

    // Configure native ESP32 SNTP with automatic CET/CEST Daylight Saving Time
    configTzTime(Config::TZ_INFO, Config::NTP_SERVER);

    // Start background weather polling task on Core 0
    Weather::begin();

    // Load NVS settings, connect Wi-Fi, and start the HTTP server
    WebUi::begin(scroller);

    Serial.println("Start...");
}

void BlinkLed(uint32_t frame_start, bool overloaded)
{
    static uint32_t last_led_toggle = 0;
    static bool led_on = false;

    if (!overloaded) {
        if (led_on) {
            led_on = false;
            rgbLedWrite(Config::LED_PIN, 0, 0, 0);
        }
        return;
    }

    if (frame_start - last_led_toggle >= 200)
    {
        last_led_toggle = frame_start;
        led_on = !led_on;
        rgbLedWrite(Config::LED_PIN, led_on ? 30 : 0, 0, 0);
    }
}

// =========================================================================
// Main Loop (~25 FPS)
// =========================================================================
void loop()
{
    const uint32_t frame_start = millis();

    if (WebUi::isApMode()) {
        // We are the Access Point (Show scene that displays connect info)
        renderScene(SceneId::SetupAp);
    } else if (SceneId id = checkWifiState(); id != SceneId::None) {
        // We are trying to connect to an Access Point show scenes about how it is going
        renderScene(id);
    } else {
        // We are connected to an Access Point, we are good to show scenes
        const time_t now = time(nullptr);
        localtime_r(&now, &current_time);
        if (WebUi::isTestMode())
        {
            renderScene(SceneId::Test);
        } else {
            renderScene(selectDateScene(current_time));
        }

        if (WebUi::showIp())
        {
            const IPAddress ip = WiFi.localIP();
            if (!ip_overlay || ip != cached_ip)
            {
                cached_ip  = ip;
                ip_overlay = std::unique_ptr<FlyingText>(
                    new FlyingText(*dma_display, ip.toString(), -1, 0, 0, 0, Colors::GREEN, TINY_WIDE_FONT)
                );
            }
            ip_overlay->render();
        }
    }

    dma_display->flipDMABuffer();
    WebUi::handleClient();

    const uint32_t elapsed = millis() - frame_start;
    if (elapsed < Config::FRAME_TIME_MS) {
        BlinkLed(frame_start, false);
        delay(Config::FRAME_TIME_MS - elapsed);
    } else {
        Serial.println("Rendering took too long");
        BlinkLed(frame_start, true);
    }
}
