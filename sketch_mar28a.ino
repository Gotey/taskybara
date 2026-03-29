#include <Arduino_GFX_Library.h>
#include <PCA95x5.h>

#define GFX_BL 45

// =========================
// Display setup
// =========================
Arduino_DataBus *bus = new Arduino_SWSPI(
    GFX_NOT_DEFINED /* DC */, PCA95x5::Port::P04 /* CS */,
    41 /* SCK */, 48 /* MOSI */, GFX_NOT_DEFINED /* MISO */);

Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
    18 /* DE */, 17 /* VSYNC */, 16 /* HSYNC */, 21 /* PCLK */,
    4 /* R0 */, 3 /* R1 */, 2 /* R2 */, 1 /* R3 */, 0 /* R4 */,
    10 /* G0 */, 9 /* G1 */, 8 /* G2 */, 7 /* G3 */, 6 /* G4 */, 5 /* G5 */,
    15 /* B0 */, 14 /* B1 */, 13 /* B2 */, 12 /* B3 */, 11 /* B4 */,
    1 /* hsync_polarity */, 10 /* hsync_front_porch */, 8 /* hsync_pulse_width */, 50 /* hsync_back_porch */,
    1 /* vsync_polarity */, 10 /* vsync_front_porch */, 8 /* vsync_pulse_width */, 20 /* vsync_back_porch */);

// Leave rotation at the value that already worked for you
Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
    480 /* width */, 480 /* height */, rgbpanel, 0 /* rotation */, true /* auto_flush */,
    bus, GFX_NOT_DEFINED /* RST */, st7701_type1_init_operations, sizeof(st7701_type1_init_operations));

// =========================
// Colors
// =========================
const uint16_t COLOR_BG       = 0x0000;
const uint16_t COLOR_WHITE    = 0xFFFF;
const uint16_t COLOR_GREEN    = 0x07E0;
const uint16_t COLOR_RED      = 0xF800;
const uint16_t COLOR_BLUE     = 0x001F;
const uint16_t COLOR_YELLOW   = 0xFFE0;
const uint16_t COLOR_CYAN     = 0x07FF;
const uint16_t COLOR_MAGENTA  = 0xF81F;
const uint16_t COLOR_ORANGE   = 0xFD20;
const uint16_t COLOR_GRAY     = 0x8410;

// =========================
// App state
// =========================
enum ScreenState {
  HOME_SCREEN,
  TASK_SCREEN,
  CARE_SCREEN
};

enum PetMood {
  PET_IDLE,
  PET_HAPPY,
  PET_SAD,
  PET_SLEEPY
};

ScreenState currentScreen = HOME_SCREEN;
PetMood currentMood = PET_HAPPY;

int health = 82;
int happiness = 76;
int hunger = 58;

int streak = 3;
int coins = 12;

// Task timer
int taskMinutes = 25;
int taskSeconds = 0;
bool taskRunning = false;
unsigned long lastCountdownTick = 0;

// =========================
// Helpers
// =========================
const char* getPetFace(PetMood mood) {
  switch (mood) {
    case PET_IDLE:   return "(-_-)";
    case PET_HAPPY:  return "(^_^)";
    case PET_SAD:    return "(T_T)";
    case PET_SLEEPY: return "(-.-)z";
    default:         return "(^_^)";
  }
}

const char* getPetStatus(PetMood mood) {
  switch (mood) {
    case PET_IDLE:   return "Idle";
    case PET_HAPPY:  return "Happy";
    case PET_SAD:    return "Sad";
    case PET_SLEEPY: return "Sleepy";
    default:         return "Unknown";
  }
}

uint16_t getMoodColor(PetMood mood) {
  switch (mood) {
    case PET_IDLE:   return COLOR_CYAN;
    case PET_HAPPY:  return COLOR_GREEN;
    case PET_SAD:    return COLOR_RED;
    case PET_SLEEPY: return COLOR_YELLOW;
    default:         return COLOR_WHITE;
  }
}

void clampStats() {
  if (health < 0) health = 0;
  if (health > 100) health = 100;

  if (happiness < 0) happiness = 0;
  if (happiness > 100) happiness = 100;

  if (hunger < 0) hunger = 0;
  if (hunger > 100) hunger = 100;
}

void clearScreen() {
  gfx->fillScreen(COLOR_BG);
}

void drawCenteredText(const char* text, int y, uint16_t color, int textSize) {
  gfx->setTextSize(textSize);
  gfx->setTextColor(color);

  int16_t x1, y1;
  uint16_t w, h;
  gfx->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

  int x = (gfx->width() - w) / 2;
  gfx->setCursor(x, y);
  gfx->print(text);
}

void drawButton(int x, int y, int w, int h, uint16_t fillColor, const char* label) {
  gfx->fillRoundRect(x, y, w, h, 12, fillColor);
  gfx->drawRoundRect(x, y, w, h, 12, COLOR_WHITE);

  gfx->setTextSize(2);
  gfx->setTextColor(COLOR_WHITE);

  int16_t x1, y1;
  uint16_t tw, th;
  gfx->getTextBounds(label, 0, 0, &x1, &y1, &tw, &th);

  int tx = x + (w - tw) / 2;
  int ty = y + (h / 2) + (th / 2) - 14;

  gfx->setCursor(tx, ty);
  gfx->print(label);
}

void drawBar(int x, int y, int w, int h, int value, uint16_t color, const char* label) {
  value = max(0, min(100, value));

  gfx->setTextSize(1);
  gfx->setTextColor(COLOR_WHITE);
  gfx->setCursor(x, y - 8);
  gfx->print(label);

  gfx->drawRoundRect(x, y, w, h, 6, COLOR_WHITE);

  int fillW = (w - 4) * value / 100;
  gfx->fillRoundRect(x + 2, y + 2, fillW, h - 4, 4, color);
}

void drawStatTextLeft(const char* text, int x, int y, uint16_t color, int size) {
  gfx->setTextSize(size);
  gfx->setTextColor(color);
  gfx->setCursor(x, y);
  gfx->print(text);
}

// =========================
// Screens
// =========================
void drawHomeScreen() {
  clearScreen();

  drawCenteredText("Taskybara", 28, COLOR_WHITE, 3);
  drawCenteredText(getPetFace(currentMood), 120, getMoodColor(currentMood), 5);
  drawCenteredText(getPetStatus(currentMood), 190, COLOR_WHITE, 2);

  char buffer[32];
  snprintf(buffer, sizeof(buffer), "Streak: %d", streak);
  drawStatTextLeft(buffer, 40, 235, COLOR_GREEN, 2);

  snprintf(buffer, sizeof(buffer), "Coins: %d", coins);
  drawStatTextLeft(buffer, 270, 235, COLOR_YELLOW, 2);

  drawBar(90, 275, 300, 18, health, COLOR_GREEN, "Health");
  drawBar(90, 305, 300, 18, happiness, COLOR_CYAN, "Happiness");
  drawBar(90, 335, 300, 18, hunger, COLOR_ORANGE, "Hunger");

  drawButton(90, 370, 300, 30, COLOR_BLUE, "Start Task");
  drawButton(90, 410, 300, 30, COLOR_RED, "Care");
  drawButton(90, 450, 300, 25, COLOR_MAGENTA, "Music");
}

void drawTaskScreen() {
  clearScreen();

  drawCenteredText("Focus Mode", 28, COLOR_WHITE, 3);
  drawCenteredText(getPetFace(currentMood), 110, getMoodColor(currentMood), 5);

  char timerBuffer[16];
  snprintf(timerBuffer, sizeof(timerBuffer), "%02d:%02d", taskMinutes, taskSeconds);
  drawCenteredText(timerBuffer, 220, COLOR_GREEN, 4);

  if (taskRunning) {
    drawCenteredText("Stay focused", 285, COLOR_WHITE, 2);
  } else {
    drawCenteredText("Task paused", 285, COLOR_YELLOW, 2);
  }

  drawButton(70, 355, 150, 50, COLOR_GREEN, "Working");
  drawButton(260, 355, 150, 50, COLOR_RED, "Distracted");
  drawButton(140, 420, 200, 40, COLOR_BLUE, "Home");
}

void drawCareScreen() {
  clearScreen();

  drawCenteredText("Take Care", 28, COLOR_WHITE, 3);
  drawCenteredText(getPetFace(currentMood), 110, getMoodColor(currentMood), 5);
  drawCenteredText("Feed, play, clean", 190, COLOR_WHITE, 2);

  drawBar(90, 235, 300, 18, health, COLOR_GREEN, "Health");
  drawBar(90, 265, 300, 18, happiness, COLOR_CYAN, "Happiness");
  drawBar(90, 295, 300, 18, hunger, COLOR_ORANGE, "Hunger");

  drawButton(70, 345, 340, 34, COLOR_ORANGE, "Feed");
  drawButton(70, 387, 340, 34, COLOR_BLUE, "Play");
  drawButton(70, 429, 340, 34, COLOR_MAGENTA, "Clean");
}

void drawCurrentScreen() {
  switch (currentScreen) {
    case HOME_SCREEN:
      drawHomeScreen();
      break;
    case TASK_SCREEN:
      drawTaskScreen();
      break;
    case CARE_SCREEN:
      drawCareScreen();
      break;
  }
}

// =========================
// State updates
// =========================
void switchToHome() {
  currentScreen = HOME_SCREEN;
  drawCurrentScreen();
}

void switchToTask() {
  currentScreen = TASK_SCREEN;
  taskRunning = true;
  lastCountdownTick = millis();
  drawCurrentScreen();
}

void switchToCare() {
  currentScreen = CARE_SCREEN;
  drawCurrentScreen();
}

void cycleMood() {
  currentMood = static_cast<PetMood>((currentMood + 1) % 4);
  drawCurrentScreen();
}

void resetState() {
  currentScreen = HOME_SCREEN;
  currentMood = PET_HAPPY;
  health = 82;
  happiness = 76;
  hunger = 58;
  streak = 3;
  coins = 12;
  taskMinutes = 25;
  taskSeconds = 0;
  taskRunning = false;
  drawCurrentScreen();
}

void feedPet() {
  hunger += 15;
  happiness += 3;
  currentMood = PET_HAPPY;
  clampStats();
  drawCurrentScreen();
}

void playWithPet() {
  happiness += 12;
  hunger -= 4;
  currentMood = PET_HAPPY;
  clampStats();
  drawCurrentScreen();
}

void cleanPet() {
  health += 10;
  currentMood = PET_IDLE;
  clampStats();
  drawCurrentScreen();
}

void punishDistraction() {
  health -= 10;
  happiness -= 12;
  currentMood = PET_SAD;
  clampStats();
  drawCurrentScreen();
}

void rewardTaskComplete() {
  streak += 1;
  coins += 5;
  happiness += 8;
  currentMood = PET_HAPPY;
  clampStats();
}

// =========================
// Serial input
// =========================
void handleSerialInput() {
  if (!Serial.available()) return;

  char cmd = Serial.read();

  switch (cmd) {
    case 'h':
    case 'H':
      switchToHome();
      Serial.println("HOME");
      break;

    case 't':
    case 'T':
      switchToTask();
      Serial.println("TASK");
      break;

    case 'c':
    case 'C':
      switchToCare();
      Serial.println("CARE");
      break;

    case 'm':
    case 'M':
      cycleMood();
      Serial.println("MOOD");
      break;

    case 'r':
    case 'R':
      resetState();
      Serial.println("RESET");
      break;

    case 'f':
    case 'F':
      feedPet();
      Serial.println("FEED");
      break;

    case 'p':
    case 'P':
      playWithPet();
      Serial.println("PLAY");
      break;

    case 'l':
    case 'L':
      cleanPet();
      Serial.println("CLEAN");
      break;

    case 'd':
    case 'D':
      punishDistraction();
      Serial.println("DISTRACTION");
      break;
  }
}

// =========================
// Task countdown
// =========================
void updateTaskCountdown() {
  if (currentScreen != TASK_SCREEN || !taskRunning) return;

  unsigned long now = millis();
  if (now - lastCountdownTick >= 1000) {
    lastCountdownTick = now;

    if (taskSeconds == 0) {
      if (taskMinutes > 0) {
        taskMinutes--;
        taskSeconds = 59;
      } else {
        taskRunning = false;
        rewardTaskComplete();
        switchToHome();
        return;
      }
    } else {
      taskSeconds--;
    }

    drawTaskScreen();
  }
}

// =========================
// Setup / loop
// =========================
void setup(void) {
  Serial.begin(115200);
  Serial.println("Taskybara starting...");
  Serial.println("Commands:");
  Serial.println("h = home");
  Serial.println("t = task");
  Serial.println("c = care");
  Serial.println("m = cycle mood");
  Serial.println("r = reset");
  Serial.println("f = feed");
  Serial.println("p = play");
  Serial.println("l = clean");
  Serial.println("d = distraction");

#ifdef GFX_EXTRA_PRE_INIT
  GFX_EXTRA_PRE_INIT();
#endif

  if (!gfx->begin()) {
    Serial.println("gfx->begin() failed!");
    return;
  }

  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH);

  drawCurrentScreen();
  lastCountdownTick = millis();
}

void loop() {
  handleSerialInput();
  updateTaskCountdown();
}