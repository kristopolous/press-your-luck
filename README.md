# Whammy Raffle

A very complicated way to select a raffle winner, inspired by the TV game show *Press Your Luck*.

## What it does

Numbers scroll chaotically across 16 8x8 LED matrix displays while synthesized audio plays. Press STOP and the machine freezes on a winner — displayed large across the center screens with a scrolling animation. Press START to run it again.

Hit a Whammy and you lose everything.

## Hardware

- Arduino Nano
- 16x MAX7219 8x8 LED matrix displays (4 chained controllers, 4 displays each)
- 2x momentary push buttons (START / STOP)
- Audio output on Mozzi pin 9

## Wiring

| Component | Pin |
|---|---|
| LED array 1 (DIN, CLK, CS) | 10, 12, 11 |
| LED array 2 (DIN, CLK, CS) | 6, 8, 7 |
| LED array 3 (DIN, CLK, CS) | 3, 5, 4 |
| LED array 4 (DIN, CLK, CS) | 14, 16, 15 |
| BTN_START | 13 |
| BTN_STOP | 2 |
| Audio out | 9 (Mozzi) |

## Dependencies

- [Mozzi](https://github.com/sensorium/Mozzi) — audio synthesis
- [LedControl](https://github.com/wayoda/LedControl) — MAX7219 LED matrix driver

## Building

Open `pyl.ino` in the Arduino IDE, install the dependencies via Library Manager, and upload to an Arduino Nano.

## How to use

1. Power on — numbers start scrolling immediately
2. Press **STOP** to freeze and reveal the winner
3. Press **START** to run again
