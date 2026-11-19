#include <Arduino.h>
#include <Wire.h>
#include <ctime>
#include <memory>

#include <WiFi.h>
#include <LittleFS.h>
//#define PIXEL_COLOR_DEPTH_BITS 6
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
#include "effects/FlyingPng.h"
#include "effects/ShootingStar.h"
#include "effects/Circle.h"
#include "effects/ScrollText.h"
#include "effects/Blood.h"
#include "effects/BubblesInt.h"
#include "effects/Flames.h"
#include "effects/WeatherOverlay.h"
#include "effects/WaveSim.h"
#include "effects/BumpMap.h"
#include "effects/Hearts.h"
#include "effects/Cube3D.h"
#include "effects/QrCode.h"
#include "effects/Border.h"

namespace {
// =========================================================================
// Hardware & Timing Configuration (ESP32-S3 SuperMini)
// =========================================================================
namespace Config
{
    constexpr uint8_t  LED_PIN        = 48;

    constexpr uint16_t PANEL_RES_X   = 64;
    constexpr uint16_t PANEL_RES_Y   = 32;
    constexpr uint16_t PANEL_CHAIN   = 3; // 2 chained panels = 128x32

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
    constexpr bool     SHOW_CPU_LOAD_BAR = true; // 1-pixel CPU load bar on far right column

    // POSIX timezone string for Central European Time with automatic DST
    constexpr char     TZ_INFO[]     = "CET-1CEST,M3.5.0,M10.5.0/3";
    constexpr char     NTP_SERVER[]  = "pool.ntp.org";
    constexpr char     DEFAULT_SCROLL[] = "put your text here now --- \x80\x80\x80 --- ";
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

std::unique_ptr<FlyingText> ip_overlay;
IPAddress                   cached_ip;

std::unique_ptr<Scene> active_scene;
SceneId                active_scene_id = SceneId::None;

// =========================================================================
// On-Demand Scene Management
// =========================================================================

std::unique_ptr<Scene> createScene(SceneId id, MatrixPanel_I2S_DMA &display)
{
    const int current_year = (current_time.tm_year >= 120) ? (current_time.tm_year + 1900) : 2020;
    auto scene = std::make_unique<Scene>(id, display);

    switch (id) {
        case SceneId::Connecting:
            scene->add_effect<ClearScreen>();
            scene->add_effect<FlyingText>("Connecting...", XPOS_CENTER, YPOS_CENTER, 0, 0, Colors::ORANGE, TINY_WIDE_FONT);
            break;

        case SceneId::Welcome:
            scene->add_effect<ClearScreen>();
            scene->add_effect<Border>(2, Colors::WHITE, Colors::BLUE, BorderPattern::Dashed, 4, 1);
            scene->add_effect<FlyingText>("-OCCATION DISPLAY-", XPOS_CENTER, YPOS_CENTER, 0, 0, Colors::RED);
            break;

        case SceneId::SetupAp:
            scene->add_effect<ClearScreen>();
            scene->add_effect<FlyingText>(String("SSID : ") + WebUi::apSsid(), 0, 7, 0, 0, Colors::WHITE);
            scene->add_effect<FlyingText>(String("Pass : ") + WebUi::apPassword(), 0, 15, 0, 0, Colors::WHITE);
            scene->add_effect<FlyingText>(String("IpAdr: ") + WiFi.localIP().toString(), 0, 23, 0, 0, Colors::WHITE);
            if (WebUi::wifiError().length() > 0) {
                scene->add_effect<FlyingText>(WebUi::wifiError(), XPOS_CENTER, 0, 0, 0, Colors::RED, TINY_WIDE_FONT);
            }
            break;

        case SceneId::WifiUnknown:
            scene->add_effect<ClearScreen>();
            scene->add_effect<FlyingText>("Unknown SSID", XPOS_CENTER, 28, 0, 0, Colors::RED, TINY_WIDE_FONT);
            break;

        case SceneId::WifiStopped:
            scene->add_effect<ClearScreen>();
            scene->add_effect<FlyingText>("WiFi Stopped", XPOS_CENTER, 28, 0, 0, Colors::RED, TINY_WIDE_FONT);
            break;

        case SceneId::WebUiQr: {
            const String ip_str = WiFi.localIP().toString();
            scene->add_effect<ClearScreen>();
            scene->add_effect<QrCode>("https://" + ip_str + "/", 2, YPOS_CENTER);
            scene->add_effect<FlyingText>("SCAN FOR WEB UI", 33, 8, 0, 0, Colors::CYAN, TINY_WIDE_FONT);
            scene->add_effect<FlyingText>(ip_str, 33, 18, 0, 0, Colors::WHITE, TINY_WIDE_FONT);
            break;
        }

        case SceneId::FlamingHot:
            scene->add_effect<ClearScreen>();
            scene->add_effect<Flames>(5);
            break;

        case SceneId::WaterEffect:
            scene->add_effect<WaveSim>(5, true);
            break;

        case SceneId::BumpMapEffect:
            scene->add_effect<BumpMap>(
                scene->color("Bump Color", Colors::ORANGE),
                scene->text("Bump Text", "OCCATION"),
                false);
            break;

        case SceneId::FullscreenTest:
            scene->add_effect<FlyingPng>("/fullscreen/golfer.png", 0, 0, 0, 0, false, true);
            break;

        case SceneId::Test:
            scene->add_effect<ClearScreen>();
            scene->add_effect<Stars>();
            scene->add_effect<FunkyLine>(0, 3, color565(255, 0, 0));
            scene->add_effect<FunkyLine>(5, 3, color565(255, 100, 0));
            scene->add_effect<FunkyLine>(10, 3, color565(255, 200, 100));
            scene->add_effect<FunkyLine>(15, 3, color565(255, 255, 200));
            scene->add_effect<FunkyLine>(20, 3, color565(255, 255, 255));
            scene->add_effect<FlyingText>("*", 20, 10, -2, -1, Colors::RED);
            scene->add_effect<FlyingText>("+", 100, 20, 1, -2, Colors::GREEN);
            scene->add_effect<FlyingText>("o", 80, 24, 2, -2, Colors::CYAN);
            scene->add_effect<ShowTime>(current_time, XPOS_CENTER, YPOS_CENTER, 0, 0, scene->color("Clock Color", Colors::BLUE), scene->font("Clock Font", NORMAL_FONT), true);
            scene->add_effect<Circle>();
            scene->add_effect<Circle>(80, 20, 12, Colors::BLUE);
            scene->add_effect<ScrollText>(
                "\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2A\x2B\x2C\x2D\x2E\x2F"
                "\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3A\x3B\x3C\x3D\x3E\x3F"
                "\x40\x41\x42\x43\x44\x45\x46\x47\x48\x49\x4A\x4B\x4C\x4D\x4E\x4F"
                "\x50\x51\x52\x53\x54\x55\x56\x57\x58\x59\x5A\x5B\x5C\x5D\x5E\x5F"
                "\x60\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6A\x6B\x6C\x6D\x6E\x6F"
                "\x70\x71\x72\x73\x74\x75\x76\x77\x78\x79\x7A\x7B\x7C\x7D\x7E\x7F"
                "\x80\x81    ",
                22, -5, scene->color("Scroll Color", Colors::RED), scene->font("Scroll Font", TINY_WIDE_FONT));
            break;

        case SceneId::NormalDays:
            scene->add_effect<ClearScreen>();
            scene->add_effect<ScrollText>(scene->text("Scroll Text", Config::DEFAULT_SCROLL), 26, -10, scene->color("Scroll Color", Colors::RED), scene->font("Scroll Font", TINY_WIDE_FONT));
            scene->add_effect<ShowTime>(current_time, XPOS_CENTER, YPOS_CENTER, 0, 0, scene->color("Clock Color", Colors::BLUE), scene->font("Clock Font", NORMAL_FONT), true);
            scene->add_effect<WeatherOverlay>(XPOS_CENTER, 0, scene->color("Weather Color", Colors::CYAN));
            break;

        case SceneId::JustSnow:
            scene->add_effect<ClearScreen>();
            scene->add_effect<ScrollText>(scene->text("Scroll Text", Config::DEFAULT_SCROLL), YPOS_CENTER, -10, scene->color("Scroll Color", Colors::RED), scene->font("Scroll Font", TINY_WIDE_FONT));
            scene->add_effect<SnowScape>(static_cast<uint8_t>(scene->number("Snow Flakes", 50, 0, 200)), static_cast<int16_t>(scene->number("Snow Height", 10, 1, 31)));
            scene->add_effect<WeatherOverlay>(XPOS_RIGHT, 0, scene->color("Weather Color", Colors::CYAN));
            break;

        case SceneId::Aquatic:
            scene->add_effect<ClearScreen>();
            scene->add_effect<ShowTime>(current_time, XPOS_CENTER, YPOS_CENTER, 0, 0, scene->color("Clock Color", Colors::ORANGE), scene->font("Clock Font", TINY_WIDE_FONT), true);
            scene->add_effect<BubblesInt>(static_cast<uint8_t>(scene->number("Bubbles", 16, 1, 100)));
            break;

        case SceneId::ValentinesDay:
            scene->add_effect<ClearScreen>();
            scene->add_effect<Hearts>(static_cast<uint8_t>(scene->number("Hearts", 16, 1, 100)));
            scene->add_effect<ShowTime>(current_time, XPOS_CENTER, YPOS_1_QUART, 0, 0, scene->color("Clock Color", Colors::PURPLE), scene->font("Clock Font", TINY_WIDE_FONT), true);
            scene->add_effect<ScrollText>(scene->text("Scroll Text", Config::DEFAULT_SCROLL), YPOS_3_QUART, -10, scene->color("Scroll Color", Colors::RED), scene->font("Scroll Font", TINY_WIDE_FONT));
            break;

        case SceneId::CubeEffect:
            scene->add_effect<ClearScreen>();
            scene->add_effect<Stars>(25);
            scene->add_effect<Cube3D>(8.5f, false);
            break;

        case SceneId::ChristmasDays:
            scene->add_effect<ClearScreen>();
            scene->add_effect<SnowScape>(static_cast<uint8_t>(scene->number("Snow Flakes", 20, 0, 200)), static_cast<int16_t>(scene->number("Snow Height", 8, 1, 31)));
            scene->add_effect<ShowTime>(current_time, XPOS_CENTER, YPOS_CENTER, 0, 0, scene->color("Clock Color", Colors::BLUE), scene->font("Clock Font", NORMAL_FONT), true);
            break;

        case SceneId::ChristmasEve:
            scene->add_effect<ClearScreen>();
            scene->add_effect<Present>(-20, 5, 5, Colors::GREEN, Colors::RED);
            scene->add_effect<Present>(-10, 7, 3, Colors::CYAN, Colors::RED);
            scene->add_effect<Present>(-15, 7, 5, Colors::WHITE, Colors::YELLOW);
            scene->add_effect<Present>(-35, 11, 9, Colors::PURPLE, Colors::GREEN);
            scene->add_effect<SnowScape>(static_cast<uint8_t>(scene->number("Snow Flakes", 20, 0, 200)), static_cast<int16_t>(scene->number("Snow Height", 8, 1, 31)));
            scene->add_effect<Hearts>(8);
            scene->add_effect<FlyingText>(scene->text("Message", "Merry Christmas"), 0, 0, 1, 1, scene->color("Message Color", Colors::RED), scene->font("Message Font", NORMAL_FONT));
            break;

        case SceneId::JanuaryDays:
            scene->add_effect<ClearScreen>();
            scene->add_effect<Stars>(30);
            scene->add_effect<ShootingStar>(-1000);
            scene->add_effect<SnowScape>(static_cast<uint8_t>(scene->number("Snow Flakes", 20, 0, 200)), static_cast<int16_t>(scene->number("Snow Height", 4, 1, 31)));
            scene->add_effect<ShowTime>(current_time, XPOS_CENTER, YPOS_CENTER, 0, 0, scene->color("Clock Color", Colors::BLUE), scene->font("Clock Font", NORMAL_FONT), true);
            scene->add_effect<ScrollText>(scene->text("Scroll Text", Config::DEFAULT_SCROLL), 21, -10, scene->color("Scroll Color", Colors::RED), scene->font("Scroll Font", TINY_WIDE_FONT));
            scene->add_effect<FlyingPng>("/bitmaps/ufo.png", XPOS_CENTER, YPOS_CENTER, 8, 0);
            break;

        case SceneId::Halloween:
            scene->add_effect<ClearScreen>();
            scene->add_effect<ShowTime>(current_time, XPOS_CENTER, 27, 0, 0, scene->color("Clock Color", Colors::RED), scene->font("Clock Font", TINY_WIDE_FONT), true);
            scene->add_effect<ScrollText>(scene->text("Scroll Text", "!!!HAPPY HALLOWEEN!!! --- \x81\x81\x81 --- "), YPOS_CENTER, -10, scene->color("Scroll Color", Colors::ORANGE), scene->font("Scroll Font", TINY_WIDE_FONT));
            scene->add_effect<FlyingPng>("/bitmaps/pumpkin.png", XPOS_CENTER, YPOS_CENTER, 16, 8);
            scene->add_effect<FlyingPng>("/bitmaps/pumpkin.png", 100, YPOS_CENTER, -15, -7);
            scene->add_effect<FlyingPng>("/bitmaps/ghost.png", XPOS_CENTER, YPOS_CENTER, 26, 1);
            scene->add_effect<Blood>();
            break;

        case SceneId::Birthday: {
            const int age = current_year - scene->number("Birth Year", 2016, 1900, 2100);
            scene->add_effect<ClearScreen>();
            scene->add_effect<SnowScape>(static_cast<uint8_t>(scene->number("Snow Flakes", 20, 0, 200)), static_cast<int16_t>(scene->number("Snow Height", 8, 1, 31)));
            scene->add_effect<Present>(-20, 5, 5, Colors::GREEN, Colors::RED);
            scene->add_effect<Present>(-10, 7, 3, Colors::CYAN, Colors::RED);
            scene->add_effect<Present>(-15, 7, 5, Colors::WHITE, Colors::YELLOW);
            scene->add_effect<Present>(-35, 11, 9, Colors::PURPLE, Colors::GREEN);
            scene->add_effect<FlyingText>(scene->text("Message", "Happy birthday"), 20, 10, -2, -1, scene->color("Message Color", Colors::RED), scene->font("Message Font", NORMAL_FONT));
            scene->add_effect<FlyingText>(scene->text("Name", "ERIK"), 0, 0, 1, 1, scene->color("Name Color", Colors::BLUE), scene->font("Name Font", NORMAL_FONT));
            scene->add_effect<FlyingText>(String(age) + " \x86r", 64, 16, -1, 1, scene->color("Age Color", Colors::CYAN), scene->font("Age Font", NORMAL_FONT));
            break;
        }

        case SceneId::NewYearsDay:
            scene->add_effect<ClearScreen>();
            scene->add_effect<Stars>();
            scene->add_effect<FireWorks>();
            scene->add_effect<FlyingText>(scene->text("Message", "Happy New Year"), XPOS_CENTER, 5, 0, 0, scene->color("Message Color", Colors::BLUE), scene->font("Message Font", NORMAL_FONT));
            scene->add_effect<FlyingText>(String(current_year + 1), XPOS_CENTER, 13, 0, 0, scene->color("Year Color", Colors::WHITE), scene->font("Year Font", NORMAL_FONT));
            scene->add_effect<ShowTime>(current_time, XPOS_CENTER, 21, 0, 0, scene->color("Clock Color", Colors::CYAN), scene->font("Clock Font", NORMAL_FONT));
            break;

        case SceneId::NewYearsMidnight:
            scene->add_effect<ClearScreen>();
            scene->add_effect<Stars>();
            scene->add_effect<FlyingPng>("/bitmaps/champagne.png", XPOS_CENTER, YPOS_CENTER);
            scene->add_effect<FireWorks>(-200);
            scene->add_effect<FireWorks>(-200);
            scene->add_effect<FireWorks>(-200);
            scene->add_effect<FlyingText>(scene->text("Message", "Happy New Year"), XPOS_CENTER, 12, 0, 0, scene->color("Message Color", Colors::BLUE), scene->font("Message Font", NORMAL_FONT));
            break;

        case SceneId::Test1:
            scene->add_effect<ClearScreen>();
            scene->add_effect<Border>(2, scene->color("Border Color 1", Colors::RED), scene->color("Border Color 2", Colors::YELLOW), BorderPattern::Solid);
            scene->add_effect<FlyingText>(scene->text("Label Text", "Test 1: Solid"), XPOS_CENTER, YPOS_CENTER, 0, 0, scene->color("Text Color", Colors::WHITE), scene->font("Text Font", TINY_WIDE_FONT));
            break;

        case SceneId::Test2:
            scene->add_effect<ClearScreen>();
            scene->add_effect<Border>(2, scene->color("Border Color 1", Colors::RED), scene->color("Border Color 2", Colors::WHITE), BorderPattern::Dashed, 4, 2);
            scene->add_effect<FlyingText>(scene->text("Label Text", "Test 2: Dashed"), XPOS_CENTER, YPOS_CENTER, 0, 0, scene->color("Text Color", Colors::WHITE), scene->font("Text Font", TINY_WIDE_FONT));
            break;

        case SceneId::Test3:
            scene->add_effect<ClearScreen>();
            scene->add_effect<Border>(2, scene->color("Border Color 1", Colors::CYAN), scene->color("Border Color 2", Colors::BLUE), BorderPattern::Chase, 8, 3);
            scene->add_effect<FlyingText>(scene->text("Label Text", "Test 3: Chase"), XPOS_CENTER, YPOS_CENTER, 0, 0, scene->color("Text Color", Colors::WHITE), scene->font("Text Font", TINY_WIDE_FONT));
            break;

        case SceneId::Test4:
            scene->add_effect<ClearScreen>();
            scene->add_effect<Border>(2, scene->color("Border Color 1", Colors::YELLOW), scene->color("Border Color 2", Colors::PURPLE), BorderPattern::Twinkle, 3, 2);
            scene->add_effect<FlyingText>(scene->text("Label Text", "Test 4: Twinkle"), XPOS_CENTER, YPOS_CENTER, 0, 0, scene->color("Text Color", Colors::WHITE), scene->font("Text Font", TINY_WIDE_FONT));
            break;

        case SceneId::Test5:
            scene->add_effect<ClearScreen>();
            scene->add_effect<Border>(2, scene->color("Border Color 1", Colors::WHITE), scene->color("Border Color 2", Colors::BLACK), BorderPattern::Rainbow, 4, 3);
            scene->add_effect<FlyingText>(scene->text("Label Text", "Test 5: Rainbow"), XPOS_CENTER, YPOS_CENTER, 0, 0, scene->color("Text Color", Colors::WHITE), scene->font("Text Font", TINY_WIDE_FONT));
            break;

        case SceneId::None:
        default:
            break;
    }

    return scene;
}

void renderScene(SceneId id)
{
    if (!active_scene || id != active_scene_id || WebUi::consumeSceneDirty()) {
        active_scene.reset(); // Release existing scene's RAM before allocating the next
        active_scene    = createScene(id, *dma_display);
        active_scene_id = id;
        SceneParams::publishActive(id, active_scene->params());
    }
    active_scene->render();
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

    // Initialize LittleFS filesystem
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed!");
    } else {
        Serial.println("LittleFS mounted successfully.");
    }

    // Configure native ESP32 SNTP with automatic CET/CEST Daylight Saving Time
    configTzTime(Config::TZ_INFO, Config::NTP_SERVER);

    // Start background weather polling task on Core 0
    Weather::begin();

    // Load NVS settings, connect Wi-Fi, and start the HTTP server
    WebUi::begin();

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

void drawCpuLoadBar(uint32_t render_us, uint32_t budget_us)
{
    if (!dma_display) return;

    const int16_t x = dma_display->width() - 1;
    const int16_t screen_h = dma_display->height();

    // Instantaneous load with slight smoothing (EMA) to prevent single-frame flicker
    static float smoothed_load = 0.0f;
    const float current_load = static_cast<float>(render_us) / static_cast<float>(budget_us);
    smoothed_load = (smoothed_load * 0.70f) + (current_load * 0.30f);

    int16_t bar_h = static_cast<int16_t>(lroundf(smoothed_load * screen_h));
    if (bar_h < 0) bar_h = 0;
    if (bar_h > screen_h) bar_h = screen_h;

    // Peak-hold dot tracking with decay
    static int16_t peak_dot = 0;
    static uint32_t last_peak_decay = 0;
    if (bar_h >= peak_dot) {
        peak_dot = bar_h;
    } else if (millis() - last_peak_decay >= 60) {
        last_peak_decay = millis();
        if (peak_dot > 0) peak_dot--;
    }

    // 1. Clear background of empty top section of meter column
    for (int16_t h = bar_h; h < screen_h; ++h) {
        const int16_t y = (screen_h - 1) - h;
        dma_display->drawPixel(x, y, Colors::BLACK);
    }

    // 2. Draw meter bar from bottom up (Green -> Yellow -> Red)
    for (int16_t h = 0; h < bar_h; ++h) {
        const int16_t y = (screen_h - 1) - h;
        uint16_t col;
        if (h >= 26) {
            col = Colors::RED;       // > ~80% load
        } else if (h >= 16) {
            col = Colors::YELLOW;    // 50% - 80% load
        } else {
            col = Colors::GREEN;     // < 50% load
        }
        dma_display->drawPixel(x, y, col);
    }

    // 3. Peak-hold indicator
    if (peak_dot > 0 && peak_dot <= screen_h) {
        const int16_t peak_y = (screen_h - 1) - (peak_dot - 1);
        dma_display->drawPixel(x, peak_y, (peak_dot >= 26) ? Colors::RED : Colors::WHITE);
    }
}

// =========================================================================
// Main Loop (~25 FPS)
// =========================================================================
void loop()
{
    const uint32_t frame_start = millis();
    const uint32_t frame_start_us = micros();

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
        if (const SceneId override_id = WebUi::selectedScene(); override_id != SceneId::None)
        {
            renderScene(override_id);
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
                    new FlyingText(*dma_display, ip.toString(), XPOS_CENTER, 0, 0, 0, Colors::GREEN, TINY_WIDE_FONT));
            }
            ip_overlay->render();
        }
    }

    if (Config::SHOW_CPU_LOAD_BAR) {
        const uint32_t render_us = micros() - frame_start_us;
        const uint32_t budget_us = Config::FRAME_TIME_MS * 1000;
        drawCpuLoadBar(render_us, budget_us);
    }

    dma_display->flipDMABuffer();
    WebUi::handleClient();

    const uint32_t elapsed = millis() - frame_start;
    if (elapsed < Config::FRAME_TIME_MS) {
        BlinkLed(frame_start, false);
        delay(Config::FRAME_TIME_MS - elapsed);
    } else {
        //Serial.println("Rendering took too long");
        BlinkLed(frame_start, true);
    }
}
