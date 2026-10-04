import os
import subprocess
import time
import serial

PORT     = os.environ.get("BUDDY_PORT", "COM5")   # your Nano's port
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

def to_serial_line(text):
    lines = [l.strip() for l in text.splitlines() if l.strip()]
    return "|".join(lines[:8])

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