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
#include "effects/GolfHoleInOne.h"
#include "effects/SDFEffect.h"

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

    constexpr uint8_t  BRIGHTNESS      = 32;
    constexpr uint32_t FRAME_TIME_MS   = 40; // ~25 FPS
    constexpr uint32_t FRAME_BUDGET_US = FRAME_TIME_MS * 1000UL;
    constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000; // Fallback to SetupAp after 15s

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

        case SceneId::SetupAp: {
            const String wifi_qr = String("WIFI:T:WPA;S:") + WebUi::apSsid() + ";P:" + WebUi::apPassword() + ";;";
            scene->add_effect<ClearScreen>();
            scene->add_effect<QrCode>(wifi_qr, 1, YPOS_CENTER, Colors::BLACK, color565(255, 255, 255), 1);
            if (WebUi::wifiError().length() > 0) {
                scene->add_effect<FlyingText>(WebUi::wifiError(), 35, 0, 0, 0, Colors::RED, TINY_WIDE_FONT);
            }
            scene->add_effect<FlyingText>(String("SSID : ") + WebUi::apSsid(), 35, 8, 0, 0, Colors::WHITE, TINY_WIDE_FONT);
            scene->add_effect<FlyingText>(String("Pass : ") + WebUi::apPassword(), 35, 16, 0, 0, Colors::WHITE, TINY_WIDE_FONT);
            scene->add_effect<FlyingText>(String("IpAdr: ") + WiFi.softAPIP().toString(), 35, 24, 0, 0, Colors::CYAN, TINY_WIDE_FONT);
            break;
        }

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

        case SceneId::TrackmanTM5Beta:
            scene->add_effect<GolfHoleInOne>();
            scene->add_effect<FlyingText>(scene->text("Title", "Countdown to TM5 Beta"), XPOS_CENTER, 0, 0, 0, scene->color("Title Color", Colors::BLACK), scene->font("Title Font", TINY_WIDE_FONT));
            scene->add_effect<ShowTime>(current_time, XPOS_CENTER, YPOS_3_QUART, 0, 0, scene->color("Clock Color", Colors::ORANGE), scene->font("Clock Font", NORMAL_FONT), "dd hh:mm:ss", "2026-12-01 00:00:00", "Beta release!!");
//          scene->add_effect<FlyingPng>("/bitmaps/trackman.png", 10, 4, 8, 4, true, false);
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
            scene->add_effect<ShowTime>(current_time, XPOS_CENTER, YPOS_CENTER, 0, 0, scene->color("Clock Color", Colors::BLUE), scene->font("Clock Font", NORMAL_FONT), "yyyy-mm-dd hh:mm:ss");
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
            scene->add_effect<ScrollText>(scene->text("Scroll Text", Config::DEFAULT_SCROLL), YPOS_BOTTOM, -10, scene->color("Scroll Color", Colors::RED), scene->font("Scroll Font", TINY_WIDE_FONT));
            scene->add_effect<ShowTime>(current_time, XPOS_CENTER, YPOS_CENTER, 0, 0, scene->color("Clock Color", Colors::BLUE), scene->font("Clock Font", NORMAL_FONT), "yyyy-mm-dd hh:mm:ss");
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
            scene->add_effect<ShowTime>(current_time, XPOS_CENTER, YPOS_CENTER, 0, 0, scene->color("Clock Color", Colors::ORANGE), scene->font("Clock Font", TINY_WIDE_FONT), "yyyy-mm-dd hh:mm:ss");
            scene->add_effect<BubblesInt>(static_cast<uint8_t>(scene->number("Bubbles", 16, 1, 100)));
            break;

        case SceneId::ValentinesDay:
            scene->add_effect<ClearScreen>();
            scene->add_effect<Hearts>(static_cast<uint8_t>(scene->number("Hearts", 16, 1, 100)));
            scene->add_effect<ShowTime>(current_time, XPOS_CENTER, YPOS_1_QUART, 0, 0, scene->color("Clock Color", Colors::PURPLE), scene->font("Clock Font", TINY_WIDE_FONT), "yyyy-mm-dd hh:mm:ss");
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
            scene->add_effect<ShowTime>(current_time, XPOS_CENTER, YPOS_CENTER, 0, 0, scene->color("Clock Color", Colors::BLUE), scene->font("Clock Font", NORMAL_FONT), "yyyy-mm-dd hh:mm:ss");
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
            scene->add_effect<ShowTime>(current_time, XPOS_CENTER, YPOS_CENTER, 0, 0, scene->color("Clock Color", Colors::BLUE), scene->font("Clock Font", NORMAL_FONT), "yyyy-mm-dd hh:mm:ss");
            scene->add_effect<ScrollText>(scene->text("Scroll Text", Config::DEFAULT_SCROLL), YPOS_3_QUART, -10, scene->color("Scroll Color", Colors::RED), scene->font("Scroll Font", TINY_WIDE_FONT));
            scene->add_effect<FlyingPng>("/bitmaps/ufo.png", XPOS_CENTER, YPOS_1_QUART, 8, 0);
            break;

        case SceneId::Halloween:
            scene->add_effect<ClearScreen>();
            scene->add_effect<ShowTime>(current_time, XPOS_CENTER, 27, 0, 0, scene->color("Clock Color", Colors::RED), scene->font("Clock Font", TINY_WIDE_FONT), "yyyy-mm-dd hh:mm:ss");
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
            scene->add_effect<FlyingText>(scene->text("Message", "Happy New Year"), XPOS_CENTER, YPOS_1_QUART, 0, 0, scene->color("Message Color", Colors::BLUE), scene->font("Message Font", NORMAL_FONT));
            scene->add_effect<FlyingText>(String(current_year), XPOS_CENTER, YPOS_CENTER, 0, 0, scene->color("Year Color", Colors::WHITE), scene->font("Year Font", NORMAL_FONT));
            scene->add_effect<ShowTime>(current_time, XPOS_CENTER, YPOS_3_QUART, 0, 0, scene->color("Clock Color", Colors::CYAN), scene->font("Clock Font", NORMAL_FONT));
            break;

        case SceneId::NewYearsMidnight:
            scene->add_effect<ClearScreen>();
            scene->add_effect<Stars>();
            scene->add_effect<FlyingPng>("/bitmaps/champagne.png", XPOS_CENTER, YPOS_CENTER);
            scene->add_effect<FireWorks>(-200);
            scene->add_effect<FireWorks>(-200);
            scene->add_effect<FireWorks>(-200);
            scene->add_effect<FlyingText>(scene->text("Message", "Happy New Year"), XPOS_CENTER, YPOS_1_THIRD, 0, 0, scene->color("Message Color", Colors::BLUE), scene->font("Message Font", NORMAL_FONT));
            scene->add_effect<FlyingText>(String(current_year), XPOS_CENTER, YPOS_2_THIRD, 0, 0, scene->color("Year Color", Colors::WHITE), scene->font("Year Font", NORMAL_FONT));
            break;

        case SceneId::BorderTest: {
            const uint8_t pattern_idx = scene->select("Border Type", static_cast<uint8_t>(BorderPattern::Solid), {
                "Solid", "Dashed", "Chase", "Twinkle", "Rainbow"
            });
            scene->add_effect<ClearScreen>();
            scene->add_effect<Border>(static_cast<uint8_t>(scene->number("Thickness", 2, 1, 8)), scene->color("Border Color 1", Colors::RED), scene->color("Border Color 2", Colors::YELLOW), static_cast<BorderPattern>(pattern_idx), static_cast<uint8_t>(scene->number("Stripe Length", 4, 1, 32)), static_cast<int8_t>(scene->number("Speed", 2, -16, 16)));
            scene->add_effect<FlyingText>(scene->text("Label Text", "Border Test"), XPOS_CENTER, YPOS_CENTER, 0, 0, scene->color("Text Color", Colors::WHITE), scene->font("Text Font", TINY_WIDE_FONT));
            break;
        }

        case SceneId::ScrollerTest:
            scene->add_effect<ClearScreen>();
            scene->add_effect<ScrollText>(scene->text("Scroll Text", Config::DEFAULT_SCROLL), YPOS_CENTER, static_cast<int16_t>(scene->number("Scroll Speed", -10, -50, 50)), scene->color("Scroll Color", Colors::GREY), scene->font("Scroll Font", SPACE_GROTESK_32_FONT));
            break;

        case SceneId::TestAnimatedPng:
            scene->add_effect<ClearScreen>(0, 14, 28);
            scene->add_effect<FlyingPng>("/bitmaps/koi.png", XPOS_CENTER, YPOS_2_THIRD, 9, -3, true, false, true);
            scene->add_effect<FlyingPng>("/bitmaps/koi.png", XPOS_CENTER, YPOS_CENTER, 7, 1, true, false, true);
            scene->add_effect<FlyingPng>("/bitmaps/koi.png", XPOS_1_QUART, YPOS_1_THIRD, 8, 2, true, false, true);
            scene->add_effect<BubblesInt>(static_cast<uint8_t>(scene->number("Bubbles", 8, 1, 50)));
            break;

        case SceneId::SDFTest: {
            static constexpr const SdfAtlas* SDF_ATLASES[] = {
                &SDF_STAR, &SDF_SHAMROCK, &SDF_DIAMOND, &SDF_HEART
            };
            const uint8_t atlas_idx = scene->select("Atlas", 0, {
                "Star", "Shamrock", "Diamond", "Heart"
            });
            const SdfAtlas& atlas = *SDF_ATLASES[atlas_idx < 4 ? atlas_idx : 0];
            const uint16_t color = scene->color("Color", Colors::YELLOW);
            const float scale = static_cast<float>(scene->number("Scale (%)", 100, 25, 200)) / 100.0f;
            const float spin = static_cast<float>(scene->number("Spin Speed", 2, -15, 15));
            const float outline = static_cast<float>(scene->number("Outline (x0.5px)", 3, 0, 12)) * 0.5f;
            scene->add_effect<ClearScreen>();
            scene->add_effect<SDFEffect>(atlas, color, XPOS_CENTER, YPOS_CENTER, scale, 0.0f, spin, 0, 0, outline);
            scene->add_effect<SDFEffect>(SDF_STAR, Colors::YELLOW, XPOS_1_QUART, YPOS_CENTER, scale, 0.0f, spin, 7, 3, outline);
            scene->add_effect<SDFEffect>(SDF_HEART, Colors::RED, XPOS_3_QUART, YPOS_CENTER, scale, 0.0f, spin, -9, -3, outline);
            scene->add_effect<SDFEffect>(SDF_SHAMROCK, Colors::GREEN, XPOS_CENTER, YPOS_CENTER, scale, 0.0f, spin, -5, 5, outline);
            break;
        }

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
    static uint32_t connect_start_ms = 0;

    switch (WiFi.status())
    {
        case WL_CONNECTED:
            connect_start_ms = 0;
            return SceneId::None;

        case WL_IDLE_STATUS:
            if (connect_start_ms == 0) connect_start_ms = millis();
            return SceneId::Welcome;

        case WL_NO_SSID_AVAIL:
            connect_start_ms = 0;
            WebUi::startAccessPoint("Couldn't find SSID");
            active_scene_id = SceneId::None;
            return SceneId::SetupAp;

        case WL_CONNECT_FAILED:
            connect_start_ms = 0;
            WebUi::startAccessPoint("Bad password!");
            active_scene_id = SceneId::None;
            return SceneId::SetupAp;

        case WL_CONNECTION_LOST:
            connect_start_ms = 0;
            WebUi::startAccessPoint("Connection lost!");
            active_scene_id = SceneId::None;
            return SceneId::SetupAp;

        case WL_DISCONNECTED:
            if (connect_start_ms == 0) {
                connect_start_ms = millis();
            } else if (millis() - connect_start_ms >= Config::WIFI_CONNECT_TIMEOUT_MS) {
                connect_start_ms = 0;
                WebUi::startAccessPoint("Wi-Fi timeout");
                active_scene_id = SceneId::None;
                return SceneId::SetupAp;
            }
            return SceneId::Connecting;

        case WL_NO_SHIELD:
        case WL_SCAN_COMPLETED:
            connect_start_ms = 0;
            return SceneId::WifiUnknown;

        case WL_STOPPED:
            connect_start_ms = 0;
            return SceneId::WifiStopped;
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
        WebUi::panelChain()
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
    dma_display->setBrightness8(WebUi::brightness());
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

void blinkLed(bool overloaded)
{
    static uint32_t last_toggle_ms = 0;
    static bool led_on = false;

    if (!overloaded) {
        if (led_on) {
            led_on = false;
            rgbLedWrite(Config::LED_PIN, 0, 0, 0);
        }
        return;
    }

    const uint32_t now_ms = millis();
    if (now_ms - last_toggle_ms >= 200) {
        last_toggle_ms = now_ms;
        led_on = !led_on;
        rgbLedWrite(Config::LED_PIN, led_on ? 30 : 0, 0, 0);
    }
}

void drawCpuLoadBar(uint32_t render_us)
{
    if (!dma_display) return;

    const int16_t x        = dma_display->width() - 1;
    const int16_t screen_h = dma_display->height();

    // Smooth instantaneous load (EMA) to prevent single-frame flicker
    static float smoothed_load = 0.0f;
    const float current_load   = static_cast<float>(render_us) / static_cast<float>(Config::FRAME_BUDGET_US);
    smoothed_load              = (smoothed_load * 0.70f) + (current_load * 0.30f);

    int16_t bar_h = static_cast<int16_t>(lroundf(smoothed_load * screen_h));
    bar_h         = constrain(bar_h, static_cast<int16_t>(0), screen_h);

    // Peak-hold dot with 60ms decay
    static int16_t  peak_dot        = 0;
    static uint32_t last_peak_decay = 0;
    if (bar_h >= peak_dot) {
        peak_dot = bar_h;
    } else if (millis() - last_peak_decay >= 60) {
        last_peak_decay = millis();
        if (peak_dot > 0) peak_dot--;
    }

    // 1. Clear empty top portion
    if (bar_h < screen_h) {
        dma_display->drawFastVLine(x, 0, screen_h - bar_h, Colors::BLACK);
    }

    // 2. Draw Green (<50%), Yellow (50-80%), Red (>80%) segments bottom-up
    const int16_t yellow_start = screen_h / 2;          // 16
    const int16_t red_start    = (screen_h * 8) / 10;   // 25

    const int16_t green_h = min(bar_h, yellow_start);
    if (green_h > 0) {
        dma_display->drawFastVLine(x, screen_h - green_h, green_h, Colors::GREEN);
    }
    if (bar_h > yellow_start) {
        const int16_t yellow_h = min(bar_h, red_start) - yellow_start;
        if (yellow_h > 0) {
            dma_display->drawFastVLine(x, screen_h - yellow_start - yellow_h, yellow_h, Colors::YELLOW);
        }
    }
    if (bar_h > red_start) {
        const int16_t red_h = bar_h - red_start;
        dma_display->drawFastVLine(x, screen_h - bar_h, red_h, Colors::RED);
    }

    // 3. Peak-hold dot
    if (peak_dot > 0 && peak_dot <= screen_h) {
        dma_display->drawPixel(x, screen_h - peak_dot, (peak_dot > red_start) ? Colors::RED : Colors::WHITE);
    }
}

void drawMemBar()
{
    if (!dma_display) return;

    const int16_t x        = dma_display->width() - 2;
    const int16_t screen_h = dma_display->height();

    const uint32_t total_heap = ESP.getHeapSize();
    const uint32_t free_heap  = ESP.getFreeHeap();
    const uint32_t used_heap  = (total_heap > free_heap) ? (total_heap - free_heap) : 0;
    const float    mem_ratio  = (total_heap > 0)
        ? (static_cast<float>(used_heap) / static_cast<float>(total_heap))
        : 0.0f;

    int16_t bar_h = static_cast<int16_t>(lroundf(mem_ratio * screen_h));
    bar_h         = constrain(bar_h, static_cast<int16_t>(0), screen_h);

    if (bar_h < screen_h) {
        dma_display->drawFastVLine(x, 0, screen_h - bar_h, Colors::BLACK);
    }
    if (bar_h > 0) {
        dma_display->drawFastVLine(x, screen_h - bar_h, bar_h, Colors::BLUE);
    }
}

// =========================================================================
// Main Loop (~25 FPS)
// =========================================================================
void loop()
{
    const uint32_t frame_start_us = micros();

    if (WebUi::isApMode()) {
        renderScene(SceneId::SetupAp);
    } else if (SceneId id = checkWifiState(); id != SceneId::None) {
        renderScene(id);
    } else {
        const time_t now = time(nullptr);
        localtime_r(&now, &current_time);
        const bool    has_valid_time = (current_time.tm_year >= 120);
        const SceneId override_id    = WebUi::selectedScene();
        const SceneId auto_id        = has_valid_time ? selectDateScene(current_time) : SceneId::Welcome;
        renderScene((override_id != SceneId::None) ? override_id : auto_id);

        if (WebUi::showIp()) {
            const IPAddress ip = WiFi.localIP();
            if (!ip_overlay || ip != cached_ip) {
                cached_ip  = ip;
                ip_overlay = std::unique_ptr<FlyingText>(
                    new FlyingText(*dma_display, ip.toString(), XPOS_CENTER, 0, 0, 0, Colors::GREEN, ZOLO_MICRO_FONT));
            }
            ip_overlay->render();
        }
    }

    static uint8_t clear_cpu_frames = 0;
    static bool    prev_show_cpu    = true;
    const bool     show_cpu         = WebUi::showCpuLoad();
    if (prev_show_cpu && !show_cpu) {
        clear_cpu_frames = 2;
    }
    prev_show_cpu = show_cpu;

    if (show_cpu) {
        drawMemBar();
        drawCpuLoadBar(micros() - frame_start_us);
    } else if (clear_cpu_frames > 0) {
        dma_display->drawFastVLine(dma_display->width() - 2, 0, dma_display->height(), Colors::BLACK);
        dma_display->drawFastVLine(dma_display->width() - 1, 0, dma_display->height(), Colors::BLACK);
        --clear_cpu_frames;
    }

    static uint8_t prev_brightness = WebUi::brightness();
    const uint8_t  cur_brightness  = WebUi::brightness();
    if (cur_brightness != prev_brightness) {
        prev_brightness = cur_brightness;
        dma_display->setBrightness8(cur_brightness);
    }

    FastHub75::applyPanelCalibration(
        *dma_display,
        WebUi::panelCalibrations(),
        static_cast<size_t>(dma_display->width() / Config::PANEL_RES_X),
        Config::PANEL_RES_X
    );

    dma_display->flipDMABuffer();
    WebUi::handleClient();

    const uint32_t frame_us = micros() - frame_start_us;
    if (frame_us < Config::FRAME_BUDGET_US) {
        blinkLed(false);
        const uint32_t remaining_ms = (Config::FRAME_BUDGET_US - frame_us) / 1000UL;
        if (remaining_ms > 0) {
            delay(remaining_ms);
        }
    } else {
        blinkLed(true);
    }
}
