import os
import subprocess
import time
import serial
import datetime
import re
import json
import urllib.request

PORT     = os.environ.get("BUDDY_PORT", "COM7")   # your Nano's port
INTERVAL = 15                                      # seconds between pulls

REPO_DIR = os.path.dirname(os.path.abspath(__file__))   # buddy.py is at the repo root
FILE     = os.path.join(REPO_DIR, "data.txt")

def pull():
    # --autostash stops the pull from failing if you have local edits
    r = subprocess.run(
        ["git", "-C", REPO_DIR, "pull", "-q", "--rebase", "--autostash"],
        capture_output=True, text=True,
    )
    if r.returncode != 0:
        print("git pull failed:", r.stderr.strip())

def read_file():
    with open(FILE, "r", encoding="utf-8") as f:
        return f.read()

MONTHS = ["jan","feb","mar","apr","may","jun","jul","aug","sep","oct","nov","dec"]

def is_due_today(line, today):
    # Looks for a date like "Oct 6" after the ';'. Unreadable dates count as not due today.
    due = line.split(";", 1)[1] if ";" in line else ""
    m = re.search(r"\b(" + "|".join(MONTHS) + r")[a-z]*\.?\s+(\d{1,2})\b", due, re.I)
    if not m:
        return False
    month = MONTHS.index(m.group(1).lower()) + 1
    return month == today.month and int(m.group(2)) == today.day

WEATHER_URL = ("https://api.open-meteo.com/v1/forecast?latitude=49.28&longitude=-123.12"
               "&current=weather_code&timezone=America%2FVancouver")
WEATHER_EVERY = 30 * 60        # only ask every 30 minutes
weather_kind = -1              # -1 = unknown, 0 = sun, 1 = cloud, 2 = rain
weather_checked = 0

def get_weather():
    global weather_kind, weather_checked
    if time.time() - weather_checked < WEATHER_EVERY:
        return weather_kind
    weather_checked = time.time()
    try:
        with urllib.request.urlopen(WEATHER_URL, timeout=5) as r:
            code = json.load(r)["current"]["weather_code"]      # no [0] here
        if code <= 1:
            weather_kind = 0                                   # clear / mostly clear
        elif 51 <= code <= 67 or 80 <= code <= 82 or code >= 95:
            weather_kind = 2                                   # drizzle, rain, showers, thunderstorm
        else:
            weather_kind = 1                                   # cloudy, fog, snow
    except Exception as e:
        print("Weather fetch failed:", e)
        weather_checked = time.time() - WEATHER_EVERY + 60     # retry in a minute
    return weather_kind

def to_serial_line(text):
    lines = [l.strip() for l in text.splitlines() if l.strip()]
    now = datetime.datetime.now()
    assignments = [l for l in lines if not l.startswith("*")]
    due_today = sum(is_due_today(l, now.date()) for l in assignments)

    date_str = f"{now:%a %b} {now.day}"                   # Sat Oct 3
    time_str = now.strftime("%I:%M %p").lstrip("0")       # 5:42 PM
    header = f"@{date_str},{time_str},{due_today},{get_weather()}"
    return "|".join([header] + lines[:12])

port = serial.Serial(PORT, 9600)
time.sleep(2)                                      # the Nano resets when the port opens

last_sent = None
while True:
    try:
        pull()
        line = to_serial_line(read_file())
        if line != last_sent:
            port.write((line + "\n").encode("ascii", errors="replace"))
            print("Sent:", line)
            last_sent = line
    except Exception as e:
        print("Problem:", e)
    time.sleep(INTERVAL)