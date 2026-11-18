
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Preferences.h>
#include <pgmspace.h>
#include "Sprites.h"

// Wiring for ESP32-WROOM-32 30-pin and a 7-pin 240x240 ST7789 TFT.
// Screen SCL -> GPIO18; SDA -> GPIO23; RES -> GPIO22; DC -> GPIO21.
// Screen VCC -> 3V3; GND -> GND; BLK -> 3V3 only if supported by the module.
// Screen CS is absent and is assumed permanently selected by the module.
// Buttons use GPIO32, GPIO33 and GPIO27, each to GND.
// Buzzer on GPIO26 only if rated for direct 3.3V GPIO drive.
constexpr int TFT_SCLK = 18;
constexpr int TFT_MOSI = 23;
constexpr int TFT_DC   = 21;
constexpr int TFT_RST  = 22;
constexpr int TFT_CS   = -1;
constexpr int BTN_LEFT = 32;
constexpr int BTN_RIGHT = 33;
constexpr int BTN_OK = 27;
constexpr int BUZZER = 26;
constexpr int W = 240;
constexpr int H = 240;
constexpr int PET_Y = 112;
constexpr int PET_W = 90;
constexpr int PET_H = 90;

// RGB565 theme colors
constexpr uint16_t DAY_BG     = 0xFFDD; // soft warm cream
constexpr uint16_t NIGHT_BG   = 0x18CB; // deep midnight navy for sleep
constexpr uint16_t STAR_WHITE = 0xFFFF;
constexpr uint16_t STAR_GOLD  = 0xFFB5;
constexpr uint16_t PINK       = 0xF554;
constexpr uint16_t ROSE       = 0xDAAD;
constexpr uint16_t PLUM       = 0x698C;
constexpr uint16_t WHITE      = 0xFFFF;
constexpr uint16_t BLACK      = 0x0000;

Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST);
Preferences prefs;


enum GameState { HOME, MENU, SETTINGS, MINI_GAME };
enum PetAction { WALK, SLEEP, EAT, PLAY, SAD };
GameState screenState = HOME;
PetAction action = WALK;

struct Button {
  int pin;
  bool lastSample = HIGH;
  bool stable = HIGH;
  unsigned long changedAt = 0;
  bool poll() {
    bool sample = digitalRead(pin);
    if (sample != lastSample) { lastSample = sample; changedAt = millis(); }
    if (millis() - changedAt > 30 && sample != stable) {
      stable = sample;
      return stable == LOW;
    }
    return false;
  }
};
Button leftButton{BTN_LEFT}, rightButton{BTN_RIGHT}, okButton{BTN_OK};


int hunger = 65, happy = 75, energy = 100, poops = 0;
int petX = 76, moveDx = 3, frameNo = 0, menuIndex = 0, settingsIndex = 0;
int miniScore = 0, treatX = 90, treatY = 50;
unsigned long lastFrame = 0, lastMove = 0, lastStats = 0, lastPoop = 0;
unsigned long actionUntil = 0, miniStart = 0, lastFall = 0, lastSaved = 0;
bool sleeping = false, muted = false, forceRedraw = true;
bool homeNightDrawn = false;

void chirp(unsigned freq = 950, unsigned ms = 60) {
  if (!muted) tone(BUZZER, freq, ms);
}
int clampStat(int value) { return constrain(value, 0, 100); }
void savePet() {
  prefs.putInt("hunger", hunger);
  prefs.putInt("happy", happy);
  prefs.putInt("energy", energy);
  prefs.putInt("poops", poops);
}

// Draw a 16-bit sprite; 0x0000 is transparent.  Each pixel's RGB565
// color is preserved; this is NOT 1-bit/black-and-white rendering.
void drawSprite(int x, int y, int w, int h, const uint16_t* image, bool flip = false) {
  tft.startWrite();
  for (int row = 0; row < h; ++row) {
    for (int col = 0; col < w; ++col) {
      int from = flip ? (w - 1 - col) : col;
      uint16_t color = pgm_read_word(image + row * w + from);
      if (color != BLACK) tft.writePixel(x + col, y + row, color);
    }
  }
  tft.endWrite();
}


const uint16_t* spriteForAction() {
  switch (action) {
    case WALK:  return walk_frames[frameNo];
    case SLEEP: return sleep_frames[frameNo];
    case EAT:   return eat_frames[frameNo];
    case PLAY:  return play_frames[frameNo];
    case SAD:   return sad_frames[frameNo];
  }
  return neutral_original;
}

// The same sprite artwork is used in both modes. Only the background changes.
uint16_t homeBackground() { return sleeping ? NIGHT_BG : DAY_BG; }
uint16_t homeTextColor() { return sleeping ? STAR_GOLD : PLUM; }

struct Star { uint8_t x, y, radius; };
// Fixed positions keep the stars stationary as the sprite animates.
constexpr Star STARS[] = {
  {12, 64, 1}, {39, 91, 1}, {63, 59, 1}, {89, 82, 1},
  {113, 69, 1}, {145, 96, 1}, {172, 62, 1}, {230, 108, 1},
  {22, 132, 1}, {55, 155, 1}, {98, 119, 1}, {132, 147, 1},
  {183, 170, 1}, {222, 148, 1}, {11, 184, 1}, {70, 192, 1},
  {153, 188, 1}, {214, 197, 1}, {33, 221, 1}, {116, 215, 1},
  {190, 228, 1}
};

void drawStarsInBand(int fromY, int toY) {
  if (!sleeping) return;
  for (const auto& star : STARS) {
    if (star.y >= fromY && star.y < toY)
      tft.fillCircle(star.x, star.y, star.radius,
                     (star.x % 2 == 0) ? STAR_WHITE : STAR_GOLD);
  }
}

void drawNightSky() {
  if (!sleeping) return;
  drawStarsInBand(52, H);
  // A small crescent moon in the upper-right, below the status panel.
  tft.fillCircle(204, 79, 12, STAR_GOLD);
  tft.fillCircle(210, 73, 11, NIGHT_BG);
}

void drawStatus() {
  uint16_t bg = homeBackground();
  uint16_t fg = homeTextColor();
  tft.fillRect(0, 0, W, 52, bg);
  tft.setTextSize(2);
  tft.setTextColor(fg, bg);
  tft.setCursor(8, 5);
  tft.print("Hng:"); tft.print(hunger);
  tft.setCursor(126, 5);
  tft.print("Hap:"); tft.print(happy);
  tft.setCursor(8, 28); tft.print("Energy");
  tft.drawRoundRect(88, 28, 144, 13, 3, fg);
  tft.fillRect(91, 31, 138 * energy / 100, 7,
               sleeping ? STAR_GOLD : PINK);
}
void drawPoops() {
  for (int i = 0; i < poops && i < 3; ++i)
    drawSprite(14 + i * 30, 204, 20, 20, poop_frame);
}
void drawHomeBase() {
  tft.fillScreen(homeBackground());
  homeNightDrawn = sleeping;
  drawNightSky();
  drawStatus();
  tft.drawFastHLine(0, 202, W, homeTextColor());
  tft.setTextColor(homeTextColor(), homeBackground());
  tft.setTextSize(1);
  tft.setCursor(68, 230);
  tft.print("ACTION: MENU");
  drawPoops();
}
void drawHomePet() {
  // Redraw only the moving sprite strip; restore stars in that strip first.
  tft.fillRect(0, PET_Y, W, PET_H, homeBackground());
  drawStarsInBand(PET_Y, PET_Y + PET_H);
  drawSprite(petX, PET_Y, PET_W, PET_H, spriteForAction(), moveDx < 0);
}

const char* MENU_ITEMS[6] = {"Feed", "Play", "Sleep", "Clean", "Settings", "Exit"};
void drawMenu() {
  drawHomeBase();
  // The menu pauses the poodle's animation and shows her original neutral pose.
  drawSprite(petX, 140, PET_W, PET_H, neutral_original, false);
  tft.fillRoundRect(23, 35, 194, 168, 10, BLACK);
  tft.drawRoundRect(23, 35, 194, 168, 10, WHITE);
  tft.setTextSize(2);
  for (int i = 0; i < 6; ++i) {
    const int yy = 47 + i * 27;
    tft.setCursor(46, yy);
    tft.setTextColor(i == menuIndex ? PINK : WHITE, BLACK);
    tft.print(i == menuIndex ? ">" : " ");
    tft.setCursor(71, yy);
    tft.print(MENU_ITEMS[i]);
  }
}
void drawSettings() {
  tft.fillScreen(DAY_BG);
  tft.setTextSize(2); tft.setTextColor(PLUM, DAY_BG);
  tft.setCursor(33, 24); tft.print("Settings");
  tft.setCursor(30, 95);
  tft.print(settingsIndex == 0 ? ">" : " ");
  tft.print("Sound: "); tft.print(muted ? "OFF" : "ON");
  tft.setCursor(30, 132);
  tft.print(settingsIndex == 1 ? ">" : " "); tft.print("Back");
}
void startAction(PetAction next, unsigned durationMs = 0) {
  action = next;
  frameNo = 0;
  actionUntil = durationMs ? millis() + durationMs : 0;
  forceRedraw = true;
}
void updatePetNeeds() {
  if (millis() - lastStats < 30000) return;
  lastStats = millis();
  if (sleeping) {
    energy = clampStat(energy + 10);
    if (energy >= 100) { sleeping = false; startAction(WALK); }
  } else {
    hunger = clampStat(hunger - 1);
    happy  = clampStat(happy - (poops > 0 ? 2 : 1));
    energy = clampStat(energy - 1);
    if (energy <= 0) { sleeping = true; startAction(SLEEP); }
    else if (hunger <= 20 || happy <= 20) startAction(SAD);
    else if (action == SAD) startAction(WALK);
  }
  forceRedraw = true;
  savePet();
}
void enterHome() {
  screenState = HOME;
  drawHomeBase();
  forceRedraw = true;
}
void startMiniGame() {
  screenState = MINI_GAME;
  miniScore = 0;
  miniStart = millis();
  lastFall = millis();
  treatX = random(10, W - 26);
  treatY = 45;
  petX = 76;
  frameNo = 0;
  chirp(1150, 70);
  forceRedraw = true;
}
void renderMiniGame() {
  tft.fillScreen(DAY_BG);
  tft.setTextColor(PLUM, DAY_BG); tft.setTextSize(2);
  tft.setCursor(8, 7); tft.print("Score:"); tft.print(miniScore);
  tft.setCursor(146, 7); tft.print("T:");
  tft.print(max(0, 15 - int((millis() - miniStart) / 1000)));
  tft.drawFastHLine(0, 36, W, PLUM);
  drawSprite(treatX, treatY, 16, 16, treat_frame);
  drawSprite(petX, 130, PET_W, PET_H, walk_frames[frameNo], moveDx < 0);
  tft.setTextColor(PLUM, DAY_BG); tft.setTextSize(1);
  tft.setCursor(31, 230); tft.print("LEFT / RIGHT TO CATCH");
}
void finishMiniGame() {
  happy = clampStat(happy + 15 + (miniScore >= 5 ? 10 : 0));
  energy = clampStat(energy - 10);
  savePet();
  startAction(PLAY, 3000);
  enterHome();
}



// ========================================
// 3. game states and data

// game screen states
// pet animation states

// button struct for debouncing

// pet stats
// pet position and animation frame
// menu selection
// mini-game score and positions

// timers
// sleeping and sound settings


// ========================================
// 4. utility and storage functions

// chirp()              play buzzer tones
// clampStat()          keep stats from 0 to 100
// savePet()            save stats to esp32 memory


// ========================================
// 5. sprite rendering

// drawSprite()         draw rgb565 sprite
// spriteForAction()    select animation frame

// ========================================
// 6. backgrounds

// homeBackground()     choose cream or navy
// homeTextColor()      choose text color
// drawStarsInBand()    draw stars in screen area
// drawNightSky()       draw stars and moon



// ========================================
// 7. screen rendering

// drawStatus()         hunger happiness energy
// drawPoops()          poop icons
// drawHomeBase()       home background and interface
// drawHomePet()        animated poodle
// drawMenu()           activity selection
// drawSettings()       sound settings

// ========================================
// 8. pet behavior

// startAction()        start pet animation
// updatePetNeeds()     update hunger happiness energy
// enterHome()          return to home screen

// ========================================
// 9. strawberry mini-game

// startMiniGame()      initialize mini-game
// renderMiniGame()     draw falling strawberries
// finishMiniGame()     calculate rewards

// ========================================
// 10. setup

// setup()
// configure button pins
// initialize buzzer and display
// load saved pet stats
// draw initial home screen
// initialize timers




 
// ========================================
// 11. main game loop

// loop()
// poll button inputs
// handle menu selections
// handle settings
// update strawberry mini-game
// update pet needs
// generate poop every five minutes
// update animations
// update walking movement
// redraw screen when needed
