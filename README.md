# Poodle Tamagotchi Pet Project

The program is a customized virtual pet aka Tamagotchi. 

I wanted to create a fun little game inspired by my two beloved poodles, whom I miss so much while I’m away at college. This project also gave me the opportunity to strengthen my skills in embedded systems, C++ programming, graphic design, SPI display interfacing, and hardware-software integration, while bringing a personal idea to life.

-------------------------------------------------------------------------------------------

The project targets `esp32dev` (ESP32-WROOM-32 dev board).

## Hardware 

- ST7789 240x240 TFT: SCL/CLK to GPIO18, SDA/MOSI to GPIO23, RES to GPIO22, DC to GPIO21; VCC to 3V3, GND to GND. BLK to 3V3 **only if supported by that module**.
- Left button GPIO32-GND, right GPIO33-GND, action GPIO27-GND; uses internal pullups.
- Low-current piezo buzzer GPIO26-GND. Higher-current buzzers need driver circuitry.
- Powered via usb

## Files

- `platformio.ini`: board/platform/framework/library setup
- `src/main.cpp`: Arduino `setup()` / `loop()` game logic 
- `src/Sprites.cpp`: RGB565 pet image data (original design wass drawn by hand, then converted to pixels)
- `include/Sprites.h`: sprite declarations

Browser display preview - made by Claude AI :)  ->  
 
This preserves the prior Arduino code and sprites, rearranged for PlatformIO
----------------------------------------------------------------------------------------

How the game works:

The Tamagotchi features a pixel-art poodle with different animations, interactive activities, and three main needs: hunger, happiness, and energy

Home: The poodle walks around the screen while its hunger, happiness, and energy levels are displayed.

Feed: Feed the poodle to increase its hunger stat, with a cute eating animation

Play: Play a 15-second mini-game where you move the poodle left and right to catch falling strawberries and earn happiness points

Sleep: Put the poodle to sleep to restore its energy

Clean: Clean up after the poodle to improve its happiness

Emotions: The poodle displays different expressions and animations depending on its needs, such as becoming sad when hungry or unhappy.

Settings: Toggle sound effects on or off.

The game also saves pet stats, so its progress is preserved when the ESP32 restarts!