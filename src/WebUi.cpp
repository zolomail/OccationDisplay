#include "WebUi.h"

#include <WiFi.h>
#include <Preferences.h>
#include <esp_https_server.h>
#include "Weather.h"

namespace WebUi {

namespace {

constexpr char AP_SSID[]        = "ESP8285-Display";
constexpr char AP_PASSWORD[]    = "123456789";

constexpr char DEFAULT_SSID[]   = "bandit";
constexpr char DEFAULT_PASS[]   = "limfjordenerblaa";
constexpr char PREFS_NS[]       = "display";

// Self-signed EC P-256 certificate and private key (valid 2024-2036)
constexpr char SERVER_CERT_PEM[] =
"-----BEGIN CERTIFICATE-----\n"
"MIIBijCCAS+gAwIBAgIUWauMClxQZR/PrMx0v2Z0p8yVe4MwCgYIKoZIzj0EAwIw\n"
"GjEYMBYGA1UEAwwPT2NjYXNpb25EaXNwbGF5MB4XDTI0MDEwMTAwMDAwMFoXDTM2\n"
"MTIzMTIzNTk1OVowGjEYMBYGA1UEAwwPT2NjYXNpb25EaXNwbGF5MFkwEwYHKoZI\n"
"zj0CAQYIKoZIzj0DAQcDQgAEWuuR9ZkUr70dzqrY48M7ZunMPk2A16c7priI+ZPN\n"
"1kwkJgyvFijzoI4sjsRKlWqIbU4x2BFogXE7ZlI+IF77eKNTMFEwHQYDVR0OBBYE\n"
"FA/SkZIXqckLBgjf1EF3QCXW9Kq6MB8GA1UdIwQYMBaAFA/SkZIXqckLBgjf1EF3\n"
"QCXW9Kq6MA8GA1UdEwEB/wQFMAMBAf8wCgYIKoZIzj0EAwIDSQAwRgIhAPw/etHy\n"
"S3OOJ3t/Z/enObB/4USonwZ45CnGvz+uucUNAiEA8AnoGFW2m9/Avft9EgDBJ/xF\n"
"9r4vbEiuaYjRabHwdxI=\n"
"-----END CERTIFICATE-----\n";

constexpr char SERVER_KEY_PEM[] =
"-----BEGIN PRIVATE KEY-----\n"
"MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgNcOfxtNNvnNoLGgg\n"
"7RtJNXXI94b8FdQbjFmDtAMAMt+hRANCAARa65H1mRSvvR3Oqtjjwztm6cw+TYDX\n"
"pzumuIj5k83WTCQmDK8WKPOgjiyOxEqVaohtTjHYEWiBcTtmUj4gXvt4\n"
"-----END PRIVATE KEY-----\n";

httpd_handle_t http_server  = nullptr;
httpd_handle_t https_server = nullptr;
Preferences    prefs;

String wifi_ssid     = DEFAULT_SSID;
String wifi_password = DEFAULT_PASS;
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
    { SceneId::BumpMapEffect,    "BumpMapEffect"    },
    { SceneId::ValentinesDay,    "ValentinesDay"    },
    { SceneId::CubeEffect,       "CubeEffect"       },
    { SceneId::Test1,            "Test 1"           },
    { SceneId::Test2,            "Test 2"           },
    { SceneId::Test3,            "Test 3"           },
    { SceneId::Test4,            "Test 4"           },
    { SceneId::Test5,            "Test 5"           },
    { SceneId::FullscreenTest,   "FullscreenTest"   }
    
};

void connectStation()
{
    is_ap   = false;
    show_ip = true;
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
}

int hexVal(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

String urlDecode(const String& str)
{
    String out;
    out.reserve(str.length());
    for (size_t i = 0; i < str.length(); ++i) {
        const char c = str[i];
        if (c == '+') {
            out += ' ';
        } else if (c == '%' && i + 2 < str.length()) {
            const int hi = hexVal(str[i + 1]);
            const int lo = hexVal(str[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out += static_cast<char>((hi << 4) | lo);
                i += 2;
            } else {
                out += c;
            }
        } else {
            out += c;
        }
    }
    return out;
}

bool getFormArg(const String& body, const char* key, String& outVal)
{
    const String prefix = String(key) + "=";
    int pos = 0;
    while (pos < static_cast<int>(body.length())) {
        int amp = body.indexOf('&', pos);
        if (amp < 0) amp = body.length();
        const String pair = body.substring(pos, amp);
        if (pair.startsWith(prefix)) {
            outVal = urlDecode(pair.substring(prefix.length()));
            return true;
        }
        pos = amp + 1;
    }
    return false;
}

String readRequestBody(httpd_req_t* req)
{
    const size_t len = req->content_len;
    if (len == 0 || len > 2048) {
        return "";
    }
    String buf;
    buf.reserve(len);
    char tmp[128];
    size_t remaining = len;
    while (remaining > 0) {
        const int to_read = (remaining < sizeof(tmp) - 1) ? static_cast<int>(remaining) : static_cast<int>(sizeof(tmp) - 1);
        const int ret = httpd_req_recv(req, tmp, to_read);
        if (ret <= 0) {
            break;
        }
        tmp[ret] = '\0';
        buf += tmp;
        remaining -= static_cast<size_t>(ret);
    }
    return buf;
}

// GET /
esp_err_t handleRootHttp(httpd_req_t* req)
{
    String html = R"raw(<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width,initial-scale=1"><title>Occasion Display</title></head><body>)raw";
    if (wifi_err.length() > 0) {
        html += "<p style='color:red;'><b>Last Wi-Fi error:</b> " + wifi_err + "</p>";
    }
    if (Weather::isValid()) {
        html += "<p><b>Current Weather (" + Weather::city() + "):</b> " +
                String(Weather::conditionName()) + " (" + String(Weather::temperature(), 1) + " &deg;C, " +
                String(Weather::isDay() ? "Day" : "Night") + " &mdash; Moon: " +
                String(Weather::moonPhaseName()) + ")</p>";
    } else {
        html += "<p><b>Current Weather:</b> Fetching...</p>";
    }
    html += R"raw(
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

    html += R"raw(</select>
    <div id="scene-cfg" style="margin-top:14px;padding:10px;border:1px solid #ccc;max-width:420px;"></div>
    <hr>
    <h2>Wi-Fi Settings:</h2>
    <label><input type="checkbox" id="show_ip" )raw" + String(show_ip ? "checked" : "") + R"raw(> Show IP Address</label><br><br>
    <form method="POST" action="/wifi">
    SSID: <input name="ssid" value=")raw" + wifi_ssid + R"raw("><br><br>
    Password: <input name="pass" type="password" placeholder="Leave blank to keep current"><br><br>
    <button type="submit">Save &amp; Connect</button>
    </form>
    <script>
    const FONTS = [
      [0, 'Normal (5x7)'],
      [1, 'Tiny Wide (6x5)'],
      [2, 'Small Wide (6x8)'],
      [3, 'Org 01 (5x5)'],
      [4, 'Tom Thumb (3x5)'],
      [5, 'Picopixel (3x5)']
    ];

    function escHtml(s) {
      return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
    }

    function fontSelect(id, val) {
      let h = '<select data-k="' + id + '">';
      for (const [fId, fName] of FONTS) {
        h += '<option value="' + fId + '"' + (Number(val) === fId ? ' selected' : '') + '>' + fName + '</option>';
      }
      return h + '</select>';
    }

    function row(label, html) {
      return '<div style="margin:6px 0;display:flex;align-items:center;gap:8px;"><label style="min-width:120px;">' + escHtml(label) + '</label>' + html + '</div>';
    }

    function saveSceneConfig(reset) {
      const sid = document.getElementById('scene_id').value;
      const parts = ['scene_id=' + encodeURIComponent(sid)];
      if (reset) {
        parts.push('reset=1');
      } else {
        document.querySelectorAll('#scene-cfg [data-k]').forEach(el => {
          parts.push(encodeURIComponent(el.dataset.k) + '=' + encodeURIComponent(el.value));
        });
      }
      fetch('/scene_config', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: parts.join('&')
      }).then(() => { if (reset) loadSceneConfig(); });
    }

    function loadSceneConfig() {
      const sid = document.getElementById('scene_id').value;
      const box = document.getElementById('scene-cfg');
      fetch('/scene_config?scene_id=' + encodeURIComponent(sid))
        .then(r => r.json())
        .then(d => {
          if (d.is_auto) {
            box.innerHTML = '<i>Auto-selected by date. Pick a specific scene above to customize it.</i>';
            return;
          }
          const params = d.params || [];
          if (!params.length) {
            box.innerHTML = '<i>No editable parameters for this scene.</i>';
            return;
          }
          let h = '<h3 style="margin:0 0 8px 0;">Scene Settings</h3>';
          for (const p of params) {
            if (p.t === 'text') {
              h += row(p.l, '<input data-k="' + escHtml(p.k) + '" style="flex:1" value="' + escHtml(p.v) + '">');
            } else if (p.t === 'color') {
              h += row(p.l, '<input type="color" data-k="' + escHtml(p.k) + '" value="' + escHtml(p.v) + '">');
            } else if (p.t === 'font') {
              h += row(p.l, fontSelect(escHtml(p.k), p.v));
            } else if (p.t === 'number') {
              h += row(p.l, '<input type="number" data-k="' + escHtml(p.k) + '" min="' + p.min + '" max="' + p.max + '" value="' + escHtml(p.v) + '">');
            }
          }
          h += '<div style="margin-top:10px;display:flex;gap:8px;">' +
               '<button type="button" id="btn-apply">Apply</button>' +
               '<button type="button" id="btn-reset">Reset Defaults</button></div>';
          box.innerHTML = h;
          box.querySelectorAll('[data-k]').forEach(el => el.addEventListener('change', () => saveSceneConfig(false)));
          document.getElementById('btn-apply').addEventListener('click', () => saveSceneConfig(false));
          document.getElementById('btn-reset').addEventListener('click', () => saveSceneConfig(true));
        });
    }

    function updateOptions() {
    var params = 'scene_id=' + encodeURIComponent(document.getElementById('scene_id').value) +
                '&show_ip=' + (document.getElementById('show_ip').checked ? '1' : '0');
    return fetch('/options', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: params
    });
    }
    document.getElementById('scene_id').addEventListener('change', function() {
      updateOptions().then(() => loadSceneConfig());
    });
    document.getElementById('show_ip').addEventListener('change', updateOptions);
    loadSceneConfig();
    </script>
    </body></html>)raw";

    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, html.c_str(), html.length());
}

// POST /options
esp_err_t handleOptionsHttp(httpd_req_t* req)
{
    const String body = readRequestBody(req);
    String val;
    if (getFormArg(body, "scene_id", val)) {
        const int id = val.toInt();
        if (id >= static_cast<int>(SceneId::Welcome) && id <= static_cast<int>(SceneId::None)) {
            selected_scene = static_cast<SceneId>(id);
        }
    }
    if (getFormArg(body, "show_ip", val)) {
        show_ip = (val == "1");
    }

    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, "OK", 2);
}

String jsonEscape(const String& s)
{
    String out;
    out.reserve(s.length() + 8);
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

const char* paramTypeName(SceneParamType t)
{
    switch (t) {
        case SceneParamType::Text:   return "text";
        case SceneParamType::Color:  return "color";
        case SceneParamType::Font:   return "font";
        case SceneParamType::Number: return "number";
    }
    return "text";
}

// GET /scene_config?scene_id=<id>
esp_err_t handleSceneConfigGetHttp(httpd_req_t* req)
{
    SceneId sid = selected_scene;
    char query[64] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char val[16] = {0};
        if (httpd_query_key_value(query, "scene_id", val, sizeof(val)) == ESP_OK) {
            const int id = atoi(val);
            if (id >= static_cast<int>(SceneId::Welcome) && id <= static_cast<int>(SceneId::None)) {
                sid = static_cast<SceneId>(id);
            }
        }
    }

    if (sid == SceneId::None) {
        const String json = "{\"is_auto\":true,\"params\":[]}";
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, json.c_str(), json.length());
    }

    std::vector<SceneParam> params;
    bool ready = false;
    for (int attempt = 0; attempt < 25; ++attempt) {
        if (SceneParams::getActive(sid, params)) {
            ready = true;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    String json;
    json.reserve(512);
    json += "{\"is_auto\":false,\"params\":[";
    if (ready) {
        for (size_t i = 0; i < params.size(); ++i) {
            const SceneParam& p = params[i];
            if (i > 0) json += ',';
            json += "{\"k\":\"" + jsonEscape(p.key) + "\"";
            json += ",\"l\":\"" + jsonEscape(p.label) + "\"";
            json += ",\"t\":\"";
            json += paramTypeName(p.type);
            json += "\"";
            json += ",\"v\":\"" + jsonEscape(p.value) + "\"";
            if (p.type == SceneParamType::Number) {
                json += ",\"min\":" + String(p.min_val);
                json += ",\"max\":" + String(p.max_val);
            }
            json += "}";
        }
    }
    json += "]}";

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json.c_str(), json.length());
}

// POST /scene_config
esp_err_t handleSceneConfigPostHttp(httpd_req_t* req)
{
    const String body = readRequestBody(req);
    String val;
    if (!getFormArg(body, "scene_id", val)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing scene_id");
        return ESP_FAIL;
    }

    const int id = val.toInt();
    if (id < static_cast<int>(SceneId::Welcome) || id >= static_cast<int>(SceneId::None)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid scene_id");
        return ESP_FAIL;
    }

    const SceneId sid = static_cast<SceneId>(id);

    if (getFormArg(body, "reset", val) && val == "1") {
        SceneParams::clearOverrides(sid);
    } else {
        int pos = 0;
        while (pos < static_cast<int>(body.length())) {
            int amp = body.indexOf('&', pos);
            if (amp < 0) amp = body.length();
            const String pair = body.substring(pos, amp);
            const int eq = pair.indexOf('=');
            if (eq > 0) {
                const String k = urlDecode(pair.substring(0, eq));
                const String v = urlDecode(pair.substring(eq + 1));
                if (k != "scene_id" && k != "reset") {
                    SceneParams::setOverride(sid, k, v);
                }
            }
            pos = amp + 1;
        }
    }

    scene_dirty = true;
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, "OK", 2);
}

// POST /wifi
esp_err_t handleWifiHttp(httpd_req_t* req)
{
    const String body = readRequestBody(req);
    String ssid_val;
    String pass_val;

    prefs.begin(PREFS_NS, false);
    if (getFormArg(body, "ssid", ssid_val) && ssid_val.length() > 0) {
        wifi_ssid = ssid_val;
        prefs.putString("ssid", wifi_ssid);
    }
    if (getFormArg(body, "pass", pass_val) && pass_val.length() > 0) {
        wifi_password = pass_val;
        prefs.putString("pass", wifi_password);
    }
    prefs.end();

    wifi_err = "";
    const String resp =
        "<!DOCTYPE html><html><body><p>Saved! Connecting to <b>" + wifi_ssid +
        "</b>...</p><p><a href='/'>Back</a></p></body></html>";

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, resp.c_str(), resp.length());

    delay(200);
    connectStation();
    return ESP_OK;
}

void registerRoutes(httpd_handle_t srv)
{
    if (!srv) return;

    const httpd_uri_t root_uri             = { "/",             HTTP_GET,  handleRootHttp,            nullptr };
    const httpd_uri_t options_uri          = { "/options",      HTTP_POST, handleOptionsHttp,         nullptr };
    const httpd_uri_t scene_cfg_get_uri    = { "/scene_config", HTTP_GET,  handleSceneConfigGetHttp,  nullptr };
    const httpd_uri_t scene_cfg_post_uri   = { "/scene_config", HTTP_POST, handleSceneConfigPostHttp, nullptr };
    const httpd_uri_t wifi_uri             = { "/wifi",         HTTP_POST, handleWifiHttp,            nullptr };

    httpd_register_uri_handler(srv, &root_uri);
    httpd_register_uri_handler(srv, &options_uri);
    httpd_register_uri_handler(srv, &scene_cfg_get_uri);
    httpd_register_uri_handler(srv, &scene_cfg_post_uri);
    httpd_register_uri_handler(srv, &wifi_uri);
}

} // namespace

void begin()
{
    // Load persisted settings from NVS flash (survives reflashing)
    prefs.begin(PREFS_NS, true);
    wifi_ssid     = prefs.getString("ssid", DEFAULT_SSID);
    wifi_password = prefs.getString("pass", DEFAULT_PASS);
    prefs.end();

    connectStation();

    // 1. Start plain HTTP server on port 80 (pinned to Core 0)
    httpd_config_t http_cfg = HTTPD_DEFAULT_CONFIG();
    http_cfg.server_port      = 80;
    http_cfg.ctrl_port        = 32768;
    http_cfg.core_id          = 0;
    http_cfg.lru_purge_enable = true;
    if (httpd_start(&http_server, &http_cfg) == ESP_OK) {
        registerRoutes(http_server);
        Serial.println("HTTP server started on port 80");
    }

    // 2. Start HTTPS (TLS) server on port 443 (pinned to Core 0)
    httpd_ssl_config_t https_cfg = HTTPD_SSL_CONFIG_DEFAULT();
    https_cfg.port_secure      = 443;
    https_cfg.httpd.ctrl_port  = 32769;
    https_cfg.httpd.core_id    = 0;
    https_cfg.httpd.max_open_sockets = 2;
    https_cfg.servercert       = reinterpret_cast<const uint8_t*>(SERVER_CERT_PEM);
    https_cfg.servercert_len   = sizeof(SERVER_CERT_PEM);
    https_cfg.prvtkey_pem      = reinterpret_cast<const uint8_t*>(SERVER_KEY_PEM);
    https_cfg.prvtkey_len      = sizeof(SERVER_KEY_PEM);
    if (httpd_ssl_start(&https_server, &https_cfg) == ESP_OK) {
        registerRoutes(https_server);
        Serial.println("HTTPS server started on port 443");
    }
}

void handleClient()
{
    // Both HTTP and HTTPS servers run asynchronously in background FreeRTOS tasks on Core 0
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
const String& wifiError()  { return wifi_err; }
const char* apSsid()       { return AP_SSID; }
const char* apPassword()   { return AP_PASSWORD; }

} // namespace WebUi
