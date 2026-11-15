# ESP32 live follower display

Firmware for the **ESP32-2432S028R** 2.8" touchscreen board (the "Cheap Yellow Display", sold by
DIYmalls and others) that shows your **Instagram followers, TikTok followers and the combined total**,
refreshed every few minutes. Tap the screen to refresh right away. The on-board RGB LED blinks green
when the total goes up.

```
+--------------------------------------------------+
| LIVE FOLLOWERS                     12:40 PM   *  |
| +--------------------+  +----------------------+ |
| | Instagram          |  | TikTok               | |
| | @you               |  | @you                 | |
| |      12,345        |  |       67,890         | |
| |        +12         |  |         +3           | |
| +--------------------+  +----------------------+ |
| | TOTAL                                  +15   | |
| |                 80,235                       | |
| +----------------------------------------------+ |
|  Updated 2m ago | next in 2:58 | tap = refresh    |
+--------------------------------------------------+
```

It works on home Wi-Fi **or an iPhone / Android hotspot**: list both networks in the config and it
joins whichever is in range.

## What you need

* ESP32-2432S028R board (the one with a single micro-USB port and a resistive touch screen) and its USB cable
* A computer with either [PlatformIO](https://platformio.org/install) (recommended) or the Arduino IDE
* A 2.4 GHz Wi-Fi network or a phone hotspot
* Your Instagram and TikTok usernames, plus (optionally) API tokens, see *Data sources* below

No wiring: the screen, touch panel and LED are all on the board.

## Setup

1. Copy `follower_display/config.example.h` to `follower_display/config.h` and fill it in
   (Wi-Fi networks, usernames, which data source to use for each platform). `config.h` is git-ignored
   so your passwords and tokens never end up in the repo.
2. Build and flash.

### PlatformIO (recommended)

```bash
cd esp32-follower-display
pio run -t upload        # builds, downloads the libraries, flashes over USB
pio device monitor       # watch the log at 115200 baud
```

`platformio.ini` already contains the screen pin configuration for this board, so nothing else
needs editing.

### Arduino IDE

1. Boards Manager: install **esp32 by Espressif Systems**. Select board **ESP32 Dev Module**
   and set **Partition Scheme** to **Huge APP (3MB No OTA)**.
2. Library Manager: install **TFT_eSPI** (Bodmer), **ArduinoJson** (Benoit Blanchon, 7.x) and
   **XPT2046_Touchscreen** (Paul Stoffregen).
3. Copy `User_Setup_CYD.h` over `Documents/Arduino/libraries/TFT_eSPI/User_Setup.h`
   (this tells TFT_eSPI which pins the display uses).
4. Open `follower_display/follower_display.ino` and upload.

If the upload does not start, hold the **BOOT** button on the board while it says `Connecting...`.

## Using an iPhone hotspot

The display is happy on a hotspot, with three iPhone-specific details:

1. **Turn on "Maximize Compatibility"** in *Settings > Personal Hotspot*. The ESP32 only speaks
   2.4 GHz, and without this setting newer iPhones broadcast 5 GHz only.
2. **Use the exact network name.** The hotspot name is your phone's name, and iOS writes the
   apostrophe as a curly `’` (so `Dillon’s iPhone`, not `Dillon's iPhone`). Either paste the name
   exactly into `config.h`, or rename the phone to something simple in
   *Settings > General > About > Name* (for example `DillonPhone`).
3. **Keep the Personal Hotspot screen open** on the phone while the display boots the first time.
   iOS only advertises the hotspot for a short while unless that screen is open or a device is
   already connected. Once the display has joined, the hotspot stays up on its own.

In `config.h` list the hotspot next to your home network:

```c
#define WIFI_NETWORKS \
  { "HomeWifi",         "homepassword"    }, \
  { "Dillon’s iPhone",  "hotspotpassword" }
```

The display picks the strongest known network it can see. If the phone goes out of range, the footer
shows *No Wi-Fi, looking for: ...* and it retries every 15 seconds, so when you come back (or open the
Hotspot screen again) it reconnects by itself. It shows up in the hotspot's client list as
`follower-display`.

Android hotspots work the same way; just make sure the hotspot is set to 2.4 GHz.

Note that the hotspot shares your phone's cellular data. The display uses very little: the official
APIs are a few kilobytes per refresh; the scrape modes download part of a web page (a few hundred KB)
per refresh, so raise `REFRESH_INTERVAL_MIN` if you are on a tight data plan.

## Data sources

Each platform can be read in one of three ways. Set `INSTAGRAM_SOURCE` and `TIKTOK_SOURCE` in
`config.h`.

| Mode | Setup effort | Reliability | Notes |
|------|--------------|-------------|-------|
| `SOURCE_API` | one-time token setup | best | official APIs, exact counts |
| `SOURCE_SCRAPE` | none | so-so | reads the public profile page; Instagram often shows a login wall, TikTok sometimes a bot check |
| `SOURCE_BRIDGE` | run a script on a PC/Pi | good | the script does the fetching, the ESP32 just asks it |

A sensible starting point is `SOURCE_API` for Instagram and `SOURCE_SCRAPE` for TikTok.

### Instagram: official API token

Requires an Instagram **Professional** account (Business or Creator, free to switch:
*Instagram app > Settings > Account type and tools > Switch to professional account*) that is
**linked to a Facebook Page** (*Instagram app > Edit profile > Page > Connect or create*; any page
name is fine, it can stay empty).

Newer Meta apps no longer offer the "API setup with Instagram login" shortcut, so use the
Facebook-login route below. It is all clicking in the browser and the resulting token never expires.

1. Go to <https://developers.facebook.com/apps>, **Create App**, pick the use case
   **"Manage messaging and content on Instagram"**, and finish the wizard.
2. Open **Tools > Graph API Explorer**. On the right, choose your app under *Meta App*, and under
   *Permissions* add `pages_show_list`, `instagram_basic`, `pages_read_engagement` and
   `business_management`. Click **Generate Access Token**, log in, and select your Instagram account
   and its Page.
3. Make the token permanent: copy the token from the *Access Token* box, open
   **Tools > Access Token Debugger**, paste it, click **Debug**, then **Extend Access Token** at the
   bottom and copy the extended token. Paste that back into the Explorer's *Access Token* box.
4. In the Explorer query box enter `me/accounts?fields=name,access_token,instagram_business_account`
   and press **Submit**. The reply contains the Page's `access_token` (this one never expires) and
   `instagram_business_account.id` (your Instagram user id).
5. Optional check: query `<IG_USER_ID>?fields=username,followers_count` with the Page token in the
   *Access Token* box. You should see your follower count.
6. In `config.h`:

   ```c
   #define INSTAGRAM_SOURCE        SOURCE_API
   #define INSTAGRAM_ACCESS_TOKEN  "<the Page access token>"
   #define INSTAGRAM_API_BASE      "https://graph.facebook.com/v22.0/<IG_USER_ID>"
   #define INSTAGRAM_AUTO_REFRESH_TOKEN 0
   ```

If you have an older app that still shows **API setup with Instagram login**, its *Generate token*
button gives a 60-day token for `graph.instagram.com`; leave `INSTAGRAM_API_BASE` at its default and
auto-refresh on, and the display renews it every 30 days.

### TikTok: official API refresh token

1. Go to <https://developers.tiktok.com/>, create an app, add **Login Kit**, and request the scopes
   `user.info.basic` and `user.info.stats`. Add a redirect URI (any HTTPS URL you control works,
   for example your own website; it only receives a code once). Add your TikTok account as a
   **Sandbox** target user so you do not need app review.
2. Open this URL in a browser (fill in your client key and the redirect URI), log in and approve:

   ```
   https://www.tiktok.com/v2/auth/authorize/?client_key=CLIENT_KEY&scope=user.info.basic,user.info.stats&response_type=code&redirect_uri=REDIRECT_URI&state=abc
   ```

3. Copy the `code=` value from the URL you land on (URL-decode it: `%2F` becomes `/`, etc.) and
   exchange it:

   ```bash
   curl -X POST https://open.tiktokapis.com/v2/oauth/token/ \
     -H 'Content-Type: application/x-www-form-urlencoded' \
     -d 'client_key=CLIENT_KEY&client_secret=CLIENT_SECRET&code=CODE&grant_type=authorization_code&redirect_uri=REDIRECT_URI'
   ```

4. Put `client_key`, `client_secret` and the `refresh_token` from the reply into `config.h`.

Refresh tokens last a year; the display turns them into 24-hour access tokens by itself and saves the
rotated refresh token in flash.

### Bridge script

If you prefer not to deal with tokens but want something sturdier than scraping from the ESP32, run
`bridge/follower_bridge.py` on any computer that is always on and on the same network:

```bash
pip install requests
python3 bridge/follower_bridge.py --instagram yourhandle --tiktok yourhandle
```

Then set the source(s) to `SOURCE_BRIDGE` and point `BRIDGE_URL` at
`http://<that computer's IP>:8787/followers`. (On a hotspot the computer must be on the hotspot too.)

## Look up: what's that plane?

The same screen doubles as a plane spotter. **Tap the header title** to switch between the follower
counter and "Look up" mode (it remembers which one you left it on).

* Shows the aircraft most directly **overhead** right now: a **photo of that exact airframe** (by its
  registration, from the planespotters.net archive, photographer credited), a compass arrow with
  **which way to look and how far up**, its distance and altitude, and underneath the callsign,
  airline, route (`JFK - LAX` and the city names) or type, speed and heading.
* **Tap** anywhere to step to the next plane. Tap the footer to refresh now.
* Flight data comes from the free [adsb.lol](https://adsb.lol) community feed, photos from
  [planespotters.net](https://www.planespotters.net). No keys needed. Refreshes every
  `PLANES_REFRESH_SEC` seconds.
* **Set `HOME_LAT` / `HOME_LON` in `config.h`.** Left at 0, the display guesses a rough location from
  your internet connection, which is often 10 to 30 miles off and worse on a phone hotspot, and then
  the plane you can see is not the plane it shows.
* `PLANES_RADIUS_NM` sets how far out to look, `PLANES_PHOTOS 0` turns the photos off, and
  `PLANES_ENABLED 0` removes the feature.

## Animations

* **Every new follower**: the number rolls up from the old value to the new one, then the card
  bursts with confetti and a big green `+1` (or `+N`) for about a second, with the LED pulsing green.
  The total rolls with it.
* **Milestones**: when Instagram, TikTok or the combined total crosses a round number, the screen
  fills with confetti showing the milestone for a few seconds and the LED cycles colours. Round
  numbers are every 100 below 1,000, every 500 below 10,000, every 1,000 below 100,000, every
  10,000 below a million, then every 100,000. Each milestone is celebrated once (remembered in flash),
  and a milestone you crossed while the display was unplugged is celebrated on the next boot. Tap to
  skip.

Both can be switched off in `config.h` (`ANIMATE_COUNT_UP`, `CELEBRATE_MILESTONES`), and
`CELEBRATION_SECONDS` sets the confetti length.

## Day-to-day

* **Tap a card** (Instagram or TikTok) or the **total** to blow it up full screen with a big number.
  Tap anywhere to go back to the dashboard.
* **Tap the header or footer** on the dashboard to refresh immediately.
* The look is a "money" theme by default: dollar-bill green and gold, banknote-style cards with serif
  numerals, ticker arrows for changes, a gold balance panel for the total, cash-register digits when
  zoomed, and money rain for milestones. `DISPLAY_TITLE` sets the header text, `ZOOM_LCD_DIGITS 0`
  swaps the register digits for serif numerals, and `THEME 0` gives the classic dark look.
* The header dot is green when Wi-Fi is up and red when it is not.
* If a fetch fails, the card keeps showing the last good number with *update failed* under it and the
  footer shows the reason. Open the serial monitor at 115200 baud for details.
* Follower counts are remembered across reboots, so the last numbers appear instantly at power-on.

## Troubleshooting

* **Colours look inverted or washed out**: uncomment `TFT_INVERSION_ON` in `platformio.ini`
  (or `User_Setup_CYD.h`). Some board revisions need it.
* **Screen stays white / black**: double-check you are using the pin setup from this folder; the
  default TFT_eSPI `User_Setup.h` is for a different board. Board variants with a USB-C port plus
  a micro-USB port use an ST7789 controller: change `ILI9341_2_DRIVER` to `ST7789_DRIVER` and enable
  inversion.
* **Touch does nothing**: the touch panel is resistive, press firmly with a fingernail or the stylus.
* **Sketch too big**: pick the *Huge APP* partition scheme (Arduino IDE) or use the included
  `platformio.ini`, which already does this.
* **Instagram says "login wall"** in scrape mode: Instagram rarely serves public pages to devices
  anymore. Use `SOURCE_API` or the bridge.
* **TikTok says "bot-check page"**: wait for the next refresh; if it never recovers use the bridge or
  the API.
* **Hotspot never connects**: check *Maximize Compatibility* is on, the name matches exactly
  (curly apostrophe), and the Personal Hotspot screen is open on the phone while the display boots.

## Notes

* TLS connections are made without certificate pinning so the display keeps working when sites
  rotate certificates. Fine for reading follower counts.
* Scrape mode reads public pages the same way a browser does; it is best-effort and can break if the
  sites change their HTML. The official APIs are the supported path.
* The TikTok client secret and Instagram token are stored on the device in plain flash. Do not lend
  the board to someone you would not lend your account to.
