/*
  Live Instagram + TikTok follower counter
  Board: ESP32-2432S028R  (2.8" "Cheap Yellow Display": ILI9341 240x320 + XPT2046 resistive touch)

  Shows your Instagram followers, TikTok followers and the combined total, refreshed on a timer.
  Tap the screen to refresh immediately. The RGB LED blinks green when the total goes up.

  Data sources are chosen per platform in config.h:
    SOURCE_API    - official APIs (Instagram API with Instagram Login / TikTok Display API)
    SOURCE_SCRAPE - read the public profile page directly (no keys needed, less reliable)
    SOURCE_BRIDGE - ask the small helper script in ../bridge running on a PC / Raspberry Pi
    SOURCE_OFF    - hide this platform

  See ../README.md for setup instructions.
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <time.h>

// Source identifiers (used by config.h, so they must come first)
#define SOURCE_OFF    0
#define SOURCE_API    1
#define SOURCE_SCRAPE 2
#define SOURCE_BRIDGE 3

#if __has_include("config.h")
  #include "config.h"
#else
  #error "No config.h found: copy config.example.h to config.h (next to this sketch) and fill in your settings."
#endif

// ---------- optional settings, override them in config.h if you like ----------
#ifndef SCREEN_ROTATION
#define SCREEN_ROTATION 1          // 1 = landscape, 3 = landscape upside down
#endif
#ifndef REFRESH_INTERVAL_MIN
#define REFRESH_INTERVAL_MIN 5
#endif
#ifndef TZ_INFO
#define TZ_INFO "UTC0"
#endif
#ifndef CLOCK_24H
#define CLOCK_24H 0
#endif
#ifndef HTTP_TIMEOUT_MS
#define HTTP_TIMEOUT_MS 15000
#endif
#ifndef INSTAGRAM_SOURCE
#define INSTAGRAM_SOURCE SOURCE_OFF
#endif
#ifndef TIKTOK_SOURCE
#define TIKTOK_SOURCE SOURCE_OFF
#endif
#ifndef INSTAGRAM_USERNAME
#define INSTAGRAM_USERNAME ""
#endif
#ifndef TIKTOK_USERNAME
#define TIKTOK_USERNAME ""
#endif
#ifndef INSTAGRAM_ACCESS_TOKEN
#define INSTAGRAM_ACCESS_TOKEN ""
#endif
#ifndef INSTAGRAM_API_BASE
#define INSTAGRAM_API_BASE "https://graph.instagram.com/v22.0/me"
#endif
#ifndef INSTAGRAM_AUTO_REFRESH_TOKEN
#define INSTAGRAM_AUTO_REFRESH_TOKEN 1
#endif
#ifndef TIKTOK_CLIENT_KEY
#define TIKTOK_CLIENT_KEY ""
#endif
#ifndef TIKTOK_CLIENT_SECRET
#define TIKTOK_CLIENT_SECRET ""
#endif
#ifndef TIKTOK_REFRESH_TOKEN
#define TIKTOK_REFRESH_TOKEN ""
#endif
#ifndef BRIDGE_URL
#define BRIDGE_URL ""
#endif
#ifndef WIFI_HOSTNAME
#define WIFI_HOSTNAME "follower-display"   // name shown in your router / iPhone hotspot client list
#endif
#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif
#ifndef THEME
#define THEME 1                     // 1 = sage green, 0 = classic dark
#endif
// Raw touch range -> pixels. Adjust in config.h if taps land in the wrong place.
#ifndef TOUCH_MIN_X
#define TOUCH_MIN_X 200
#endif
#ifndef TOUCH_MAX_X
#define TOUCH_MAX_X 3700
#endif
#ifndef TOUCH_MIN_Y
#define TOUCH_MIN_Y 240
#endif
#ifndef TOUCH_MAX_Y
#define TOUCH_MAX_Y 3800
#endif
#ifndef ANIMATE_COUNT_UP
#define ANIMATE_COUNT_UP 1          // numbers roll up when followers increase
#endif
#ifndef CELEBRATE_MILESTONES
#define CELEBRATE_MILESTONES 1      // confetti when a count crosses a round number
#endif
#ifndef CELEBRATION_SECONDS
#define CELEBRATION_SECONDS 6
#endif
#ifndef BROWSER_UA
#define BROWSER_UA "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36"
#endif

// ---------- ESP32-2432S028R pins (display pins live in the TFT_eSPI setup) ----------
#define PIN_BACKLIGHT 21
#define PIN_LED_R      4     // RGB LED is active LOW
#define PIN_LED_G     16
#define PIN_LED_B     17
#define XPT_IRQ       36
#define XPT_MOSI      32
#define XPT_MISO      39
#define XPT_CLK       25
#define XPT_CS        33

// ---------- layout (320 x 240 landscape) ----------
#define SCR_W     320
#define SCR_H     240
#define HDR_H      26
#define CARD_Y     30
#define CARD_H    108
#define CARD_W    152
#define CARD_GAP    8
#define CARD_X1     4
#define CARD_X2   (CARD_X1 + CARD_W + CARD_GAP)
#define TOTAL_Y   144
#define TOTAL_H    64
#define FOOT_Y    214
#define FOOT_H     26

// ---------- colours (RGB565) ----------
#if THEME == 1
// Sage green: deep forest background, moss cards, a sage panel for the total.
#define C_BG          0x1924   // #1B2520 deep forest
#define C_CARD        0x29C6   // #2A3A30 moss card
#define C_TOTAL       0x9D71   // #9CAF88 sage panel
#define C_TOTAL_TEXT  0x1924   // dark text on the sage panel
#define C_TOTAL_MUTED 0x3A67
#define C_TOTAL_GOOD  0x2AC5   // deep green
#define C_TOTAL_BAD   0x89C5   // brick
#define C_TEXT        0xF79C   // #F3F1E7 cream
#define C_MUTED       0xA5B3   // #A3B59B sage grey
#define C_ACCENT      0x9D71   // sage
#define C_ACCENT_L    0xCEB7   // #C8D6BC pale sage
#define C_IG          0xDD35   // #D9A5AC dusty rose
#define C_IG2         0xEE14   // #EAC3A3 peach
#define C_TT          0x8E58   // #8FCBC2 soft teal
#define C_TT2         0xE534   // #E3A7A7 blush
#define C_GOOD        0xAEF3   // #A9DD9A light green
#define C_BAD         0xE4D1   // #E09A8A soft red
#define C_WARN        0xE651   // #E6C98A sand
#else
// Classic dark
#define C_BG          0x10A3
#define C_CARD        0x2124
#define C_TOTAL       0x1926
#define C_TOTAL_TEXT  TFT_WHITE
#define C_TOTAL_MUTED 0x8C71
#define C_TOTAL_GOOD  0x2E8E
#define C_TOTAL_BAD   0xE945
#define C_TEXT        TFT_WHITE
#define C_MUTED       0x8C71
#define C_ACCENT      0x7BFF
#define C_ACCENT_L    TFT_YELLOW
#define C_IG          0xE1AD
#define C_IG2         0xFC40
#define C_TT          0x6E5A
#define C_TT2         0xE8EA
#define C_GOOD        0x2E8E
#define C_BAD         0xE945
#define C_WARN        0xFD20
#endif

// ---------- globals ----------
TFT_eSPI tft;
TFT_eSprite spr(&tft);
SPIClass touchSPI(VSPI);
XPT2046_Touchscreen touch(XPT_CS, XPT_IRQ);
Preferences prefs;
WiFiMulti wifiMulti;

// Every network the display may join. It picks the strongest one it can see,
// so you can list your home Wi-Fi and your phone's hotspot together.
struct WifiCred { const char* ssid; const char* pass; };
static const WifiCred WIFI_LIST[] = {
#ifdef WIFI_NETWORKS
  WIFI_NETWORKS
#else
  { WIFI_SSID, WIFI_PASSWORD }
#endif
};
static const size_t WIFI_COUNT = sizeof(WIFI_LIST) / sizeof(WIFI_LIST[0]);

typedef bool (*FetchFn)(long& out, String& err);

struct Platform {
  const char* name;
  const char* user;
  int         source;
  FetchFn     api;
  FetchFn     scrape;
  const char* bridgeKey;   // key in the bridge JSON
  const char* prefKey;     // NVS key for the last known count
  uint16_t    accent;
  uint16_t    accent2;
  long        count;       // -1 = unknown
  long        prev;        // value before the latest successful reading
  long        delta;       // change since the previous reading
  bool        ok;          // last fetch succeeded
  String      err;
};

static bool fetchInstagramApi(long& out, String& err);
static bool fetchTikTokApi(long& out, String& err);
static bool scrapeInstagram(long& out, String& err);
static bool scrapeTikTok(long& out, String& err);
static String wifiNames();

Platform ig = {"Instagram", INSTAGRAM_USERNAME, INSTAGRAM_SOURCE, fetchInstagramApi, scrapeInstagram,
               "instagram", "ig_last", C_IG, C_IG2, -1, -1, 0, false, ""};
Platform tt = {"TikTok",    TIKTOK_USERNAME,    TIKTOK_SOURCE,    fetchTikTokApi,    scrapeTikTok,
               "tiktok",    "tt_last", C_TT, C_TT2, -1, -1, 0, false, ""};

static const unsigned long REFRESH_MS = (unsigned long)REFRESH_INTERVAL_MIN * 60UL * 1000UL;
static unsigned long lastFetchMs   = 0;
static unsigned long nextFetchMs   = 0;
static unsigned long lastTouchMs   = 0;
static unsigned long lastWifiTryMs = 0;
static unsigned long lastBarsMs    = 0;
static bool   forceRefresh = true;
static bool   everFetched  = false;
static long   lastTotal    = -1;
static String footerMsg;
static bool   footerIsError = false;

// What the screen is showing: the dashboard, or one panel blown up full screen.
enum View { VIEW_DASH, VIEW_IG, VIEW_TT, VIEW_TOTAL };
static View view = VIEW_DASH;

// =====================================================================
//  small helpers
// =====================================================================
static void setLed(bool r, bool g, bool b) {
  digitalWrite(PIN_LED_R, r ? LOW : HIGH);
  digitalWrite(PIN_LED_G, g ? LOW : HIGH);
  digitalWrite(PIN_LED_B, b ? LOW : HIGH);
}

static bool timeSynced() { return time(nullptr) > 1700000000; }

static String clockStr() {
  if (!timeSynced()) return "";
  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  char buf[16];
  strftime(buf, sizeof buf, CLOCK_24H ? "%H:%M" : "%I:%M %p", &t);
  String s = buf;
  if (!CLOCK_24H && s.startsWith("0")) s.remove(0, 1);
  return s;
}

// 1234567 -> "1,234,567"
static String fmtNum(long n) {
  if (n < 0) return "--";
  String s = String(n), out;
  int len = s.length();
  for (int i = 0; i < len; i++) {
    if (i && (len - i) % 3 == 0) out += ',';
    out += s[i];
  }
  return out;
}

// "1,234" / "1.5M" / "12K" -> number
static long parseCount(String s) {
  s.trim();
  if (!s.length()) return -1;
  double mult = 1;
  char last = s[s.length() - 1];
  if (last == 'K' || last == 'k') mult = 1e3;
  else if (last == 'M' || last == 'm') mult = 1e6;
  else if (last == 'B' || last == 'b') mult = 1e9;
  if (mult != 1) s.remove(s.length() - 1);
  String digits;
  for (unsigned i = 0; i < s.length(); i++) {
    char c = s[i];
    if (isdigit(c) || (c == '.' && mult != 1)) digits += c;
  }
  if (!digits.length()) return -1;
  return (long)(digits.toDouble() * mult + 0.5);
}

static String urlEncode(const String& s) {
  String out;
  const char* hex = "0123456789ABCDEF";
  for (unsigned i = 0; i < s.length(); i++) {
    char c = s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out += c;
    else { out += '%'; out += hex[(c >> 4) & 0xF]; out += hex[c & 0xF]; }
  }
  return out;
}

static String fingerprint(const String& s) {
  // short identifier for a config value so we can tell when the user changed it
  return s.substring(0, 12) + ":" + String(s.length());
}

// =====================================================================
//  HTTP
// =====================================================================
// Returns the HTTP status code (or a negative HTTPClient error). Body goes to `response`.
static int httpRequest(const String& url, bool post, const String& body, const char* contentType,
                       const char* bearer, String& response) {
  WiFiClientSecure secure;
  WiFiClient plain;
  WiFiClient* client = &plain;
  if (url.startsWith("https://")) {
    secure.setInsecure();   // no certificate pinning: simpler to keep working for years
    client = &secure;
  }
  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setReuse(false);
  http.setUserAgent("ESP32-FollowerDisplay/1.0");
  if (!http.begin(*client, url)) return -100;
  if (bearer && *bearer) http.addHeader("Authorization", String("Bearer ") + bearer);
  if (contentType) http.addHeader("Content-Type", contentType);
  int code = post ? http.POST(body) : http.GET();
  if (code > 0) response = http.getString();
  http.end();
  return code;
}

static String httpErr(int code) {
  if (code < 0) return String("HTTP: ") + HTTPClient::errorToString(code);
  return "HTTP " + String(code);
}

static bool parseJson(const String& body, JsonDocument& doc, String& err) {
  DeserializationError e = deserializeJson(doc, body);
  if (e) { err = String("bad JSON: ") + e.c_str(); return false; }
  return true;
}

// Stream a web page and pull a number out of it without buffering the whole page.
//   numberBefore=false : number follows `key`   e.g.  "followerCount":1234
//   numberBefore=true  : number precedes `key`  e.g.  1,234 Followers,
static bool scrapeNumber(const String& url, const char* key, bool numberBefore, long& out, String& err) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setReuse(false);
  http.useHTTP10(true);                 // avoids chunked encoding on the raw stream
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.setUserAgent(BROWSER_UA);
  if (!http.begin(client, url)) { err = "begin failed"; return false; }
  http.addHeader("Accept", "text/html,application/xhtml+xml");
  http.addHeader("Accept-Language", "en-US,en;q=0.9");
  int code = http.GET();
  if (code != 200) { err = httpErr(code); http.end(); return false; }

  WiFiClient* s = http.getStreamPtr();
  const size_t keyLen = strlen(key);
  const size_t W = 96;                  // sliding window of the most recent bytes
  char win[W + 1];
  memset(win, 0, sizeof win);
  uint8_t buf[512];
  String numStr;
  bool found = false, collecting = false;
  unsigned long lastData = millis();
  size_t total = 0;

  while (!found && (s->connected() || s->available())) {
    if (!s->available()) {
      if (millis() - lastData > 10000) break;
      delay(5);
      continue;
    }
    int n = s->read(buf, sizeof buf);
    if (n <= 0) continue;
    lastData = millis();
    total += n;
    for (int i = 0; i < n && !found; i++) {
      char c = (char)buf[i];
      if (!collecting) {
        memmove(win, win + 1, W - 1);
        win[W - 1] = c;
        if (memcmp(win + W - keyLen, key, keyLen) != 0) continue;
        if (numberBefore) {
          int e = W - keyLen, b = e;
          while (b > 0 && (isdigit(win[b - 1]) || strchr(".,KkMmBb", win[b - 1]))) b--;
          numStr = String(win + b).substring(0, e - b);
          found = true;
        } else {
          collecting = true;
        }
      } else {
        if (isdigit(c) || strchr(".,KkMmBb", c)) {
          numStr += c;
          if (numStr.length() > 24) found = true;
        } else if (c == '"' || c == ' ') {
          if (numStr.length()) found = true;      // closing quote / space after the value
        } else {
          if (numStr.length()) found = true;
          else collecting = false;                // key was followed by something else
        }
      }
    }
  }
  http.end();
  Serial.printf("[scrape] %s: read %u bytes, %s\n", url.c_str(), (unsigned)total, found ? numStr.c_str() : "not found");
  if (!found) { err = "value not found in page"; return false; }
  out = parseCount(numStr);
  if (out < 0) { err = "could not parse '" + numStr + "'"; return false; }
  return true;
}

// =====================================================================
//  Instagram
// =====================================================================
static String instagramToken() {
  String cfg = INSTAGRAM_ACCESS_TOKEN;
  String fp = fingerprint(cfg);
  if (prefs.getString("ig_cfg", "") != fp) {      // token in config.h changed: forget the stored one
    prefs.putString("ig_cfg", fp);
    prefs.remove("ig_tok");
    prefs.putUInt("ig_ref_at", 0);
  }
  return prefs.getString("ig_tok", cfg);
}

// Long-lived Instagram tokens last 60 days; refresh ours every 30 days.
static void maybeRefreshInstagramToken(String& token) {
#if INSTAGRAM_AUTO_REFRESH_TOKEN
  if (!timeSynced()) return;
  uint32_t now = (uint32_t)time(nullptr);
  uint32_t at = prefs.getUInt("ig_ref_at", 0);
  if (at == 0) { prefs.putUInt("ig_ref_at", now); return; }   // assume the token is fresh
  if (now - at < 30UL * 86400UL) return;
  String body, err;
  int code = httpRequest("https://graph.instagram.com/refresh_access_token?grant_type=ig_refresh_token&access_token=" + token,
                         false, "", nullptr, nullptr, body);
  JsonDocument doc;
  if (code == 200 && parseJson(body, doc, err) && doc["access_token"].is<const char*>()) {
    token = doc["access_token"].as<String>();
    prefs.putString("ig_tok", token);
    prefs.putUInt("ig_ref_at", now);
    Serial.println("[instagram] token refreshed");
  } else {
    Serial.printf("[instagram] token refresh failed (%d): %s\n", code, body.c_str());
    prefs.putUInt("ig_ref_at", now - 29UL * 86400UL);         // try again tomorrow
  }
#endif
}

static bool fetchInstagramApi(long& out, String& err) {
  String token = instagramToken();
  if (!token.length()) { err = "no Instagram token"; return false; }
  maybeRefreshInstagramToken(token);
  String body;
  int code = httpRequest(String(INSTAGRAM_API_BASE) + "?fields=followers_count&access_token=" + urlEncode(token),
                         false, "", nullptr, nullptr, body);
  JsonDocument doc;
  if (code > 0 && parseJson(body, doc, err)) {
    if (doc["error"]["message"].is<const char*>()) { err = doc["error"]["message"].as<String>(); return false; }
    if (doc["followers_count"].is<long>()) { out = doc["followers_count"].as<long>(); return true; }
  }
  if (code != 200) err = httpErr(code);
  else if (!err.length()) err = "no followers_count in reply";
  return false;
}

static bool scrapeInstagram(long& out, String& err) {
  // Logged-out profile pages carry  <meta ... content="1,234 Followers, 56 Following, ...">
  bool ok = scrapeNumber(String("https://www.instagram.com/") + INSTAGRAM_USERNAME + "/", " Followers, ", true, out, err);
  if (!ok && err == "value not found in page") err = "Instagram showed a login wall";
  return ok;
}

// =====================================================================
//  TikTok
// =====================================================================
static bool tiktokAccessToken(String& access, String& err) {
  uint32_t now = timeSynced() ? (uint32_t)time(nullptr) : 0;
  String cfg = TIKTOK_REFRESH_TOKEN;
  String fp = fingerprint(cfg);
  if (prefs.getString("tt_cfg", "") != fp) {      // refresh token in config.h changed
    prefs.putString("tt_cfg", fp);
    prefs.remove("tt_rtok");
    prefs.remove("tt_atok");
    prefs.putUInt("tt_aexp", 0);
  }
  String cached = prefs.getString("tt_atok", "");
  uint32_t exp = prefs.getUInt("tt_aexp", 0);
  if (cached.length() && now && now + 300 < exp) { access = cached; return true; }

  String refresh = prefs.getString("tt_rtok", cfg);
  if (!refresh.length()) { err = "no TikTok refresh token"; return false; }
  String form = "client_key=" + urlEncode(TIKTOK_CLIENT_KEY) +
                "&client_secret=" + urlEncode(TIKTOK_CLIENT_SECRET) +
                "&grant_type=refresh_token&refresh_token=" + urlEncode(refresh);
  String body;
  int code = httpRequest("https://open.tiktokapis.com/v2/oauth/token/", true, form,
                         "application/x-www-form-urlencoded", nullptr, body);
  JsonDocument doc;
  if (code <= 0 || !parseJson(body, doc, err)) { if (code <= 0) err = httpErr(code); return false; }
  if (!doc["access_token"].is<const char*>()) {
    const char* d = doc["error_description"] | (const char*)nullptr;
    if (!d) d = doc["error"] | "token refresh failed";
    err = d;
    return false;
  }
  access = doc["access_token"].as<String>();
  prefs.putString("tt_atok", access);
  prefs.putUInt("tt_aexp", now + (uint32_t)(doc["expires_in"] | 86400));
  if (doc["refresh_token"].is<const char*>()) prefs.putString("tt_rtok", doc["refresh_token"].as<String>());
  Serial.println("[tiktok] access token refreshed");
  return true;
}

static bool fetchTikTokApi(long& out, String& err) {
  String token;
  if (!tiktokAccessToken(token, err)) return false;
  String body;
  int code = httpRequest("https://open.tiktokapis.com/v2/user/info/?fields=follower_count",
                         false, "", nullptr, token.c_str(), body);
  JsonDocument doc;
  if (code <= 0 || !parseJson(body, doc, err)) { if (code <= 0) err = httpErr(code); return false; }
  const char* ecode = doc["error"]["code"] | "ok";
  if (strcmp(ecode, "ok") != 0) {
    err = doc["error"]["message"] | ecode;
    if (strstr(ecode, "token")) prefs.remove("tt_atok");   // force a refresh next time
    return false;
  }
  if (doc["data"]["user"]["follower_count"].is<long>()) { out = doc["data"]["user"]["follower_count"].as<long>(); return true; }
  err = "no follower_count (need user.info.stats scope)";
  return false;
}

static bool scrapeTikTok(long& out, String& err) {
  // The profile page embeds JSON containing  "followerCount":1234
  bool ok = scrapeNumber(String("https://www.tiktok.com/@") + TIKTOK_USERNAME + "?lang=en", "\"followerCount\":", false, out, err);
  if (!ok && err == "value not found in page") err = "TikTok served a bot-check page";
  return ok;
}

// =====================================================================
//  Bridge (helper script on your PC / Pi)
// =====================================================================
static bool fetchBridge(JsonDocument& doc, String& err) {
  if (!strlen(BRIDGE_URL)) { err = "BRIDGE_URL not set"; return false; }
  String body;
  int code = httpRequest(BRIDGE_URL, false, "", nullptr, nullptr, body);
  if (code != 200) { err = "bridge " + httpErr(code); return false; }
  return parseJson(body, doc, err);
}

// =====================================================================
//  Drawing
// =====================================================================
// Pick the largest bold font that fits `text` into `maxW` pixels.
static const GFXfont* fitFont(TFT_eSPI& g, const String& text, int maxW) {
  static const GFXfont* fonts[] = {&FreeSansBold24pt7b, &FreeSansBold18pt7b, &FreeSansBold12pt7b, &FreeSansBold9pt7b};
  for (const GFXfont* f : fonts) {
    g.setFreeFont(f);
    if (g.textWidth(text) <= maxW) return f;
  }
  return fonts[3];
}

// Draw through a sprite when there is memory for it, straight to the panel otherwise.
struct Canvas {
  TFT_eSPI* g;
  int ox, oy;
  bool sprite;
  Canvas(int x, int y, int w, int h, uint8_t depth = 16) {
    spr.setColorDepth(depth);                 // 8-bit halves the memory for big areas
    sprite = spr.createSprite(w, h) != nullptr;
    g = sprite ? static_cast<TFT_eSPI*>(&spr) : &tft;
    ox = sprite ? 0 : x; oy = sprite ? 0 : y;
    if (sprite) { px = x; py = y; }
  }
  void done() {
    if (sprite) { spr.pushSprite(px, py); spr.deleteSprite(); }
    spr.setColorDepth(16);
  }
  int px = 0, py = 0;
};

static void drawHeader() {
  Canvas c(0, 0, SCR_W, HDR_H);
  TFT_eSPI& g = *c.g;
  g.fillRect(c.ox, c.oy, SCR_W, HDR_H, C_BG);
  const char* title = view == VIEW_IG ? "INSTAGRAM" : view == VIEW_TT ? "TIKTOK" :
                      view == VIEW_TOTAL ? "TOTAL FOLLOWERS" : "LIVE FOLLOWERS";
  uint16_t tcol = view == VIEW_IG ? C_IG : view == VIEW_TT ? C_TT : C_ACCENT_L;
  g.setTextColor(tcol, C_BG);
  g.setTextDatum(ML_DATUM);
  g.setFreeFont(&FreeSansBold9pt7b);
  g.drawString(title, c.ox + 6, c.oy + HDR_H / 2);
  g.setTextFont(2);
  g.setTextDatum(MR_DATUM);
  g.setTextColor(C_MUTED, C_BG);
  String clk = clockStr();
  int right = c.ox + SCR_W - 20;
  if (clk.length()) g.drawString(clk, right, c.oy + HDR_H / 2);
  bool wifi = WiFi.status() == WL_CONNECTED;
  g.fillCircle(c.ox + SCR_W - 10, c.oy + HDR_H / 2, 4, wifi ? C_GOOD : C_BAD);
  c.done();
}

static void drawFooter() {
  Canvas c(0, FOOT_Y, SCR_W, FOOT_H);
  TFT_eSPI& g = *c.g;
  g.fillRect(c.ox, c.oy, SCR_W, FOOT_H, C_BG);
  g.setTextFont(2);
  g.setTextDatum(MC_DATUM);
  String msg = footerMsg;
  uint16_t col = footerIsError ? C_WARN : C_MUTED;
  if (!msg.length()) {
    if (!everFetched) msg = "Waiting for first update...";
    else {
      unsigned long ago = (millis() - lastFetchMs) / 1000;
      long left = (long)(nextFetchMs - millis()) / 1000; if (left < 0) left = 0;
      msg = "Updated " + String(ago < 60 ? String(ago) + "s" : String(ago / 60) + "m") + " ago";
      msg += "  |  next " + String(left / 60) + ":" + (left % 60 < 10 ? "0" : "") + String(left % 60);
      if (view != VIEW_DASH) msg += "  |  tap to go back";
      else msg += ((millis() / 6000) % 2) ? "  |  tap a panel to zoom" : "  |  tap here to refresh";
    }
  }
  while (g.textWidth(msg) > SCR_W - 8 && msg.length() > 4) msg = msg.substring(0, msg.length() - 4) + "...";
  g.setTextColor(col, C_BG);
  g.drawString(msg, c.ox + SCR_W / 2, c.oy + FOOT_H / 2);
  c.done();
}

static void drawCardInto(TFT_eSPI& g, int ox, int oy, const Platform& p, long showCount, bool hideDelta) {
  g.fillRect(ox, oy, CARD_W, CARD_H, C_BG);
  g.fillRoundRect(ox, oy, CARD_W, CARD_H, 10, C_CARD);
  g.fillRoundRect(ox, oy, CARD_W, 8, 4, p.accent);
  g.fillRect(ox, oy + 4, CARD_W, 4, C_CARD);
  g.fillRect(ox + CARD_W / 2, oy, CARD_W / 2, 4, p.accent2);

  if (p.source == SOURCE_OFF) {
    g.setTextFont(2); g.setTextDatum(MC_DATUM); g.setTextColor(C_MUTED, C_CARD);
    g.drawString(p.name, ox + CARD_W / 2, oy + CARD_H / 2 - 8);
    g.drawString("disabled", ox + CARD_W / 2, oy + CARD_H / 2 + 10);
    return;
  }

  g.setTextDatum(TL_DATUM);
  g.setFreeFont(&FreeSansBold9pt7b);
  g.setTextColor(p.accent, C_CARD);
  g.drawString(p.name, ox + 10, oy + 14);
  g.setTextFont(2);
  g.setTextColor(C_MUTED, C_CARD);
  String handle = String("@") + p.user;
  while (g.textWidth(handle) > CARD_W - 20 && handle.length() > 3) handle.remove(handle.length() - 1);
  g.drawString(handle, ox + 10, oy + 32);

  long shown = (showCount == -2) ? p.count : showCount;
  String num = fmtNum(shown);
  fitFont(g, num, CARD_W - 16);
  g.setTextDatum(MC_DATUM);
  g.setTextColor(p.ok || shown >= 0 ? C_TEXT : C_MUTED, C_CARD);
  g.drawString(num, ox + CARD_W / 2, oy + 66);

  g.setTextFont(2);
  g.setTextDatum(BC_DATUM);
  if (!p.ok && p.err.length()) {
    g.setTextColor(C_WARN, C_CARD);
    g.drawString(p.count >= 0 ? "update failed (old value)" : "update failed", ox + CARD_W / 2, oy + CARD_H - 6);
  } else if (hideDelta) {
    // animating: the change pops in once the number has finished rolling
  } else if (p.delta > 0) {
    g.setTextColor(C_GOOD, C_CARD);
    g.drawString("+" + fmtNum(p.delta), ox + CARD_W / 2, oy + CARD_H - 6);
  } else if (p.delta < 0) {
    g.setTextColor(C_BAD, C_CARD);
    g.drawString("-" + fmtNum(-p.delta), ox + CARD_W / 2, oy + CARD_H - 6);
  }
}

static void drawCard(int x, const Platform& p, long showCount = -2, bool hideDelta = false) {
  Canvas c(x, CARD_Y, CARD_W, CARD_H);
  drawCardInto(*c.g, c.ox, c.oy, p, showCount, hideDelta);
  c.done();
}

static long currentTotal() {
  long t = 0; bool any = false;
  if (ig.source != SOURCE_OFF && ig.count >= 0) { t += ig.count; any = true; }
  if (tt.source != SOURCE_OFF && tt.count >= 0) { t += tt.count; any = true; }
  return any ? t : -1;
}

static void drawTotal(long showTotal = -2, bool hideDelta = false) {
  Canvas c(0, TOTAL_Y, SCR_W, TOTAL_H);
  TFT_eSPI& g = *c.g;
  int ox = c.ox, oy = c.oy;
  g.fillRect(ox, oy, SCR_W, TOTAL_H, C_BG);
  g.fillRoundRect(ox + CARD_X1, oy, SCR_W - 2 * CARD_X1, TOTAL_H, 10, C_TOTAL);
  g.setTextFont(2);
  g.setTextDatum(TL_DATUM);
  g.setTextColor(C_TOTAL_MUTED, C_TOTAL);
  g.drawString("TOTAL", ox + CARD_X1 + 10, oy + 6);
  long total = (showTotal == -2) ? currentTotal() : showTotal;
  long delta = (ig.source != SOURCE_OFF ? ig.delta : 0) + (tt.source != SOURCE_OFF ? tt.delta : 0);
  if (delta != 0 && !hideDelta) {
    g.setTextDatum(TR_DATUM);
    g.setTextColor(delta > 0 ? C_TOTAL_GOOD : C_TOTAL_BAD, C_TOTAL);
    g.drawString((delta > 0 ? "+" : "-") + fmtNum(labs(delta)), ox + SCR_W - CARD_X1 - 10, oy + 6);
  }
  String num = fmtNum(total);
  fitFont(g, num, SCR_W - 2 * CARD_X1 - 20);
  g.setTextDatum(MC_DATUM);
  g.setTextColor(C_TOTAL_TEXT, C_TOTAL);
  g.drawString(num, ox + SCR_W / 2, oy + TOTAL_H / 2 + 6);
  c.done();
}

// Biggest number we can draw: the 75px built-in font for plain digits, then smaller ones.
static void drawBigNumber(TFT_eSPI& g, long value, int cx, int cy, int maxW) {
  g.setTextDatum(MC_DATUM);
  if (value >= 0) {
    String plain = String(value);
#ifdef LOAD_FONT8
    g.setTextFont(8);
    if (g.textWidth(plain) <= maxW) { g.drawString(plain, cx, cy); return; }
#endif
#ifdef LOAD_FONT6
    g.setTextFont(6);
    if (g.textWidth(plain) <= maxW) { g.drawString(plain, cx, cy); return; }
#endif
  }
  String num = fmtNum(value);
  fitFont(g, num, maxW);
  g.drawString(num, cx, cy);
}

// One panel blown up to fill the screen (between the header and the footer).
static void drawFullscreen(long showValue = -2, bool hideDelta = false) {
  const Platform* p = view == VIEW_IG ? &ig : view == VIEW_TT ? &tt : nullptr;
  const int W = SCR_W, H = FOOT_Y - HDR_H;
  Canvas c(0, HDR_H, W, H, 8);
  TFT_eSPI& g = *c.g;
  int ox = c.ox, oy = c.oy;
  uint16_t accent = p ? p->accent : C_ACCENT, accent2 = p ? p->accent2 : C_ACCENT_L;
  g.fillRect(ox, oy, W, H, C_BG);
  g.fillRoundRect(ox + CARD_X1, oy + 2, W - 2 * CARD_X1, H - 4, 12, C_CARD);
  g.fillRoundRect(ox + CARD_X1, oy + 2, W - 2 * CARD_X1, 8, 4, accent);
  g.fillRect(ox + CARD_X1, oy + 6, W - 2 * CARD_X1, 4, C_CARD);
  g.fillRect(ox + W / 2, oy + 2, W / 2 - CARD_X1, 4, accent2);

  g.setTextFont(2);
  g.setTextDatum(TC_DATUM);
  g.setTextColor(C_MUTED, C_CARD);
  String sub = p ? String("@") + p->user
                 : String("Instagram ") + fmtNum(ig.count) + "   +   TikTok " + fmtNum(tt.count);
  g.drawString(sub, ox + W / 2, oy + 18);

  long value = (showValue != -2) ? showValue : (p ? p->count : currentTotal());
  g.setTextColor(C_TEXT, C_CARD);
  drawBigNumber(g, value, ox + W / 2, oy + H / 2 - 2, W - 44);

  g.setFreeFont(&FreeSansBold9pt7b);
  g.setTextDatum(TC_DATUM);
  g.setTextColor(accent, C_CARD);
  g.drawString("FOLLOWERS", ox + W / 2, oy + H / 2 + 38);

  g.setTextFont(2);
  g.setTextDatum(BC_DATUM);
  long delta = p ? p->delta : (ig.source != SOURCE_OFF ? ig.delta : 0) + (tt.source != SOURCE_OFF ? tt.delta : 0);
  bool failed = p && !p->ok && p->err.length();
  if (failed) {
    g.setTextColor(C_WARN, C_CARD);
    g.drawString("update failed, showing the last value", ox + W / 2, oy + H - 10);
  } else if (!hideDelta && delta != 0) {
    g.setTextColor(delta > 0 ? C_GOOD : C_BAD, C_CARD);
    g.drawString((delta > 0 ? "+" : "-") + fmtNum(labs(delta)) + " since last check", ox + W / 2, oy + H - 10);
  } else if (!hideDelta && everFetched) {
    g.setTextColor(C_MUTED, C_CARD);
    g.drawString("no change since last check", ox + W / 2, oy + H - 10);
  }
  c.done();
}

static void drawAll() {
  drawHeader();
  if (view == VIEW_DASH) {
    drawCard(CARD_X1, ig);
    drawCard(CARD_X2, tt);
    drawTotal();
  } else {
    drawFullscreen();
  }
  drawFooter();
}

static void setView(View v) {
  view = v;
  tft.fillScreen(C_BG);
  drawAll();
}

static void setFooter(const String& msg, bool isError) {
  footerMsg = msg;
  footerIsError = isError;
  drawFooter();
}

static void splash(const String& line1, const String& line2) {
  tft.fillScreen(C_BG);
  tft.setTextDatum(MC_DATUM);
  tft.setFreeFont(&FreeSansBold12pt7b);
  tft.setTextColor(C_TEXT, C_BG);
  tft.drawString(line1, SCR_W / 2, SCR_H / 2 - 16);
  tft.setTextFont(2);
  tft.setTextColor(C_MUTED, C_BG);
  tft.drawString(line2, SCR_W / 2, SCR_H / 2 + 14);
}

// =====================================================================
//  Animations
// =====================================================================
static float easeOut(float t) { return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t); }

// Roll the numbers up from their previous values to the new ones.
static void animateCountUp() {
#if ANIMATE_COUNT_UP
  bool igUp = ig.source != SOURCE_OFF && ig.ok && ig.prev >= 0 && ig.count > ig.prev;
  bool ttUp = tt.source != SOURCE_OFF && tt.ok && tt.prev >= 0 && tt.count > tt.prev;
  if (!igUp && !ttUp) return;
  long totalTo   = currentTotal();
  long totalFrom = totalTo - (igUp ? ig.count - ig.prev : 0) - (ttUp ? tt.count - tt.prev : 0);
  const int frames = 36;
  for (int f = 1; f <= frames; f++) {
    float t = easeOut((float)f / frames);
    long igV = ig.prev + (long)((ig.count - ig.prev) * t + 0.5f);
    long ttV = tt.prev + (long)((tt.count - tt.prev) * t + 0.5f);
    long toV = totalFrom + (long)((totalTo - totalFrom) * t + 0.5f);
    switch (view) {
      case VIEW_DASH:
        if (igUp) drawCard(CARD_X1, ig, igV, f < frames);
        if (ttUp) drawCard(CARD_X2, tt, ttV, f < frames);
        drawTotal(toV, f < frames);
        break;
      case VIEW_IG:    if (igUp) drawFullscreen(igV, f < frames); break;
      case VIEW_TT:    if (ttUp) drawFullscreen(ttV, f < frames); break;
      case VIEW_TOTAL: drawFullscreen(toV, f < frames); break;
    }
    delay(25);
  }
#endif
}

static const uint16_t CONFETTI[] = {C_IG, C_IG2, C_TT, C_TT2, C_GOOD, C_ACCENT_L, TFT_WHITE, C_ACCENT, C_WARN};

// A one-second burst of confetti out of a card plus a big "+N". Plays for every gain, even +1.
static void miniCelebrate(int x, const Platform& p) {
#if ANIMATE_COUNT_UP
  if (view != VIEW_DASH) {            // zoomed in: just pulse the LED
    for (int i = 0; i < 4; i++) { setLed(false, true, false); delay(100); setLed(false, false, false); delay(100); }
    return;
  }
  struct Spark { float x, y, vx, vy; uint8_t size; uint16_t color; };
  const int N = 28;
  Spark sp[N];
  for (int i = 0; i < N; i++) {
    float a = random(628) / 100.0f, v = 2.0f + random(30) / 10.0f;
    sp[i] = { CARD_W / 2.0f, 66.0f, cosf(a) * v, sinf(a) * v - 2.0f, (uint8_t)(2 + random(3)), CONFETTI[random(9)] };
  }
  String plus = "+" + fmtNum(p.delta);
  const int frames = 32;
  for (int f = 0; f < frames; f++) {
    setLed(false, (f / 4) % 2 == 0, false);
    Canvas c(x, CARD_Y, CARD_W, CARD_H);
    TFT_eSPI& g = *c.g;
    drawCardInto(g, c.ox, c.oy, p, -2, true);
    for (int i = 0; i < N; i++) {
      Spark& k = sp[i];
      k.x += k.vx; k.y += k.vy; k.vy += 0.25f;              // a little gravity
      if (k.x >= 0 && k.x < CARD_W && k.y >= 0 && k.y < CARD_H)
        g.fillRect(c.ox + (int)k.x, c.oy + (int)k.y, k.size, k.size, k.color);
    }
    // the "+N" grows in, then holds
    g.setTextDatum(MC_DATUM);
    g.setFreeFont(f < 4 ? &FreeSansBold9pt7b : f < 8 ? &FreeSansBold12pt7b : &FreeSansBold18pt7b);
    g.setTextColor(C_GOOD);
    g.drawString(plus, c.ox + CARD_W / 2, c.oy + 66);
    c.done();
    delay(30);
  }
  setLed(false, false, false);
#endif
}

// Round numbers worth celebrating: every 100 below 1k, 500 below 10k, 1k below 100k,
// 10k below 1M, then every 100k.
static long milestoneStep(long v) {
  if (v < 1000)    return 100;
  if (v < 10000)   return 500;
  if (v < 100000)  return 1000;
  if (v < 1000000) return 10000;
  return 100000;
}

// The round number crossed going from `from` to `to`, or 0 if none.
static long milestoneCrossed(long from, long to) {
  if (from < 0 || to <= from) return 0;
  long step = milestoneStep(to);
  long m = (to / step) * step;
  return (m > from) ? m : 0;
}

static void drawCelebrationText(TFT_eSPI& g, const String& who, const String& num) {
  g.setTextDatum(MC_DATUM);
  g.setFreeFont(&FreeSansBold9pt7b);
  g.setTextColor(C_ACCENT_L);                     // no background colour: text floats over the confetti
  g.drawString("NEW MILESTONE", SCR_W / 2, 72);
  fitFont(g, num, SCR_W - 20);
  g.setTextColor(TFT_WHITE);
  g.drawString(num, SCR_W / 2, 118);
  g.setFreeFont(&FreeSansBold12pt7b);
  g.setTextColor(C_ACCENT);
  g.drawString(who + " followers", SCR_W / 2, 162);
}

// Full-screen confetti for a few seconds. Tap to skip.
static void celebrate(const char* who, long value) {
  Serial.printf("[milestone] %s %ld\n", who, value);
  struct Particle { int16_t x, y; int8_t vx; uint8_t vy, size; uint16_t color; };
  const int N = 70;
  Particle parts[N];
  for (int i = 0; i < N; i++) {
    parts[i] = { (int16_t)random(SCR_W), (int16_t)(-random(SCR_H)), (int8_t)random(-1, 2),
                 (uint8_t)(2 + random(4)), (uint8_t)(3 + random(4)), CONFETTI[random(9)] };
  }
  String num = fmtNum(value);
  String who_s = who;

  // A full-screen 8-bit sprite gives flicker-free frames; fall back to drawing straight to the panel.
  spr.setColorDepth(8);
  bool useSpr = spr.createSprite(SCR_W, SCR_H) != nullptr;
  if (!useSpr) { tft.fillScreen(C_BG); drawCelebrationText(tft, who_s, num); }

  unsigned long endAt = millis() + (unsigned long)CELEBRATION_SECONDS * 1000UL;
  unsigned long skipGuard = millis() + 400;        // ignore the tap that may have triggered the refresh
  uint8_t tick = 0;
  while (millis() < endAt) {
    uint8_t phase = (tick++ >> 3) % 3;
    setLed(phase == 0, phase == 1, phase == 2);
    if (useSpr) {
      spr.fillSprite(C_BG);
      for (int i = 0; i < N; i++) spr.fillRect(parts[i].x, parts[i].y, parts[i].size, parts[i].size, parts[i].color);
      drawCelebrationText(spr, who_s, num);
      spr.pushSprite(0, 0);
    } else {
      for (int i = 0; i < N; i++)
        if (parts[i].y + parts[i].size < 60 || parts[i].y > 180)   // keep the text band clean
          tft.fillRect(parts[i].x, parts[i].y, parts[i].size, parts[i].size, C_BG);
    }
    for (int i = 0; i < N; i++) {
      Particle& p = parts[i];
      p.y += p.vy; p.x += p.vx;
      if (p.y > SCR_H) { p.y = -p.size; p.x = random(SCR_W); }
      if (p.x < 0) p.x = SCR_W - 1;
      if (p.x >= SCR_W) p.x = 0;
    }
    if (!useSpr) {
      for (int i = 0; i < N; i++)
        if (parts[i].y + parts[i].size < 60 || parts[i].y > 180)
          tft.fillRect(parts[i].x, parts[i].y, parts[i].size, parts[i].size, parts[i].color);
    }
    if (millis() > skipGuard && touch.tirqTouched() && touch.touched()) break;
    delay(30);
  }
  if (useSpr) spr.deleteSprite();
  spr.setColorDepth(16);
  setLed(false, false, false);
  tft.fillScreen(C_BG);
  drawAll();
}

static void checkMilestone(const char* key, const char* who, long from, long to, bool valid) {
#if CELEBRATE_MILESTONES
  if (!valid) return;
  long m = milestoneCrossed(from, to);
  if (!m) return;
  if (prefs.getLong(key, 0) >= m) return;     // already celebrated this one
  prefs.putLong(key, m);
  celebrate(who, m);
#endif
}

// =====================================================================
//  Fetch orchestration
// =====================================================================
static void fetchPlatform(Platform& p, JsonDocument& bridge, bool bridgeOk, const String& bridgeErr) {
  if (p.source == SOURCE_OFF) return;
  long v = -1; String err; bool ok = false;
  switch (p.source) {
    case SOURCE_API:    ok = p.api(v, err);    break;
    case SOURCE_SCRAPE: ok = p.scrape(v, err); break;
    case SOURCE_BRIDGE:
      if (!bridgeOk) err = bridgeErr;
      else if (bridge[p.bridgeKey].is<long>()) { v = bridge[p.bridgeKey].as<long>(); ok = true; }
      else err = bridge[String(p.bridgeKey) + "_error"] | "bridge gave no value";
      break;
  }
  if (ok) {
    if (p.count >= 0 && v != p.count) p.delta = v - p.count;
    p.prev = p.count;
    p.count = v;
    p.ok = true;
    p.err = "";
    prefs.putLong(p.prefKey, v);
    Serial.printf("[%s] %ld followers\n", p.name, v);
  } else {
    p.ok = false;
    p.err = err;
    Serial.printf("[%s] failed: %s\n", p.name, err.c_str());
  }
}

static void fetchAll() {
  if (WiFi.status() != WL_CONNECTED) {
    setFooter("No Wi-Fi, looking for: " + wifiNames(), true);
    return;
  }
  setLed(false, false, true);
  setFooter("Updating...", false);

  JsonDocument bridge;
  bool bridgeOk = false;
  String bridgeErr;
  if (ig.source == SOURCE_BRIDGE || tt.source == SOURCE_BRIDGE) bridgeOk = fetchBridge(bridge, bridgeErr);

  fetchPlatform(ig, bridge, bridgeOk, bridgeErr);
  fetchPlatform(tt, bridge, bridgeOk, bridgeErr);

  everFetched = true;
  lastFetchMs = millis();
  nextFetchMs = lastFetchMs + REFRESH_MS;

  String err;
  if (ig.source != SOURCE_OFF && !ig.ok) err = "IG: " + ig.err;
  if (tt.source != SOURCE_OFF && !tt.ok) err += (err.length() ? "  " : "") + String("TT: ") + tt.err;
  footerMsg = err;
  footerIsError = err.length() > 0;

  long total = currentTotal();
  setLed(false, false, false);
  animateCountUp();
  if (ig.source != SOURCE_OFF && ig.ok && ig.prev >= 0 && ig.count > ig.prev) miniCelebrate(CARD_X1, ig);
  if (tt.source != SOURCE_OFF && tt.ok && tt.prev >= 0 && tt.count > tt.prev) miniCelebrate(CARD_X2, tt);
  drawAll();
  if (lastTotal >= 0 && total > lastTotal) {
    for (int i = 0; i < 3; i++) { setLed(false, true, false); delay(120); setLed(false, false, false); delay(120); }
  }
  checkMilestone("ms_ig",  "Instagram", ig.prev, ig.count, ig.source != SOURCE_OFF && ig.ok);
  checkMilestone("ms_tt",  "TikTok",    tt.prev, tt.count, tt.source != SOURCE_OFF && tt.ok);
  checkMilestone("ms_all", "Combined",  lastTotal, total,  lastTotal >= 0 && total >= 0);
  lastTotal = total;
  Serial.printf("[total] %ld  (free heap %u)\n", total, ESP.getFreeHeap());
}

// =====================================================================
//  Wi-Fi
// =====================================================================
static String wifiNames() {
  String s;
  for (size_t i = 0; i < WIFI_COUNT; i++) {
    if (!WIFI_LIST[i].ssid || !*WIFI_LIST[i].ssid) continue;
    if (s.length()) s += ", ";
    s += WIFI_LIST[i].ssid;
  }
  return s;
}

static void startWifi() {
  WiFi.setHostname(WIFI_HOSTNAME);   // must come before WiFi.mode() on the ESP32
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  for (size_t i = 0; i < WIFI_COUNT; i++) {
    if (WIFI_LIST[i].ssid && *WIFI_LIST[i].ssid) wifiMulti.addAP(WIFI_LIST[i].ssid, WIFI_LIST[i].pass);
  }
}

// Scan for any of the known networks and join the strongest one. Blocks for a few seconds.
static bool tryWifi(uint32_t timeoutMs) {
  lastWifiTryMs = millis();
  Serial.printf("[wifi] looking for: %s\n", wifiNames().c_str());
  bool ok = wifiMulti.run(timeoutMs) == WL_CONNECTED;
  if (ok) Serial.printf("[wifi] connected to '%s', IP %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
  else    Serial.println("[wifi] none of the known networks answered");
  return ok;
}

static void ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) return;
  // Phone hotspots come and go (the phone sleeps, you walk away...), so keep looking.
  if (millis() - lastWifiTryMs > 15000) {
    setFooter("No Wi-Fi, looking for: " + wifiNames(), true);
    if (tryWifi(8000)) {
      setFooter("Connected to " + WiFi.SSID(), false);
      forceRefresh = true;
    }
    drawHeader();
  }
}

// =====================================================================
//  setup / loop
// =====================================================================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\nESP32 follower display starting");

  pinMode(PIN_LED_R, OUTPUT); pinMode(PIN_LED_G, OUTPUT); pinMode(PIN_LED_B, OUTPUT);
  setLed(false, false, false);
  pinMode(PIN_BACKLIGHT, OUTPUT);
  digitalWrite(PIN_BACKLIGHT, HIGH);

  tft.init();
  tft.setRotation(SCREEN_ROTATION);
  tft.fillScreen(C_BG);

  touchSPI.begin(XPT_CLK, XPT_MISO, XPT_MOSI, XPT_CS);
  touch.begin(touchSPI);
  touch.setRotation(SCREEN_ROTATION);

  prefs.begin("followers", false);
  ig.count = prefs.getLong(ig.prefKey, -1);   // show the last known numbers right away
  tt.count = prefs.getLong(tt.prefKey, -1);
  lastTotal = currentTotal();

  splash("Looking for Wi-Fi", wifiNames());
  startWifi();
  unsigned long t0 = millis();
  bool connected = false;
  while (!connected && millis() - t0 < 30000) connected = tryWifi(10000);
  if (connected) {
    splash("Connected to " + WiFi.SSID(), WiFi.localIP().toString());
    configTzTime(TZ_INFO, "pool.ntp.org", "time.nist.gov", "time.google.com");
    splash("Getting the time", "");
    t0 = millis();
    while (!timeSynced() && millis() - t0 < 8000) delay(100);
  } else {
    Serial.println("[wifi] could not connect, will keep trying");
  }

  tft.fillScreen(C_BG);
  drawAll();
}

// Dashboard: tap a card or the total to zoom it, tap the header/footer to refresh.
// Zoomed in: tap anywhere to go back.
static void handleTap(int x, int y) {
  Serial.printf("[touch] %d,%d\n", x, y);
  if (view != VIEW_DASH) { setView(VIEW_DASH); return; }
  if (y >= CARD_Y && y < CARD_Y + CARD_H) {
    if (x < SCR_W / 2) { if (ig.source != SOURCE_OFF) setView(VIEW_IG); }
    else               { if (tt.source != SOURCE_OFF) setView(VIEW_TT); }
    return;
  }
  if (y >= TOTAL_Y && y < TOTAL_Y + TOTAL_H) { setView(VIEW_TOTAL); return; }
  forceRefresh = true;
  Serial.println("[touch] manual refresh");
}

void loop() {
  ensureWifi();

  if (touch.tirqTouched() && touch.touched()) {
    if (millis() - lastTouchMs > 600) {
      lastTouchMs = millis();
      TS_Point pt = touch.getPoint();
      int x = constrain(map(pt.x, TOUCH_MIN_X, TOUCH_MAX_X, 0, SCR_W - 1), 0, SCR_W - 1);
      int y = constrain(map(pt.y, TOUCH_MIN_Y, TOUCH_MAX_Y, 0, SCR_H - 1), 0, SCR_H - 1);
      handleTap(x, y);
    }
  }

  if (forceRefresh || (long)(millis() - nextFetchMs) >= 0) {
    forceRefresh = false;
    fetchAll();
    if (WiFi.status() != WL_CONNECTED) nextFetchMs = millis() + 30000;   // retry soon
  }

  if (millis() - lastBarsMs >= 1000) {
    lastBarsMs = millis();
    drawHeader();
    drawFooter();
  }
  delay(20);
}
