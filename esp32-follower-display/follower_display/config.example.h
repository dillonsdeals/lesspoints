// Copy this file to config.h (same folder) and fill in your own values.
// config.h is git-ignored so your Wi-Fi password and tokens stay out of the repo.
#pragma once

// ---------------- Wi-Fi ----------------
// List every network the display may use, MOST PREFERRED FIRST. It joins the first one it can
// see, and if it is on a lower one (your hotspot) when a higher one (home) comes into range it
// switches over within a couple of minutes. Put home first and the hotspot second.
//
// iPhone hotspot notes:
//   * The network name is your phone's name (Settings > General > About > Name).
//     iOS writes the apostrophe as a curly one, so "Dillon's iPhone" is really "Dillon’s iPhone".
//     Copy the name exactly, or rename the phone to something simple like "DillonPhone".
//   * The ESP32 only does 2.4 GHz: turn ON  Settings > Personal Hotspot > Maximize Compatibility.
//   * Keep the Personal Hotspot screen open on the phone the first time so it keeps broadcasting
//     until the display has joined. After that it reconnects on its own.
#define WIFI_NETWORKS \
  { "YourHomeNetwork",  "YourHomePassword"  }, \
  { "Dillon’s iPhone",  "YourHotspotPassword" }

#define WIFI_HOSTNAME "follower-display"   // how the display shows up in the hotspot's client list

// ---------------- General ----------------
#define REFRESH_INTERVAL_MIN 5        // minutes between updates (tap the screen to refresh sooner)
#define SCREEN_ROTATION      1        // 1 = landscape, 3 = landscape flipped 180 degrees
#define CLOCK_24H            0        // 1 = 24-hour clock in the header
// POSIX timezone string. Examples:
//   New York  "EST5EDT,M3.2.0,M11.1.0"    Chicago  "CST6CDT,M3.2.0,M11.1.0"
//   Denver    "MST7MDT,M3.2.0,M11.1.0"    LA       "PST8PDT,M3.2.0,M11.1.0"
//   London    "GMT0BST,M3.5.0/1,M10.5.0"  Berlin   "CET-1CEST,M3.5.0,M10.5.0/3"
#define TZ_INFO "EST5EDT,M3.2.0,M11.1.0"

// ---------------- Look ----------------
#define THEME 1                   // 1 = money (dollar green + gold), 0 = classic dark
#define DISPLAY_TITLE "DILLON'S DEALS"   // header text on the dashboard
#define ZOOM_LCD_DIGITS 1         // cash-register digits when a panel is zoomed (0 = serif numerals)
// Touch calibration: only change if taps land in the wrong place (raw 0..4095 -> pixels)
// #define TOUCH_MIN_X 200
// #define TOUCH_MAX_X 3700
// #define TOUCH_MIN_Y 240
// #define TOUCH_MAX_Y 3800

// ---------------- Plane tracker ----------------
// Tap the header title to switch between the follower counter and the radar.
#define PLANES_ENABLED      1
#define HOME_LAT            0.0     // your latitude, e.g. 40.7128  (0,0 = guess from the internet connection)
#define HOME_LON            0.0     // your longitude, e.g. -74.0060
#define PLANES_RADIUS_NM    10      // how far to look, in nautical miles (10 nm = about 11.5 miles)
#define PLANES_REFRESH_SEC  15
#define PLANES_UNITS_MILES  1       // 1 = miles / mph, 0 = nautical miles / knots
#define PLANES_ROUTES       1       // show where the flight is going
#define PLANES_PHOTOS       1       // show a photo of the actual aircraft (planespotters.net)

// ---------------- Fun ----------------
#define ANIMATE_COUNT_UP      1   // numbers roll up when you gain followers
#define FOLLOWER_PARTY        1   // fireworks over the NYC skyline on every new follower (0 = small coin burst)
#define FOLLOWER_PARTY_SECONDS 4
#define CELEBRATE_MILESTONES  1   // confetti when a count crosses a round number (100s, 500s, 1,000s...)
#define CELEBRATION_SECONDS   6   // how long the confetti lasts (tap the screen to skip)

// ---------------- Instagram ----------------
// SOURCE_API    : official Instagram API (needs a Business/Creator account + access token, most reliable)
// SOURCE_SCRAPE : reads instagram.com/<username>/ directly (no setup, but Instagram often shows a login wall)
// SOURCE_BRIDGE : asks the helper script in ../bridge running on your PC / Raspberry Pi
// SOURCE_OFF    : hide Instagram
#define INSTAGRAM_SOURCE        SOURCE_API
#define INSTAGRAM_USERNAME      "yourhandle"
#define INSTAGRAM_ACCESS_TOKEN  "EAA..."      // token from the Meta developer dashboard (see README)
// Graph API Explorer route (never-expiring Page token, the route the README describes):
// fill in your Instagram user id and keep these two lines enabled.
#define INSTAGRAM_API_BASE "https://graph.facebook.com/v22.0/<YOUR_IG_USER_ID>"
#define INSTAGRAM_AUTO_REFRESH_TOKEN 0
// Older apps with "API setup with Instagram login" (60-day IGAA... token): delete the two lines above.

// ---------------- TikTok ----------------
// SOURCE_API    : official TikTok Display API (needs a developer app + refresh token)
// SOURCE_SCRAPE : reads tiktok.com/@<username> directly (no setup, may get a bot-check page sometimes)
// SOURCE_BRIDGE : asks the helper script in ../bridge
// SOURCE_OFF    : hide TikTok
#define TIKTOK_SOURCE         SOURCE_SCRAPE
#define TIKTOK_USERNAME       "yourhandle"
#define TIKTOK_CLIENT_KEY     ""
#define TIKTOK_CLIENT_SECRET  ""
#define TIKTOK_REFRESH_TOKEN  ""

// ---------------- Bridge (only for SOURCE_BRIDGE) ----------------
#define BRIDGE_URL "http://192.168.1.50:8787/followers"
