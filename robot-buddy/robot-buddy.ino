// Robot Buddy - Jolemmy

// Libraries
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSans9pt7b.h>

// OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_CS     10
#define OLED_DC      6
#define OLED_RESET   5

// Create OLED object using hardware SPI
Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &SPI,
  OLED_DC,
  OLED_RESET,
  OLED_CS
);

// Constants
const int buzzer = 9; //buzzer to arduino pin 9

const int MPU_ADDR = 0x68;
const float ROLL_NEUTRAL   = -177.0;  // roll reading when sitting still
const float ROLL_THRESHOLD = 22.0;    // degrees away from neutral = tilted (-155 - -177)
const float HYSTERESIS     = 3.0;     // stops flicker right at the threshold

bool tilted = false;
bool wasTilted = false;

const int MAX_ITEMS = 8;
const unsigned long PAGE_MS = 3000;   // how long each screen shows
String items[MAX_ITEMS];              // each looks like "MSE 220: Assignment 1; Tues Oct 6"
int itemCount = 0;
bool haveData = false;
String rxBuffer = "";
bool infoChanged = false;
unsigned long tiltStart = 0;
int lastPage = -2;                    // -2 = nothing drawn yet, -1 = face, 0 = intro, 1+ = assignments


void drawFace(int cx, int cy, int r, bool happy) {
  // Head
  display.drawCircle(cx, cy, r, SSD1306_WHITE);

  // Eyes
  int eyeX = r / 3;
  int eyeY = cy - r / 3;
  int eyeR = max(1, r / 8);
  display.fillCircle(cx - eyeX, eyeY, eyeR, SSD1306_WHITE);
  display.fillCircle(cx + eyeX, eyeY, eyeR, SSD1306_WHITE);

  // Mouth: an arc made of short line segments
  int mr = r / 2;                         // mouth radius
  int mcy, startAngle, endAngle;

  if (happy) {
    mcy = cy;                             // bottom half of a circle = smile
    startAngle = 20;
    endAngle = 160;
  } else {
    mcy = cy + (r * 7) / 10;              // top half of a lower circle = frown
    startAngle = 200;
    endAngle = 340;
  }

  int prevX = 0, prevY = 0;
  for (int a = startAngle; a <= endAngle; a += 10) {
    float rad = a * PI / 180.0;
    int x = cx + mr * cos(rad);
    int y = mcy + mr * sin(rad);
    if (a > startAngle) {
      display.drawLine(prevX, prevY, x, prevY == 0 ? y : y, SSD1306_WHITE);
    }
    prevX = x;
    prevY = y;
  }
}

// Reads accelerometer, returns false on I2C failure
bool readAccel(int16_t &ax, int16_t &ay, int16_t &az) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);                       // first accel register
  if (Wire.endTransmission(false) != 0) return false;

  if (Wire.requestFrom(MPU_ADDR, 6) != 6) return false;
  ax = (Wire.read() << 8) | Wire.read();
  ay = (Wire.read() << 8) | Wire.read();
  az = (Wire.read() << 8) | Wire.read();
  return true;
}

// Reads incoming serial text. A message looks like:  item1|item2|item3
void readSerialLine() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      rxBuffer.trim();

      // Split on '|', skipping empty pieces
      itemCount = 0;
      int start = 0;
      while (itemCount < MAX_ITEMS && start <= (int)rxBuffer.length()) {
        int sep = rxBuffer.indexOf('|', start);
        String piece = (sep < 0) ? rxBuffer.substring(start) : rxBuffer.substring(start, sep);
        piece.trim();
        if (piece.length() > 0) items[itemCount++] = piece;
        if (sep < 0) break;
        start = sep + 1;
      }

      haveData = true;
      rxBuffer = "";
      infoChanged = true;
    } else if (c != '\r' && rxBuffer.length() < 400) {
      rxBuffer += c;
    }
  }
}

// Wraps text by pixel width in the current font. Returns the baseline y of the last line.
int drawWrappedPx(String s, int x, int y, int maxWidth, int lineH) {
  String line = "";
  int i = 0, len = s.length();
  while (i < len) {
    int j = s.indexOf(' ', i);
    if (j < 0) j = len;
    String word = s.substring(i, j);
    String test = (line.length() > 0) ? line + " " + word : word;

    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(test, 0, 0, &x1, &y1, &w, &h);

    if (w > maxWidth && line.length() > 0) {
      display.setCursor(x, y);
      display.print(line);
      y += lineH;
      line = word;
    } else {
      line = test;
    }
    i = j + 1;
  }
  display.setCursor(x, y);
  display.print(line);
  return y;
}

// "You have N assignments due this week."
void drawIntro() {
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);
  display.setFont(&FreeSansBold9pt7b);
  display.setTextSize(1);

  String msg = String("You have ") + itemCount +
               (itemCount == 1 ? " assignment due this week:" : " assignments due this week:");
  drawWrappedPx(msg, 0, 13, SCREEN_WIDTH, 16);   // 16 px between lines, fits 4 lines
}

// One assignment: bold title, lighter due date at the bottom
void drawAssignment(int index) {
  String item = items[index];
  String title = item;
  String due = "";

  int semi = item.indexOf(';');
  if (semi >= 0) {
    title = item.substring(0, semi);
    due = item.substring(semi + 1);
  }
  title.trim();
  due.trim();

  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);
  display.setTextSize(1);

  display.setFont(&FreeSansBold9pt7b);
  drawWrappedPx(title, 0, 13, SCREEN_WIDTH, 17);

  display.setFont(&FreeSans9pt7b);
  drawWrappedPx(due, 0, 61, SCREEN_WIDTH, 17);
}

void setup(){
  pinMode(buzzer, OUTPUT);

  Serial.begin(9600);

  // Start OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println("OLED NOT FOUND");
    while (true);
  }

  Serial.println("OLED connected!");

  // Start accelerometer
  Wire.begin();

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);                       // power management register
  Wire.write(0);                          // wake the sensor
  if (Wire.endTransmission() != 0) {
    Serial.println("MPU6050 NOT FOUND");
    while (true);
  }

  Serial.println("MPU6050 ready");

  display.clearDisplay();
  display.display();
}

void loop(){

  // Check for new text from the computer (first, so a failed sensor read can't skip it)
  readSerialLine();

  int16_t ax, ay, az;
  if (!readAccel(ax, ay, az)) {
    Serial.println("Accelerometer read error");
    delay(100);
    return;
  }

  // Roll in degrees (-180 to 180)
  float roll = atan2((float)ay, (float)az) * 180.0 / PI;

  // Difference from neutral, wrapped to -180..180 so the +/-180 jump doesn't matter
  float diff = roll - ROLL_NEUTRAL;
  if (diff > 180.0)  diff -= 360.0;
  if (diff < -180.0) diff += 360.0;

  // Hysteresis: easy to enter "tilted", slightly harder to leave it
  if (!tilted && fabs(diff) > ROLL_THRESHOLD) {
    tilted = true;
  } else if (tilted && fabs(diff) < ROLL_THRESHOLD - HYSTERESIS) {
    tilted = false;
  }

  // Note the moment a tilt begins so the sequence restarts at the intro
  if (tilted && !wasTilted) {
    tiltStart = millis();
    Serial.print("Roll: ");
    Serial.print(roll, 1);
    Serial.println("  -> TILTED");
  } else if (!tilted && wasTilted) {
    Serial.print("Roll: ");
    Serial.print(roll, 1);
    Serial.println("  -> NEUTRAL");
  }
  wasTilted = tilted;

  // Work out which screen should be showing
  // -1 = sad face, 0 = intro, 1..N = assignment number
  int page;
  if (!tilted) {
    page = -1;
  } else {
    int totalPages = itemCount + 1;                       // intro + each assignment
    page = ((millis() - tiltStart) / PAGE_MS) % totalPages;
  }

  // Redraw only when the screen changes or new data arrives
  if (page != lastPage || infoChanged) {
    display.clearDisplay();

    if (page == -1) {
      drawFace(64, 32, 28, false);
    } else if (!haveData) {
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 0);
      display.print("No data yet");
    } else if (page == 0) {
      drawIntro();
    } else {
      drawAssignment(page - 1);
    }

    display.display();
    lastPage = page;
    infoChanged = false;
  }

  // tone(buzzer, 1000); // Send 1KHz sound signal...
  // delay(1000);         // ...for 1 sec
  // noTone(buzzer);     // Stop sound...
  // delay(1000);         // ...for 1sec

  delay(50);
}