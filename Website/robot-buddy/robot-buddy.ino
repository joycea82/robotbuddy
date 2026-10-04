// Robot Buddy - Jolemmy

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSans9pt7b.h>

// ---------- Hardware ----------
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_CS       10
#define OLED_DC       6
#define OLED_RESET    5
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI, OLED_DC, OLED_RESET, OLED_CS);

const int buzzer   = 9;
const int MPU_ADDR = 0x68;

// ---------- Settings ----------
const float ANGLE_THRESHOLD = 30.0;    // degrees the door must swing from closed
const float HYSTERESIS      = 3.0;
const unsigned long PAGE_MS  = 3000;   // how long each screen shows
const unsigned long PRINT_MS = 250;    // how often to print live readings

const float GYRO_SCALE = 65.5;         // LSB per deg/s at the +/-500 deg/s range
const float REST_RATE  = 2.0;          // deg/s: below this the door counts as not moving
const float REST_ZONE  = 10.0;         // deg: only re-zero the angle when this close to closed

const int TASK_TEXT_X = 16, TASK_FIRST_Y = 12, TASK_LINE_H = 14, TASK_GAP = 4, TASK_MAX_BASELINE = 60;

// ---------- State ----------
const int MAX_ITEMS = 12;
String items[MAX_ITEMS], tasks[MAX_ITEMS];   // assignments, and tasks (lines starting with *)
int itemCount = 0, taskCount = 0, taskPageTotal = 0;
String dateStr, timeStr, rxBuffer;
int dueToday = 0, weather = -1;              // weather: -1 unknown, 0 sun, 1 cloud, 2 rain
bool haveData = false, haveTime = false, infoChanged = false;
bool tilted = false, wasTilted = false, soundPlayed = false;
unsigned long tiltStart = 0;
unsigned long lastPrint = 0;
int lastPage = -2;                           // -2 nothing drawn, -1 resting screen, 0+ info screens

float gxBias = 0;                            // gyro X resting offset, deg/s
float angX = 0;                              // integrated door angle, deg (0 = closed)
unsigned long lastMicros = 0;

// ---------- Sound (non-blocking) ----------
struct Note { uint16_t freq, ms; };          // freq 0 = rest

const Note ALARM[] = {
  {2000,150},{1400,150},{2000,150},{1400,150},{2000,150},{1400,150},{2000,150},{1400,150},
  {0,200},
  {2000,150},{1400,150},{2000,150},{1400,150},{2000,150},{1400,150},{2000,150},{1400,150}
};
const Note HAPPY[] = { {523,120},{659,120},{784,120},{1047,300},{0,60},{784,120},{1047,450} };

const Note* song = nullptr;
int songLen = 0, songIdx = 0;
unsigned long noteEnd = 0;

void playNote() {
  if (song[songIdx].freq) tone(buzzer, song[songIdx].freq);
  else noTone(buzzer);
  noteEnd = millis() + song[songIdx].ms;
}

void startSong(const Note* s, int len) { song = s; songLen = len; songIdx = 0; playNote(); }
void stopSound() { noTone(buzzer); song = nullptr; }

void updateSound() {
  if (!song || (long)(millis() - noteEnd) < 0) return;
  if (++songIdx >= songLen) stopSound();
  else playNote();
}

// ---------- Drawing helpers ----------
void drawFace(int cx, int cy, int r, int mood) {   // 0 sad, 1 neutral, 2 happy
  display.drawCircle(cx, cy, r, SSD1306_WHITE);
  display.fillCircle(cx - r / 3, cy - r / 3, max(1, r / 8), SSD1306_WHITE);
  display.fillCircle(cx + r / 3, cy - r / 3, max(1, r / 8), SSD1306_WHITE);

  if (mood == 1) {                                 // straight mouth
    display.drawLine(cx - r / 2, cy + r / 2, cx + r / 2, cy + r / 2, SSD1306_WHITE);
    return;
  }
  int mr = r / 2;
  int mcy = (mood == 2) ? cy : cy + (r * 7) / 10;  // smile = bottom arc, frown = top arc
  int a0  = (mood == 2) ? 20 : 200;
  for (int a = a0; a < a0 + 140; a += 10) {
    float r1 = a * PI / 180.0, r2 = (a + 10) * PI / 180.0;
    display.drawLine(cx + mr * cos(r1), mcy + mr * sin(r1),
                     cx + mr * cos(r2), mcy + mr * sin(r2), SSD1306_WHITE);
  }
}

void drawStar(int cx, int cy, int r) {             // filled 5-point star
  int px[10], py[10];
  for (int i = 0; i < 10; i++) {
    float a = (-90 + i * 36) * PI / 180.0;
    int rad = (i % 2 == 0) ? r : (r * 38) / 100;
    px[i] = cx + rad * cos(a);
    py[i] = cy + rad * sin(a);
  }
  for (int i = 0; i < 10; i++) {
    int n = (i + 1) % 10;
    display.fillTriangle(cx, cy, px[i], py[i], px[n], py[n], SSD1306_WHITE);
  }
}

void drawWeatherIcon(int cx, int cy) {
  if (weather < 0) return;
  if (weather == 0) {                              // sun
    display.fillCircle(cx, cy, 6, SSD1306_WHITE);
    for (int i = 0; i < 8; i++) {
      float a = i * 45 * PI / 180.0;
      display.drawLine(cx + 9 * cos(a), cy + 9 * sin(a), cx + 12 * cos(a), cy + 12 * sin(a), SSD1306_WHITE);
    }
    return;
  }
  int y = (weather == 2) ? cy - 4 : cy;            // cloud (raised a bit when raining)
  display.fillCircle(cx - 7, y + 2, 5, SSD1306_WHITE);
  display.fillCircle(cx,     y - 2, 7, SSD1306_WHITE);
  display.fillCircle(cx + 8, y + 3, 4, SSD1306_WHITE);
  display.fillRect(cx - 7, y + 2, 16, 6, SSD1306_WHITE);
  if (weather == 2) {                              // raindrops
    for (int dx = -6; dx <= 6; dx += 6)
      display.drawLine(cx + dx, cy + 5, cx + dx - 2, cy + 10, SSD1306_WHITE);
  }
}

// Wraps text by pixel width in the current font. Draws it if draw is true. Returns the line count.
int wrapText(String s, int x, int y, int maxW, int lineH, bool draw) {
  String line = "";
  int lines = 1, i = 0, len = s.length();
  while (i < len) {
    int j = s.indexOf(' ', i);
    if (j < 0) j = len;
    String word = s.substring(i, j);
    String test = line.length() ? line + " " + word : word;

    int16_t x1, y1; uint16_t w, h;
    display.getTextBounds(test, 0, 0, &x1, &y1, &w, &h);

    if (w > maxW && line.length()) {
      if (draw) { display.setCursor(x, y); display.print(line); }
      y += lineH; lines++;
      line = word;
    } else {
      line = test;
    }
    i = j + 1;
  }
  if (draw) { display.setCursor(x, y); display.print(line); }
  return lines;
}

// ---------- Screens ----------
void drawMessage(String msg) {                     // intro text, e.g. "You have 2 tasks to do:"
  display.setFont(&FreeSansBold9pt7b);
  wrapText(msg, 0, 13, SCREEN_WIDTH, 16, true);
}

void drawAssignment(int index) {                   // bold title, lighter due date at the bottom
  String title = items[index], due = "";
  int semi = title.indexOf(';');
  if (semi >= 0) { due = title.substring(semi + 1); title = title.substring(0, semi); }
  title.trim(); due.trim();

  display.setFont(&FreeSansBold9pt7b);
  wrapText(title, 0, 13, SCREEN_WIDTH, 17, true);
  display.setFont(&FreeSans9pt7b);
  wrapText(due, 0, 61, SCREEN_WIDTH, 17, true);
}

// Lays out tasks across screens. Draws screen p if p >= 0. Returns the number of task screens.
int layoutTasks(int p) {
  display.setFont(&FreeSans9pt7b);
  int page = 0, used = 0;
  for (int i = 0; i < taskCount; i++) {
    int n = wrapText(tasks[i], 0, 0, SCREEN_WIDTH - TASK_TEXT_X, TASK_LINE_H, false);
    if (TASK_FIRST_Y + used + (n - 1) * TASK_LINE_H > TASK_MAX_BASELINE && used > 0) { page++; used = 0; }
    if (page == p) {
      int y = TASK_FIRST_Y + used;
      drawStar(6, y - 5, 5);
      wrapText(tasks[i], TASK_TEXT_X, y, SCREEN_WIDTH - TASK_TEXT_X, TASK_LINE_H, true);
    }
    used += n * TASK_LINE_H + TASK_GAP;
  }
  return taskCount ? page + 1 : 0;
}

void drawRestScreen(int mood) {                    // face top right, weather bottom right, time + date left
  drawFace(115, 12, 12, mood);
  drawWeatherIcon(113, 50);

  display.setFont(&FreeSansBold9pt7b);
  display.setCursor(0, 16);
  if (!haveTime) { display.print("Waiting..."); return; }
  display.print(timeStr);

  display.setFont(&FreeSans9pt7b);
  display.setCursor(0, 38);
  display.print(dateStr);
}

// Order: assignment intro, assignments, task intro, task screens
void drawPage(int p) {
  if (itemCount > 0) {
    if (p == 0) {
      drawMessage(String("You have ") + itemCount + (itemCount == 1 ? " assignment due this week:" : " assignments due this week:"));
      return;
    }
    if (p <= itemCount) { drawAssignment(p - 1); return; }
    p -= itemCount + 1;
  }
  if (taskCount > 0) {
    if (p == 0) drawMessage(String("You have ") + taskCount + (taskCount == 1 ? " task to do:" : " tasks to do:"));
    else layoutTasks(p - 1);
    return;
  }
  drawMessage("Nothing due!");
}

// ---------- Input ----------
bool readGyroX(int16_t &gx) {                      // GYRO_XOUT_H / L registers
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x43);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(MPU_ADDR, 2) != 2) return false;
  gx = (Wire.read() << 8) | Wire.read();
  return true;
}

// A message looks like:  @date,time,dueToday,weather|assignment1|assignment2|*task1|*task2
void parseMessage(String msg) {
  itemCount = taskCount = 0;
  int start = 0;
  while (start <= (int)msg.length()) {
    int sep = msg.indexOf('|', start);
    String piece = (sep < 0) ? msg.substring(start) : msg.substring(start, sep);
    piece.trim();

    if (piece.length()) {
      if (piece[0] == '@') {                       // header
        int c1 = piece.indexOf(','), c2 = piece.indexOf(',', c1 + 1), c3 = piece.indexOf(',', c2 + 1);
        if (c1 > 0 && c2 > c1) {
          dateStr  = piece.substring(1, c1);
          timeStr  = piece.substring(c1 + 1, c2);
          dueToday = piece.substring(c2 + 1, (c3 > c2) ? c3 : piece.length()).toInt();
          if (c3 > c2) weather = piece.substring(c3 + 1).toInt();
          haveTime = true;
        }
      } else if (piece[0] == '*') {                // task
        piece = piece.substring(1);
        piece.trim();
        if (piece.length() && taskCount < MAX_ITEMS) tasks[taskCount++] = piece;
      } else if (itemCount < MAX_ITEMS) {          // assignment
        items[itemCount++] = piece;
      }
    }
    if (sep < 0) break;
    start = sep + 1;
  }
  haveData = infoChanged = true;
}

void readSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') { rxBuffer.trim(); parseMessage(rxBuffer); rxBuffer = ""; }
    else if (c != '\r' && rxBuffer.length() < 600) rxBuffer += c;
  }
}

// ---------- Main ----------
void setup() {
  pinMode(buzzer, OUTPUT);
  Serial.begin(9600);

  if (!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println("OLED NOT FOUND");
    while (true);
  }
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextWrap(false);

  Wire.begin();
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);                                // wake the sensor
  Wire.write(0);
  if (Wire.endTransmission() != 0) {
    Serial.println("MPU6050 NOT FOUND");
    while (true);
  }

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1B);                                // gyro range: +/-500 deg/s
  Wire.write(0x08);
  Wire.endTransmission();
  delay(100);

  // Calibrate the gyro with the door closed and still (about 1 second)
  Serial.println("Calibrating gyro, keep the door closed and still...");
  float sum = 0;
  int good = 0;
  for (int i = 0; i < 200; i++) {
    int16_t gx;
    if (readGyroX(gx)) { sum += gx / GYRO_SCALE; good++; }
    delay(5);
  }
  if (good) gxBias = sum / good;

  Serial.println("=== STARTUP READING ===");
  Serial.print("  gyro X bias: "); Serial.print(gxBias, 2);
  Serial.println(" deg/s");
  Serial.println("=======================");

  display.clearDisplay();
  display.display();
  lastMicros = micros();
}

void loop() {
  readSerial();
  updateSound();

  int16_t gx;
  if (!readGyroX(gx)) {
    Serial.println("Gyro read error");
    delay(100);
    return;
  }

  // Integrate the gyro X rate into a door angle (0 = closed)
  unsigned long now = micros();
  float dt = (now - lastMicros) / 1000000.0;
  lastMicros = now;

  float rate = gx / GYRO_SCALE - gxBias;
  angX += rate * dt;

  // Fight drift: when the door is closed and not moving, re-zero and fine-tune the bias
  if (!tilted && fabs(rate) < REST_RATE && fabs(angX) < REST_ZONE) {
    gxBias += 0.01 * rate;
    angX = 0;
  }

  if (!tilted && fabs(angX) > ANGLE_THRESHOLD) tilted = true;
  else if (tilted && fabs(angX) < ANGLE_THRESHOLD - HYSTERESIS) tilted = false;

  if (tilted != wasTilted) {                       // door opened or closed
    if (tilted) { tiltStart = millis(); soundPlayed = false; }
    else stopSound();
    Serial.print("Angle: "); Serial.print(angX, 1);
    Serial.println(tilted ? "  -> OPEN" : "  -> CLOSED");
    wasTilted = tilted;
  }

  // Live readout (throttled)
  if (millis() - lastPrint >= PRINT_MS) {
    lastPrint = millis();
    Serial.print("rate: ");        Serial.print(rate, 1);
    Serial.print("  angle: ");     Serial.print(angX, 1);
    Serial.print("  bias: ");      Serial.print(gxBias, 2);
    Serial.println(tilted ? "  [OPEN]" : "  [closed]");
  }

  // Alarm if assignments are due, otherwise a happy tune (once per door opening)
  if (tilted && !soundPlayed && haveData) {
    if (itemCount > 0) startSong(ALARM, sizeof(ALARM) / sizeof(ALARM[0]));
    else               startSong(HAPPY, sizeof(HAPPY) / sizeof(HAPPY[0]));
    soundPlayed = true;
  }

  if (infoChanged) taskPageTotal = layoutTasks(-1);   // recount task screens when data changes

  // Which screen to show (-1 = resting screen)
  int totalPages = (itemCount ? itemCount + 1 : 0) + (taskCount ? taskPageTotal + 1 : 0);
  int page = !tilted ? -1 : (totalPages == 0 ? 0 : ((millis() - tiltStart) / PAGE_MS) % totalPages);

  if (page != lastPage || infoChanged) {
    display.clearDisplay();
    display.setFont();

    if (page == -1) {
      drawRestScreen(itemCount == 0 ? 2 : (dueToday > 0 ? 0 : 1));   // happy / sad / neutral
    } else if (!haveData) {
      display.setCursor(0, 0);
      display.print("No data yet");
    } else {
      drawPage(page);
    }

    display.display();
    lastPage = page;
    infoChanged = false;
  }

  delay(10);
}