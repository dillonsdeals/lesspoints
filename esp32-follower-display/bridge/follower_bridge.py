#!/usr/bin/env python3
"""
Optional helper for the ESP32 follower display.

Runs on any always-on computer (PC, Mac, Raspberry Pi) on the same network, fetches your
public Instagram and TikTok follower counts, and serves them as JSON for the ESP32:

    GET /followers  ->  {"instagram": 1234, "tiktok": 5678}

Usage:
    pip install requests
    python3 follower_bridge.py --instagram yourhandle --tiktok yourhandle

Then set INSTAGRAM_SOURCE / TIKTOK_SOURCE to SOURCE_BRIDGE in config.h and point
BRIDGE_URL at http://<this computer's IP>:8787/followers
"""
import argparse
import json
import re
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, HTTPServer

import requests

UA = ("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
      "(KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36")


def instagram_followers(username: str) -> int:
    # Public JSON endpoint used by the Instagram web app.
    r = requests.get("https://i.instagram.com/api/v1/users/web_profile_info/",
                     params={"username": username},
                     headers={"User-Agent": UA, "X-IG-App-ID": "936619743392459"},
                     timeout=15)
    if r.ok:
        try:
            return int(r.json()["data"]["user"]["edge_followed_by"]["count"])
        except (ValueError, KeyError, TypeError):
            pass
    # Fallback: the <meta> description on the public profile page ("1,234 Followers, ...")
    r = requests.get(f"https://www.instagram.com/{username}/", headers={"User-Agent": UA}, timeout=15)
    r.raise_for_status()
    m = re.search(r'content="([\d.,]+[KkMmBb]?) Followers,', r.text)
    if not m:
        raise RuntimeError("Instagram did not return a follower count (login wall?)")
    return parse_count(m.group(1))


def tiktok_followers(username: str) -> int:
    r = requests.get(f"https://www.tiktok.com/@{username}?lang=en",
                     headers={"User-Agent": UA, "Accept-Language": "en-US,en;q=0.9"}, timeout=15)
    r.raise_for_status()
    m = re.search(r'"followerCount":(\d+)', r.text)
    if not m:
        raise RuntimeError("followerCount not found (TikTok may have served a bot-check page)")
    return int(m.group(1))


def parse_count(text: str) -> int:
    mult = {"k": 1e3, "m": 1e6, "b": 1e9}.get(text[-1].lower(), 1)
    if mult != 1:
        text = text[:-1]
    return int(round(float(text.replace(",", "")) * mult))


class Cache:
    def __init__(self, args):
        self.args = args
        self.data = {}
        self.ts = 0.0
        self.lock = threading.Lock()

    def get(self):
        with self.lock:
            if time.time() - self.ts >= self.args.interval:
                self.refresh()
            return dict(self.data)

    def refresh(self):
        jobs = [("instagram", instagram_followers, self.args.instagram),
                ("tiktok", tiktok_followers, self.args.tiktok)]
        for key, fn, user in jobs:
            if not user:
                continue
            try:
                self.data[key] = fn(user)
                self.data.pop(key + "_error", None)
                print(f"{time.strftime('%H:%M:%S')} {key}: {self.data[key]}", flush=True)
            except Exception as e:  # keep the last good value, report the error alongside it
                self.data[key + "_error"] = str(e)[:120]
                print(f"{time.strftime('%H:%M:%S')} {key}: ERROR {e}", file=sys.stderr, flush=True)
        self.ts = time.time()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--instagram", default="", help="Instagram username (without @)")
    ap.add_argument("--tiktok", default="", help="TikTok username (without @)")
    ap.add_argument("--port", type=int, default=8787)
    ap.add_argument("--interval", type=int, default=300, help="seconds between upstream fetches")
    args = ap.parse_args()
    if not args.instagram and not args.tiktok:
        ap.error("give at least one of --instagram / --tiktok")

    cache = Cache(args)

    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            if self.path.split("?")[0] != "/followers":
                self.send_error(404)
                return
            body = json.dumps(cache.get()).encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, *a):  # quiet
            pass

    print(f"Serving http://0.0.0.0:{args.port}/followers  (refresh every {args.interval}s)", flush=True)
    HTTPServer(("0.0.0.0", args.port), Handler).serve_forever()


if __name__ == "__main__":
    main()
