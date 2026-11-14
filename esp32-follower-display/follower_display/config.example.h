// Copy this file to config.h (same folder) and fill in your own values.
// config.h is git-ignored so your Wi-Fi password and tokens stay out of the repo.
#pragma once

// ---------------- Wi-Fi ----------------
// List every network the display may use. It joins the strongest one it can see, and keeps
// looking whenever it loses the connection, so you can put your home Wi-Fi AND your phone's
// hotspot here and carry the display around.
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

// ---------------- Instagram ----------------
// SOURCE_API    : official Instagram API (needs a Business/Creator account + access token, most reliable)
// SOURCE_SCRAPE : reads instagram.com/<username>/ directly (no setup, but Instagram often shows a login wall)
// SOURCE_BRIDGE : asks the helper script in ../bridge running on your PC / Raspberry Pi
// SOURCE_OFF    : hide Instagram
#define INSTAGRAM_SOURCE        SOURCE_API
#define INSTAGRAM_USERNAME      "yourhandle"
#define INSTAGRAM_ACCESS_TOKEN  "IGAA..."     // long-lived token from the Meta developer dashboard
// Only needed if you use the Facebook-Login flavour of the API instead of "Instagram Login":
// #define INSTAGRAM_API_BASE "https://graph.facebook.com/v22.0/<YOUR_IG_USER_ID>"
// #define INSTAGRAM_AUTO_REFRESH_TOKEN 0

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
