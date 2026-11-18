
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Preferences.h>
#include <pgmspace.h>
#include "Sprites.h"


// 1 hardware configuration

constexpr int TFT_SCLK = 18;
constexpr int TFT_MOSI = 23;
constexpr int TFT_DC   = 21;
constexpr int TFT_RST  = 22;
constexpr int TFT_CS   = -1;

constexpr int BTN_LEFT = 32;
constexpr int BTN_RIGHT = 33;
constexpr int BTN_OK = 27;
constexpr int BUZZER = 26;

// screen dimensions in pixels
constexpr int W = 240;
constexpr int H = 240;

// pet sprite dimensions and vertical position
constexpr int PET_Y = 112;
constexpr int PET_W = 90;
constexpr int PET_H = 90;


// ========================================
// 2 colors and display objects


// cream background when awake
// navy background when sleeping
constexpr uint16_t DAY_BG     = 0xFFDD;
constexpr uint16_t NIGHT_BG   = 0x18CB;

// colors used for stars and the moon
constexpr uint16_t STAR_WHITE = 0xFFFF;
constexpr uint16_t STAR_GOLD  = 0xFFB5;

// colors used for menus text and other graphics
constexpr uint16_t PINK       = 0xF554;
constexpr uint16_t ROSE       = 0xDAAD;
constexpr uint16_t PLUM       = 0x698C;
constexpr uint16_t WHITE      = 0xFFFF;
constexpr uint16_t BLACK      = 0x0000;

// creates the display object using the selected pins
Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST);

// stores pet data in esp32 nonvolatile memory
Preferences prefs;


// ========================================
// 3 game states and data

// tracks which screen is currently open
enum GameState {
  HOME,
  MENU,
  SETTINGS,
  MINI_GAME
};

// tracks which animation the poodle should play
enum PetAction {
  WALK,
  SLEEP,
  EAT,
  PLAY,
  SAD
};

// game starts on the home screen with the poodle walking
GameState screenState = HOME;
PetAction action = WALK;

// stores the state of each button
// checks for a press and filters out button bouncing
// waits 30 ms for the button reading to become stable
// returns true only when a new button press is detected

struct Button {
  int pin;
  bool lastSample = HIGH;
  bool stable = HIGH;
  unsigned long changedAt = 0;

  bool poll() {
    bool sample = digitalRead(pin);

    if (sample != lastSample) {
      lastSample = sample;
      changedAt = millis();
    }

    if (millis() - changedAt > 30 && sample != stable) {
      stable = sample;
      return stable == LOW;
    }

    return false;
  }
};

// creates a button object for each physical button
Button leftButton{BTN_LEFT};
Button rightButton{BTN_RIGHT};
Button okButton{BTN_OK};


// hunger happiness and energy range from 0 to 100
// poops tracks how many poop icons are on the screen

int hunger = 65;
int happy = 75;
int energy = 100;
int poops = 0;


// petX controls the horizontal position of the poodle
// moveDx controls walking speed and direction
// frameNo selects the current animation frame
// menu indexes track which option is highlighted

int petX = 76;
int moveDx = 3;
int frameNo = 0;
int menuIndex = 0;
int settingsIndex = 0;


// mini game data
// tracks the score and falling strawberry position
int miniScore = 0;
int treatX = 90;
int treatY = 50;


// millis is used so the game can keep running without delays
// each timer tracks when something last happened
unsigned long lastFrame = 0;
unsigned long lastMove = 0;
unsigned long lastStats = 0;
unsigned long lastPoop = 0;

unsigned long actionUntil = 0;
unsigned long miniStart = 0;
unsigned long lastFall = 0;
unsigned long lastSaved = 0;


bool sleeping = false;
bool muted = false;
bool forceRedraw = true;
bool homeNightDrawn = false;


// ========================================
// 4 utility and storage functions


// plays a short sound through the buzzer
// frequency controls the pitch
// duration controls how long the sound lasts
// no sound is played if muted is enabled

void chirp(unsigned freq = 950, unsigned ms = 60) {
  if (!muted) {
    tone(BUZZER, freq, ms);
  }
}

// prevents pet stats from going below 0 or above 100:
int clampStat(int value) {
  return constrain(value, 0, 100);
}

// saves the current pet stats to esp32 memory
// allows stats to be restored after restarting the device

void savePet() {
  prefs.putInt("hunger", hunger);
  prefs.putInt("happy", happy);
  prefs.putInt("energy", energy);
  prefs.putInt("poops", poops);
}


// ========================================
// 5 sprite rendering


void drawSprite(
  int x,
  int y,
  int w,
  int h,
  const uint16_t* image,
  bool flip = false
) {

  tft.startWrite();

  // go through every row and column of the sprite
  for (int row = 0; row < h; ++row) {
    for (int col = 0; col < w; ++col) {

      // reverse the column order when flipping the sprite
      int from = flip ? (w - 1 - col) : col;

      // read the color of the current pixel
      uint16_t color = pgm_read_word(image + row * w + from);

      // only draw pixels that are not transparent
      if (color != BLACK) {
        tft.writePixel(x + col, y + row, color);
      }
    }
  }

  tft.endWrite();
}


// spriteForAction

// chooses which sprite animation to display
// each action has four animation frames
// frameNo determines which frame is currently shown
// uses the neutral sprite if no action matches

const uint16_t* spriteForAction() {
  switch (action) {
    case WALK:
      return walk_frames[frameNo];

    case SLEEP:
      return sleep_frames[frameNo];

    case EAT:
      return eat_frames[frameNo];

    case PLAY:
      return play_frames[frameNo];

    case SAD:
      return sad_frames[frameNo];
  }

  return neutral_original;
}


// ========================================
// 6 backgrounds


uint16_t homeBackground() {
  return sleeping ? NIGHT_BG : DAY_BG;
}


// homeTextColor

// uses dark text during the day
// uses light text at night so it is easier to read

uint16_t homeTextColor() {
  return sleeping ? STAR_GOLD : PLUM;
}

// stores the position and size of each star
// fixed positions keep the stars from moving during animations

struct Star {
  uint8_t x;
  uint8_t y;
  uint8_t radius;
};

constexpr Star STARS[] = {
  {12, 64, 1}, {39, 91, 1}, {63, 59, 1}, {89, 82, 1},
  {113, 69, 1}, {145, 96, 1}, {172, 62, 1}, {230, 108, 1},
  {22, 132, 1}, {55, 155, 1}, {98, 119, 1}, {132, 147, 1},
  {183, 170, 1}, {222, 148, 1}, {11, 184, 1}, {70, 192, 1},
  {153, 188, 1}, {214, 197, 1}, {33, 221, 1}, {116, 215, 1},
  {190, 228, 1}
};


// drawStarsInBand

// draws stars only when the poodle is sleeping
// only draws stars inside the selected vertical area
// this helps restore stars when the poodle animation updates
// stars alternate between white and light yellow

void drawStarsInBand(int fromY, int toY) {
  if (!sleeping) return;

  for (const auto& star : STARS) {
    if (star.y >= fromY && star.y < toY) {
      tft.fillCircle(
        star.x,
        star.y,
        star.radius,
        (star.x % 2 == 0) ? STAR_WHITE : STAR_GOLD
      );
    }
  }
}


// drawNightSky

// draws the nighttime details when the poodle sleeps
// adds stars below the status bar
// draws two overlapping circles to make a crescent moon

void drawNightSky() {
  if (!sleeping) return;

  drawStarsInBand(52, H);

  // draw the moon in the upper right corner
  tft.fillCircle(204, 79, 12, STAR_GOLD);

  // cover part of the circle to create the crescent shape
  tft.fillCircle(210, 73, 11, NIGHT_BG);
}


// ========================================
// 7 screen rendering


void drawStatus() {
  uint16_t bg = homeBackground();
  uint16_t fg = homeTextColor();

  // clear the status area before drawing updated values
  tft.fillRect(0, 0, W, 52, bg);

  tft.setTextSize(2);
  tft.setTextColor(fg, bg);

  // display hunger
  tft.setCursor(8, 5);
  tft.print("Hng:");
  tft.print(hunger);

  // display happiness
  tft.setCursor(126, 5);
  tft.print("Hap:");
  tft.print(happy);

  // display energy label and bar
  tft.setCursor(8, 28);
  tft.print("Energy");

  tft.drawRoundRect(88, 28, 144, 13, 3, fg);

  // fill the bar based on the percentage of energy remaining
  tft.fillRect(
    91,
    31,
    138 * energy / 100,
    7,
    sleeping ? STAR_GOLD : PINK
  );
}


// drawPoops

// draws one poop sprite for each poop counted
// limits the number of visible poop icons to three
// places the icons along the bottom of the screen

void drawPoops() {
  for (int i = 0; i < poops && i < 3; ++i) {
    drawSprite(14 + i * 30, 204, 20, 20, poop_frame);
  }
}


// drawHomeBase

// draws the main home screen without the moving poodle
// sets the background for daytime or nighttime
// draws the stars status bar menu label and poop icons
// stores which background was last drawn

void drawHomeBase() {
  tft.fillScreen(homeBackground());

  homeNightDrawn = sleeping;

  drawNightSky();
  drawStatus();

  // draw a line above the bottom menu area
  tft.drawFastHLine(0, 202, W, homeTextColor());

  // show how to open the menu
  tft.setTextColor(homeTextColor(), homeBackground());
  tft.setTextSize(1);
  tft.setCursor(68, 230);
  tft.print("ACTION: MENU");

  drawPoops();
}


// drawHomePet

// updates only the area where the poodle moves
// clears the previous frame before drawing the next one
// redraws any stars that were covered by the old frame
// flips the poodle when she walks to the left

void drawHomePet() {

  // clear the area used by the pet animation
  tft.fillRect(0, PET_Y, W, PET_H, homeBackground());

  // restore the stars behind the poodle when sleeping
  drawStarsInBand(PET_Y, PET_Y + PET_H);

  // draw the current animation frame
  drawSprite(
    petX,
    PET_Y,
    PET_W,
    PET_H,
    spriteForAction(),
    moveDx < 0
  );
}


// menu items

// stores the available actions in the main menu
const char* MENU_ITEMS[6] = {
  "Feed",
  "Play",
  "Sleep",
  "Clean",
  "Settings",
  "Exit"
};


// drawMenu

// draws the activity menu over the home screen
// shows the original neutral poodle sprite
// highlights the currently selected menu option
// menuIndex is changed by the left and right buttons

void drawMenu() {
  drawHomeBase();

  // show the poodle in her neutral pose
  drawSprite(
    petX,
    140,
    PET_W,
    PET_H,
    neutral_original,
    false
  );

  // draw the menu background and border
  tft.fillRoundRect(23, 35, 194, 168, 10, BLACK);
  tft.drawRoundRect(23, 35, 194, 168, 10, WHITE);

  tft.setTextSize(2);

  // draw each menu option
  for (int i = 0; i < 6; ++i) {
    const int yy = 47 + i * 27;

    tft.setCursor(46, yy);

    // highlight the currently selected option in pink
    tft.setTextColor(
      i == menuIndex ? PINK : WHITE,
      BLACK
    );

    tft.print(i == menuIndex ? ">" : " ");

    tft.setCursor(71, yy);
    tft.print(MENU_ITEMS[i]);
  }
}


// drawSettings

// draws the settings screen with a cream background
// allows the sound setting to be turned on or off
// shows an arrow next to the selected option

void drawSettings() {
  tft.fillScreen(DAY_BG);

  tft.setTextSize(2);
  tft.setTextColor(PLUM, DAY_BG);

  // settings title
  tft.setCursor(33, 24);
  tft.print("Settings");

  // sound option
  tft.setCursor(30, 95);
  tft.print(settingsIndex == 0 ? ">" : " ");
  tft.print("Sound: ");
  tft.print(muted ? "OFF" : "ON");

  // return to the menu
  tft.setCursor(30, 132);
  tft.print(settingsIndex == 1 ? ">" : " ");
  tft.print("Back");
}


// ========================================
// 8 pet behavior


void startAction(PetAction next, unsigned durationMs = 0) {
  action = next;
  frameNo = 0;

  actionUntil = durationMs ? millis() + durationMs : 0;

  forceRedraw = true;
}


// updatePetNeeds

// updates the pet stats every 30 seconds
// sleeping restores energy instead of lowering needs
// the poodle wakes up when energy reaches 100
// while awake hunger happiness and energy slowly decrease
// happiness decreases faster when poop is present
// the poodle automatically sleeps when energy reaches zero
// low hunger or happiness causes the sad animation
// updated stats are saved after each change

void updatePetNeeds() {

  // only update stats once every 30 seconds
  if (millis() - lastStats < 30000) return;

  lastStats = millis();

  if (sleeping) {

    // restore energy while the poodle is asleep
    energy = clampStat(energy + 10);

    // wake the poodle when energy is full
    if (energy >= 100) {
      sleeping = false;
      startAction(WALK);
    }

  } else {

    // lower the needs while the poodle is awake
    hunger = clampStat(hunger - 1);
    happy = clampStat(happy - (poops > 0 ? 2 : 1));
    energy = clampStat(energy - 1);

    // start sleeping when energy runs out
    if (energy <= 0) {
      sleeping = true;
      startAction(SLEEP);
    }

    // show the sad animation when hunger or happiness is low
    else if (hunger <= 20 || happy <= 20) {
      startAction(SAD);
    }

    // return to walking when the poodle is no longer sad
    else if (action == SAD) {
      startAction(WALK);
    }
  }

  // update the display and save the new stats
  forceRedraw = true;
  savePet();
}


// enterHome

// switches back to the home screen
// redraws the background and requests a pet animation update

void enterHome() {
  screenState = HOME;
  drawHomeBase();
  forceRedraw = true;
}


// ========================================
// 9 strawberry mini game


void startMiniGame() {
  screenState = MINI_GAME;

  miniScore = 0;
  miniStart = millis();
  lastFall = millis();

  // place the first strawberry near the top
  treatX = random(10, W - 26);
  treatY = 45;

  // reset the poodle position and animation
  petX = 76;
  frameNo = 0;

  chirp(1150, 70);
  forceRedraw = true;
}


// renderMiniGame

// draws the strawberry catching mini game
// shows the score and remaining time
// draws the falling strawberry and moving poodle
// the poodle uses its walking animation during the game

void renderMiniGame() {

  // clear the screen using the cream background
  tft.fillScreen(DAY_BG);

  tft.setTextColor(PLUM, DAY_BG);
  tft.setTextSize(2);

  // display the number of strawberries caught
  tft.setCursor(8, 7);
  tft.print("Score:");
  tft.print(miniScore);

  // display the remaining time out of 15 seconds
  tft.setCursor(146, 7);
  tft.print("T:");
  tft.print(max(0, 15 - int((millis() - miniStart) / 1000)));

  // separate the score area from the game
  tft.drawFastHLine(0, 36, W, PLUM);

  // draw the falling strawberry
  drawSprite(treatX, treatY, 16, 16, treat_frame);

  // draw the poodle at its current position
  drawSprite(
    petX,
    130,
    PET_W,
    PET_H,
    walk_frames[frameNo],
    moveDx < 0
  );

  // show the player controls
  tft.setTextColor(PLUM, DAY_BG);
  tft.setTextSize(1);
  tft.setCursor(31, 230);
  tft.print("LEFT / RIGHT TO CATCH");
}


// finishMiniGame

// calculates the happiness reward after the game
// gives 15 happiness for playing
// adds another 10 happiness if at least five strawberries were caught
// lowers energy by 10 after playing
// saves the updated stats
// plays the happy animation for three seconds
// returns to the home screen

void finishMiniGame() {

  // calculate happiness based on the score
  happy = clampStat(
    happy + 15 + (miniScore >= 5 ? 10 : 0)
  );

  // playing uses energy
  energy = clampStat(energy - 10);

  // save the new stats
  savePet();

  // show the play animation before returning to walking
  startAction(PLAY, 3000);

  // return to the home screen
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
