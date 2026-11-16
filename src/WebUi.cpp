#include "WebUi.h"

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "Weather.h"

namespace WebUi {

namespace {

constexpr char AP_SSID[]        = "ESP8285-Display";
constexpr char AP_PASSWORD[]    = "123456789";

constexpr char DEFAULT_SSID[]   = "bandit";
constexpr char DEFAULT_PASS[]   = "limfjordenerblaa";
constexpr char DEFAULT_SCROLL[] = "put your text here now --- \x80\x80\x80 --- ";
constexpr char PREFS_NS[]       = "display";

WebServer   server(80);
Preferences prefs;

String wifi_ssid     = DEFAULT_SSID;
String wifi_password = DEFAULT_PASS;
String scroll_text   = DEFAULT_SCROLL;
String wifi_err      = "";

bool    is_ap          = false;
bool    show_ip        = true;
bool    scene_dirty    = false;
SceneId selected_scene = SceneId::None;

struct SceneOption {
    SceneId     id;
    const char* label;
};

constexpr SceneOption SCENE_OPTIONS[] = {
    { SceneId::None,             "Current Occation" },
    { SceneId::Test,             "Test"             },
    { SceneId::NewYearsMidnight, "NewYearsMidnight" },
    { SceneId::JanuaryDays,      "JanuaryDays"      },
    { SceneId::Aquatic,          "Aquatic"          },
    { SceneId::Halloween,        "Halloween"        },
    { SceneId::JustSnow,         "JustSnow"         },
    { SceneId::ChristmasEve,     "ChristmasEve"     },
    { SceneId::Birthday,         "Birthday"         },
    { SceneId::NewYearsDay,      "NewYearsDay"      },
    { SceneId::ChristmasDays,    "ChristmasDays"    },
    { SceneId::NormalDays,       "NormalDays"       },
    { SceneId::FlamingHot,       "FlamingHot"       },
    { SceneId::WaterEffect,      "WaterEffect"      },
    { SceneId::BumpMapEffect,    "BumpMapEffect"    }
};

void connectStation()
{
    is_ap   = false;
    show_ip = true;
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
}

// GET /
void handleRootHttp()
{
    String html = R"raw(<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width,initial-scale=1"><title>Occasion Display</title></head><body>)raw";
    if (wifi_err.length() > 0) {
        html += "<p style='color:red;'><b>Last Wi-Fi error:</b> " + wifi_err + "</p>";
    }
    if (Weather::isValid()) {
        html += "<p><b>Current Weather (" + Weather::city() + "):</b> " +
                String(Weather::conditionName()) + " (" + String(Weather::temperature(), 1) + " &deg;C)</p>";
    } else {
        html += "<p><b>Current Weather:</b> Fetching...</p>";
    }
    html += R"raw(
    <h2>Enter scroll text:</h2>
    <form id="form">
    <input id="text" placeholder="Scroll text..."> <button type="submit">Update</button>
    </form>
    <hr>
    <h2>Display Options:</h2>
    <label for="scene_id">Scene: </label>
    <select id="scene_id">)raw";

    for (const auto& opt : SCENE_OPTIONS) {
        html += "<option value=\"" + String(static_cast<uint8_t>(opt.id)) + "\"";
        if (opt.id == selected_scene) {
            html += " selected";
        }
        html += ">" + String(opt.label) + "</option>";
    }

    html += R"raw(</select><br><br>
    <label><input type="checkbox" id="show_ip" )raw" + String(show_ip ? "checked" : "") + R"raw(> Show IP Address</label>
    <hr>
    <h2>Wi-Fi Settings:</h2>
    <form method="POST" action="/wifi">
    SSID: <input name="ssid" value=")raw" + wifi_ssid + R"raw("><br><br>
    Password: <input name="pass" type="password" placeholder="Leave blank to keep current"><br><br>
    <button type="submit">Save &amp; Connect</button>
    </form>
    <script>
    document.getElementById('form').addEventListener('submit', function (se) {
    se.preventDefault();
    var params = 'text=' + encodeURIComponent(document.getElementById('text').value);
    fetch('/scroller', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: params
    });
    });

    function updateOptions() {
    var params = 'scene_id=' + encodeURIComponent(document.getElementById('scene_id').value) +
                '&show_ip=' + (document.getElementById('show_ip').checked ? '1' : '0');
    fetch('/options', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: params
    });
    }
    document.getElementById('scene_id').addEventListener('change', updateOptions);
    document.getElementById('show_ip').addEventListener('change', updateOptions);
    </script>
    </body></html>)raw";

    server.send(200, "text/html", html);
}

// POST /scroller
void handleScrollerHttp()
{
    const String text = server.arg("text").length() > 0 ? server.arg("text") : server.arg(0);
    scroll_text = text;
    scene_dirty = true;

    prefs.begin(PREFS_NS, false);
    prefs.putString("scroll", scroll_text);
    prefs.end();

    server.send(200, "text/plain", "OK");
}

// POST /options
void handleOptionsHttp()
{
    if (server.hasArg("scene_id")) {
        const int id = server.arg("scene_id").toInt();
        if (id >= static_cast<int>(SceneId::None) && id <= static_cast<int>(SceneId::BumpMapEffect)) {
            selected_scene = static_cast<SceneId>(id);
        }
    }
    if (server.hasArg("show_ip")) {
        show_ip = (server.arg("show_ip") == "1");
    }
    server.send(200, "text/plain", "OK");
}

// POST /wifi
void handleWifiHttp()
{
    prefs.begin(PREFS_NS, false);
    if (server.hasArg("ssid") && server.arg("ssid").length() > 0) {
        wifi_ssid = server.arg("ssid");
        prefs.putString("ssid", wifi_ssid);
    }
    if (server.hasArg("pass") && server.arg("pass").length() > 0) {
        wifi_password = server.arg("pass");
        prefs.putString("pass", wifi_password);
    }
    prefs.end();

    wifi_err = "";
    server.send(200, "text/html",
        "<!DOCTYPE html><html><body><p>Saved! Connecting to <b>" + wifi_ssid +
        "</b>...</p><p><a href='/'>Back</a></p></body></html>");
    delay(200);
    connectStation();
}

} // namespace

void begin()
{
    // Load persisted settings from NVS flash (survives reflashing)
    prefs.begin(PREFS_NS, true);
    wifi_ssid     = prefs.getString("ssid", DEFAULT_SSID);
    wifi_password = prefs.getString("pass", DEFAULT_PASS);
    scroll_text   = prefs.getString("scroll", DEFAULT_SCROLL);
    prefs.end();

    connectStation();

    server.on("/", HTTP_GET, handleRootHttp);
    server.on("/scroller", HTTP_POST, handleScrollerHttp);
    server.on("/options", HTTP_POST, handleOptionsHttp);
    server.on("/wifi", HTTP_POST, handleWifiHttp);
    server.begin();
}

void handleClient()
{
    server.handleClient();
}

void startAccessPoint(const char* reason)
{
    wifi_err = reason;
    Serial.printf("Starting AP (%s)\n", reason);
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD, 1, false, 1);
    is_ap = true;
}

bool isApMode()            { return is_ap; }
SceneId selectedScene()    { return selected_scene; }
bool showIp()              { return show_ip; }
bool consumeSceneDirty()   { const bool d = scene_dirty; scene_dirty = false; return d; }
const String& scrollText() { return scroll_text; }
const String& wifiError()  { return wifi_err; }
const char* apSsid()       { return AP_SSID; }
const char* apPassword()   { return AP_PASSWORD; }

} // namespace WebUi
