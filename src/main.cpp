#include <Arduino.h>
#include <Wire.h>
#include <memory>
#include <time.h>

#include <WiFi.h>
#include <WebServer.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

// Custom Scene & Effect Headers
#include "Scene.h"
#include "ClearScreen.h"
#include "SnowScape.h"
#include "Stars.h"
#include "FunkyLine.h"
#include "Present.h"
#include "FlyingText.h"
#include "Rocket.h"
#include "ShowTime.h"
#include "Bitmap.h"
#include "ShootingStar.h"
#include "Circle.h"
#include "Scrolltext.h"
#include "Blood.h"
#include "BubblesInt.h"

#include "ufo.h"
#include "pumpkin.h"
#include "ghost.h"

const uint16_t newyear_bitmap[] = {
    #include "champagne.h"
};

// =========================================================================
// Hardware & Pin Configuration (ESP32-S3 SuperMini)
// =========================================================================
#define LED_PIN     48

#define PANEL_RES_X 64
#define PANEL_RES_Y 32
#define PANEL_CHAIN 2   // 2 chained panels = 128x32

#define PIN_R1      44  // Routed to RX (GPIO 44)
#define PIN_G1      13
#define PIN_B1      1
#define PIN_R2      2
#define PIN_G2      12
#define PIN_B2      3

#define PIN_A       4
#define PIN_B       11
#define PIN_C       5
#define PIN_D       10
#define PIN_E       -1  // GND (1/16 scan)

#define PIN_CLK     6
#define PIN_LAT     9
#define PIN_OE      7

MatrixPanel_I2S_DMA *dma_display = nullptr;

// =========================================================================
// Network & NTP Configuration
// =========================================================================
WebServer server(80);

const char* ap_ssid     = "ESP8285-Display";
const char* ap_password = "123456789";

String wifi_ssid        = "bandit";
String wifi_password    = "limfjordenerblaa";

const long utcOffsetInSeconds = 3600 * 1;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);
time_t rawtime = 0;
struct tm* ti = nullptr;

static bool is_ap        = false;
static bool first_access = false;
static bool test_mode    = false;
static String wifi_err   = "";

// =========================================================================
// Colors & Scenes (Initialized after dma_display->begin())
// =========================================================================
uint16_t myBLACK, myWHITE, myRED, myORANGE, myYELLOW;
uint16_t myGREEN, myCYAN, myBLUE, myPURPLE, myGREY;

std::shared_ptr<ScrollText> scroller;
std::shared_ptr<SnowScape> shared_snow;

Scene normal_days;
Scene just_snow;
Scene christmas_days;
Scene christmas_eve;
Scene january_days;
Scene halloween;
Scene erik_birthday;
Scene newyears_day;
Scene newyears_midnight;
Scene testScene;
Scene welcome;
Scene connecting;
Scene setup_ap;
Scene aquatic;

void initScenes()
{
    MatrixPanel_I2S_DMA& display = *dma_display;

    myBLACK  = color565(0, 0, 0);
    myWHITE  = color565(255, 255, 255);
    myRED    = color565(255, 0, 0);
    myORANGE = color565(255, 128, 0);
    myYELLOW = color565(255, 255, 0);
    myGREEN  = color565(0, 255, 0);
    myCYAN   = color565(0, 255, 255);
    myBLUE   = color565(0, 0, 255);
    myPURPLE = color565(255, 0, 255);
    myGREY   = color565(127, 127, 127);

    scroller = std::make_shared<ScrollText>(
        display,
        "put your text here now --- \x80\x80\x80 --- ",
        6, -10, myRED, TINY_WIDE_FONT
    );

    shared_snow = std::make_shared<SnowScape>(display);

    normal_days = {
        std::make_shared<ClearScreen>(display),
        scroller,
        std::make_shared<ShowTime>(display, *ti, -1, -1, 0, 0, myBLUE, NORMAL_FONT, true)
    };

    just_snow = {
        std::make_shared<ClearScreen>(display),
        scroller,
        std::make_shared<SnowScape>(display, 50, 10)
    };

    christmas_days = {
        std::make_shared<ClearScreen>(display),
        shared_snow,
        std::make_shared<ShowTime>(display, *ti, -1, -1, 0, 0, myBLUE, NORMAL_FONT, true)
    };

    christmas_eve = {
        std::make_shared<ClearScreen>(display),
        shared_snow,
        std::make_shared<FlyingText>(display, "Merry X-Mas", 0, 0, 1, 1, myBLUE)
    };

    january_days = {
        std::make_shared<ClearScreen>(display),
        std::make_shared<Stars>(display, 30),
        std::make_shared<ShootingStar>(display, -1000),
        std::make_shared<SnowScape>(display, 20, 4),
        std::make_shared<ShowTime>(display, *ti, -1, -1, 0, 0, myBLUE, NORMAL_FONT, true),
        scroller,
        std::make_shared<Bitmap>(display, (const uint16_t&)ufo_bitmap, 16, 10, -1, -1, 8, 0, ufo_mask)
    };

    halloween = {
        std::make_shared<ClearScreen>(display),
        std::make_shared<ShowTime>(display, *ti, -1, 27, 0, 0, myRED, TINY_WIDE_FONT, true),
        std::make_shared<ScrollText>(display, "!!!HAPPY HALLOWEEN!!! --- \x81\x81\x81 --- ", -1, -10, myORANGE, TINY_WIDE_FONT),
        std::make_shared<Bitmap>(display, (const uint16_t&)pumpkin_bitmap, 19, 18, -1, -1, 16, 8, pumpkin_mask),
        std::make_shared<Bitmap>(display, (const uint16_t&)pumpkin_bitmap, 19, 18, 100, -1, -15, -7, pumpkin_mask),
        std::make_shared<Bitmap>(display, (const uint16_t&)ghost_bitmap, 33, 27, -3, -1, 26, 1, ghost_mask),
        std::make_shared<Blood>(display)
    };

    erik_birthday = {
        std::make_shared<ClearScreen>(display),
        shared_snow,
        std::make_shared<Present>(display, -20, 5, 5, myGREEN, myRED),
        std::make_shared<Present>(display, -10, 7, 3, myCYAN, myRED),
        std::make_shared<Present>(display, -15, 7, 5, myWHITE, myYELLOW),
        std::make_shared<Present>(display, -35, 11, 9, myPURPLE, myGREEN),
        std::make_shared<FlyingText>(display, "Happy birthday", 20, 10, -2, -1, myRED),
        std::make_shared<FlyingText>(display, "ERIK", 0, 0, 1, 1, myBLUE),
        std::make_shared<FlyingText>(display, "3 \x86r", 64, 16, -1, 1, myCYAN)
    };

    newyears_day = {
        std::make_shared<ClearScreen>(display),
        std::make_shared<Stars>(display),
        std::make_shared<Rocket>(display),
        std::make_shared<FlyingText>(display, "Happy New Year", -1, 5, 0, 0, myBLUE),
        std::make_shared<FlyingText>(display, "2020", -1, 13, 0, 0, myWHITE),
        std::make_shared<ShowTime>(display, *ti, -1, 21, 0, 0, myCYAN)
    };

    newyears_midnight = {
        std::make_shared<ClearScreen>(display),
        std::make_shared<Stars>(display),
        std::make_shared<Bitmap>(display, (const uint16_t&)newyear_bitmap, 34, 32),
        std::make_shared<Rocket>(display, -200),
        std::make_shared<Rocket>(display, -200),
        std::make_shared<Rocket>(display, -200),
        std::make_shared<FlyingText>(display, "Happy New Year", 24, 12, 0, 0, myBLUE)
    };

    testScene = {
        std::make_shared<ClearScreen>(display),
        std::make_shared<Stars>(display),
        std::make_shared<FunkyLine>(display, 0, 3, color565(255, 0, 0)),
        std::make_shared<FunkyLine>(display, 5, 3, color565(255, 100, 0)),
        std::make_shared<FunkyLine>(display, 10, 3, color565(255, 200, 100)),
        std::make_shared<FunkyLine>(display, 15, 3, color565(255, 255, 200)),
        std::make_shared<FunkyLine>(display, 20, 3, color565(255, 255, 255)),
        std::make_shared<FlyingText>(display, "*", 20, 10, -2, -1, myRED),
        std::make_shared<FlyingText>(display, "+", 100, 20, 1, -2, myGREEN),
        std::make_shared<FlyingText>(display, "o", 80, 24, 2, -2, myCYAN),
        std::make_shared<ShowTime>(display, *ti, -1, -1, 0, 0, myBLUE, NORMAL_FONT, true),
        std::make_shared<Circle>(display),
        std::make_shared<Circle>(display, 80, 20, 12, myBLUE),
        std::make_shared<ScrollText>(
            display,
            "\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2A\x2B\x2C\x2D\x2E\x2F"
            "\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3A\x3B\x3C\x3D\x3E\x3F"
            "\x40\x41\x42\x43\x44\x45\x46\x47\x48\x49\x4A\x4B\x4C\x4D\x4E\x4F"
            "\x50\x51\x52\x53\x54\x55\x56\x57\x58\x59\x5A\x5B\x5C\x5D\x5E\x5F"
            "\x60\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6A\x6B\x6C\x6D\x6E\x6F"
            "\x70\x71\x72\x73\x74\x75\x76\x77\x78\x79\x7A\x7B\x7C\x7D\x7E\x7F"
            "\x80\x81    ",
            22, -5, myRED, TINY_WIDE_FONT
        )
    };

    welcome = {
        std::make_shared<ClearScreen>(display),
        std::make_shared<FlyingText>(display, "*********************", 2,  0, 0, 0, myWHITE),
        std::make_shared<FlyingText>(display, "*                   *", 2,  8, 0, 0, myWHITE),
        std::make_shared<FlyingText>(display, "-OCCATION DISPLAY-", -1, -1, 0, 0, myRED),
        std::make_shared<FlyingText>(display, "*                   *", 2, 16, 0, 0, myWHITE),
        std::make_shared<FlyingText>(display, "*********************", 2, 24, 0, 0, myWHITE)
    };

    connecting = {
        std::make_shared<ClearScreen>(display),
        std::make_shared<FlyingText>(display, "Connecting...", -1, -1, 0, 0, myORANGE, TINY_WIDE_FONT)
    };

    setup_ap = {
        std::make_shared<ClearScreen>(display),
        std::make_shared<FlyingText>(display, "SSID : " + String(ap_ssid), 8, 13, 0, 0, myWHITE),
        std::make_shared<FlyingText>(display, "Password : " + String(ap_password), 8, 19, 0, 0, myWHITE),
        std::make_shared<FlyingText>(display, "Config : 192.168.4.1", 8, 25, 0, 0, myWHITE)
    };

    aquatic = {
        std::make_shared<ClearScreen>(display),
        std::make_shared<ShowTime>(display, *ti, -1, -1, 0, 0, myORANGE, TINY_WIDE_FONT, true),
        std::make_shared<BubblesInt>(display, 8)
    };
}

// =========================================================================
// Web Server Handlers
// =========================================================================
void handleRootHttp()
{
    if (is_ap) {
        Serial.println("im ap");
        is_ap = false;
        first_access = false;
        WiFi.disconnect();
        WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
    } else {
        first_access = true;
    }

    server.send(200, "text/html",
        "<!DOCTYPE html><html><body><h1>Enter scroll text here:</h1>"
        "<form id='form'><input id='text'><br></form><script>"
        "document.getElementById('form').addEventListener('submit', function (se) {"
        "var http = new XMLHttpRequest();"
        "http.open('POST', '/scroller', true);"
        "http.setRequestHeader('Content-type','application/x-www-form-urlencoded');"
        "var params = 'text=' + document.getElementById('text').value + '';"
        "http.send(params);"
        "se.preventDefault();});</script></body></html>"
    );
}

// =========================================================================
// Setup
// =========================================================================
void setup()
{
    Serial.begin(115200);
    delay(1000);
    pinMode(LED_PIN, OUTPUT);
    pinMode(PIN_R1, OUTPUT);
    Serial.println("HELLO!");

    // Initialize time struct before Scene construction
    ti = localtime(&rawtime);

    // Configure HUB75 DMA Display
    HUB75_I2S_CFG mxconfig(
        PANEL_RES_X,
        PANEL_RES_Y,
        PANEL_CHAIN
    );

    mxconfig.gpio.r1  = PIN_R1;
    mxconfig.gpio.g1  = PIN_G1;
    mxconfig.gpio.b1  = PIN_B1;
    mxconfig.gpio.r2  = PIN_R2;
    mxconfig.gpio.g2  = PIN_G2;
    mxconfig.gpio.b2  = PIN_B2;
    mxconfig.gpio.a   = PIN_A;
    mxconfig.gpio.b   = PIN_B;
    mxconfig.gpio.c   = PIN_C;
    mxconfig.gpio.d   = PIN_D;
    mxconfig.gpio.e   = PIN_E;
    mxconfig.gpio.lat = PIN_LAT;
    mxconfig.gpio.oe  = PIN_OE;
    mxconfig.gpio.clk = PIN_CLK;

    mxconfig.double_buff = true;
    mxconfig.min_refresh_rate = 120;

    // 1. Invert clock phase so data is sampled on the stable edge
    mxconfig.clkphase = false; // Default is true — toggle this first!
    // 2. Increase latch blanking (1 to 4) to prevent edge pixel clipping/ghosting
    mxconfig.latch_blanking = 2;
    // 3. Lower I2S clock speed to 10 MHz to eliminate ribbon-cable propagation skew
    mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;

    dma_display = new MatrixPanel_I2S_DMA(mxconfig);
    dma_display->begin();
    dma_display->setBrightness8(32);

    // Brief startup test pattern
    dma_display->fillScreen(color565(0, 0, 0));
//    dma_display->drawRect(0, 0, dma_display->width(), dma_display->height(), color565(128, 128, 128));

    if (psramFound()) {
        Serial.print("PSRAM size: ");
        Serial.println(ESP.getPsramSize());
    } else {
        Serial.println("PSRAM not detected!");
    }

    // Build all scenes using the active DMA display instance
    initScenes();

    // Start Wi-Fi & Web Server
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
//    WiFi.setSleep(false);                     // Prevent modem sleep from dropping incoming HTTP requests
//    WiFi.setTxPower(WIFI_POWER_8_5dBm);       // Fix ceramic antenna saturation on S3 SuperMini

    server.on("/", handleRootHttp);
    server.on("/scroller", []() {
        if (server.arg(0).length() > 1 && server.arg(0).length() < 5) {
            test_mode = true;
        } else {
            test_mode = false;
            scroller->setText(server.arg(0));
        }
        server.send(200, "text/plain", "!");
    });
    server.begin();

    Serial.println("Start...");
}

// =========================================================================
// Wi-Fi & Scene State Machine
// =========================================================================
void wifi_connected()
{
    MatrixPanel_I2S_DMA& display = *dma_display;

    switch (WiFi.status())
    {
        case WL_CONNECTED:
        {
            static bool first_connect = true;
            if (first_connect) {
                timeClient.begin();
                first_connect = false;
            }

            timeClient.update();
            rawtime = timeClient.getEpochTime();
            struct tm* updated_tm = localtime(&rawtime);
            if (updated_tm != nullptr && ti != nullptr) {
                *ti = *updated_tm; // Copy into the struct referenced by ShowTime instances
            }

            uint8_t month = ti->tm_mon + 1;
            uint8_t day   = ti->tm_mday;
            uint8_t hours = ti->tm_hour;

            if (test_mode) {
                testScene.render();
            }
            else if (month == 1 && day == 1 && hours < 12) {
                newyears_midnight.render();
            }
            else if (month == 1) {
                january_days.render();
            }
            else if (month == 2 && day == 14) {
                aquatic.render();
            }
            else if ((month == 10 && day > 15) || (month == 11 && day == 1 && hours < 8)) {
                halloween.render();
            }
            else if (month == 11) {
                just_snow.render();
            }
            else if (month == 12 && day == 24) {
                christmas_eve.render();
            }
            else if (month == 12 && day == 30) {
                erik_birthday.render();
            }
            else if (month == 12 && day == 31) {
                newyears_day.render();
            }
            else if (month == 12) {
                christmas_days.render();
            }
            else {
                normal_days.render();
            }

            if (!first_access) {
                FlyingText text(display, WiFi.localIP().toString(), -1, 0, 0, 0, myGREEN, TINY_WIDE_FONT);
                text.render();
            }
            break;
        }

        case WL_IDLE_STATUS:
            welcome.render();
            break;

        case WL_NO_SSID_AVAIL:
            wifi_err = "Couldn't find SSID";
            WiFi.disconnect();
            WiFi.softAP(ap_ssid, ap_password, 1, false, 1);
            is_ap = true;
            break;

        case WL_CONNECT_FAILED:
            wifi_err = "Bad password!";
            WiFi.disconnect();
            WiFi.softAP(ap_ssid, ap_password, 1, false, 1);
            is_ap = true;
            break;

        case WL_CONNECTION_LOST:
            wifi_err = "Connection lost!";
            WiFi.disconnect();
            WiFi.softAP(ap_ssid, ap_password, 1, false, 1);
            is_ap = true;
            break;

        case WL_DISCONNECTED:
            connecting.render();
            break;

        case WL_NO_SHIELD:
        case WL_SCAN_COMPLETED:
        {
            FlyingText text(display, "???", -1, 28, 0, 0, myRED, TINY_WIDE_FONT);
            text.render();
            break;
        }
        case WL_STOPPED:
            FlyingText text(display, "WiFi Stopped", -1, 28, 0, 0, myRED, TINY_WIDE_FONT);
            text.render();
            break;
    }
//    display.drawLine(32+16, 0, 64+16, 32, myYELLOW); // Test seams between displays

}

// =========================================================================
// Main Loop (25 FPS)
// =========================================================================
void loop()
{
    uint32_t frame_start = millis();

    if (is_ap) {
        setup_ap.render();
    } else {
        wifi_connected();
    }
    dma_display->flipDMABuffer();
// Non-blocking heartbeat blink on GPIO 48 NeoPixel (every 300ms)
/*
    static uint32_t last_led_toggle = 0;
    static bool led_on = false;
    if (frame_start - last_led_toggle >= 300) {
        last_led_toggle = frame_start;
        led_on = !led_on;
        rgbLedWrite(LED_PIN, led_on ? 30 : 0, 0, 0);
    }
*/

    server.handleClient();

    uint32_t elapsed = millis() - frame_start;
    if (elapsed < 40) { // Maintain ~25 FPS
        delay(40 - elapsed);
    }
}