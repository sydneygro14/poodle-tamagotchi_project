
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Preferences.h>
#include <pgmspace.h>
#include "Sprites.h"

// ========================================
// 1. hardware configuration

// display pins
// button pins
// buzzer pin
// screen and sprite dimensions



// ========================================
// 2. colors and display objects

// cream daytime background
// navy nighttime background
// stars and interface colors

// create tft display object
// create preferences object



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
