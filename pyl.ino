#include <MozziGuts.h>
#include <Oscil.h>
#include <tables/saw2048_int8.h>
#include <tables/square_no_alias_2048_int8.h>
#include <ADSR.h>
#include <RollingAverage.h>
#include "LedControl.h"
#define CHANGE 8
#define CONTROL_RATE 256

LedControl lc[] = {
  LedControl(10,12,11,4),
  LedControl(6, 8, 7, 4), // DIN, clk, cs, # of Display connected
  LedControl(3, 5, 4, 4),
  LedControl(14,16,15,4)
};

#define BTN_STOP  2
#define BTN_START 13

volatile bool running  = true;
static int num = 0;
volatile short selector =0;

Oscil<SAW2048_NUM_CELLS, AUDIO_RATE>              saw1(SAW2048_DATA);
Oscil<SAW2048_NUM_CELLS, AUDIO_RATE>              saw2(SAW2048_DATA);
Oscil<SQUARE_NO_ALIAS_2048_NUM_CELLS, AUDIO_RATE> sqOsc(SQUARE_NO_ALIAS_2048_DATA);
ADSR<CONTROL_RATE, AUDIO_RATE>                    env;
RollingAverage<int, 3>                            filter;

const short boardNotes[] = {
  185, 196, 220, 247,
  262, 311, 330, 370,
  370, 330, 311, 262,
  247, 220, 196, 185
};

float currentHz   = 340.0f;
float targetHz    = 330.0f;
bool  notePending = false;
short btnix = 0;
void setNote(float hz) {
  targetHz    = hz;
  notePending = true;
}
static byte board_off[]={0,1,2,3,4,7,8,11,12,13,14,15};
static byte dir = 1;

byte whammy[] = {
  B00010100,
  B01111110,
  B11011101,
  B11111111,
  B01100010,
  B00111100,
  B00000000,
  B00000000
};

const byte expand[16] PROGMEM = {
  B00000000, B00000011, B00001100, B00001111,
  B00110000, B00110011, B00111100, B00111111,
  B11000000, B11000011, B11001100, B11001111,
  B11110000, B11110011, B11111100, B11111111
};


byte digits[10][5] = {
  { B010, B101, B101, B101, B010 },//0
  { B010, B110, B010, B010, B111 },//1
  { B110, B001, B010, B100, B111 },//2
  { B110, B001, B010, B001, B110 },//3
  { B101, B101, B111, B001, B001 },//4
  { B111, B100, B111, B001, B110 },//5
  { B011, B100, B111, B101, B110 },//6
  { B111, B001, B010, B010, B010 },//7
  { B111, B101, B111, B101, B111 },//8
  { B111, B101, B111, B001, B110 } //9
};
short boardState[16] = {0};



int maxvalue = 1999;



void line(int off, int id, int row) {
  lc[off].setRow(id, row, B11111111);
}
void unline(int off, int id, int row) {
  lc[off].setRow(id, row, 0);
}

void displayNumber(int off, int id, int number, int start, bool big) {
  byte thousands  = (number / 1000);
  byte hundreds   = (number / 100) % 10;
  byte tensUnits  = number % 100;
  byte leftDigit  = tensUnits / 10;
  byte rightDigit = tensUnits % 10;

  for (int row = 0; row < 5; row++) {
    int combined =( 
      (digits[thousands][row] << 12) | 
      (digits[hundreds][row] << 8) |
      (digits[leftDigit][row] << 4) | 
      digits[rightDigit][row] );
    if (start < 0) {
      combined <<= -start;
    } else {
      combined >>= start;
    }

    byte toshow = combined & 0xff;
    if (big) {
      zoom(row + 1, toshow);
    } else {
      lc[off].setRow(id, row + 1, toshow);
    }
  }
}

void showWhammy(int off, int id) {
  for (byte i = 0; i < 8; i++) {
    lc[off].setRow(id, i, whammy[i]);
  }
}

void point(int x, int y, byte color) {
  Serial.print(x);
  Serial.print(",");
  Serial.println(y);
  lc[y>>3].setLed(3-(x>>3),y&0x7,x&0x7,color);
}

void zoom(int y, byte value) {
  byte idx = (y>>2)+1,
      row = (y&3)<<1,
      rhs = value>>4,
      lhs = value&15,
      expanded = pgm_read_byte(&expand[lhs]);
  
  lc[idx].setRow(1, row, expanded);
  lc[idx].setRow(1, row+1, expanded);

  expanded = pgm_read_byte(&expand[rhs]);
  
  lc[idx].setRow(2, row, expanded);
  lc[idx].setRow(2, row+1, expanded);
}

void setup() {
  for (int off = 0; off < 4; off++) {
    for (int ix = 0; ix < 4; ix++) {
      lc[off].shutdown(ix, false);
      lc[off].setIntensity(ix, 0);
      lc[off].clearDisplay(ix);
    }
  }
  Serial.begin(9600);

  pinMode(BTN_STOP, INPUT_PULLUP);
  pinMode(BTN_START, INPUT_PULLUP);
  
  startMozzi(CONTROL_RATE);

  env.setADLevels(190, 80);
  env.setTimes(1, 50, 20, 10);  // slow bloom, long decay, haunting tail

  saw1.setFreq(185.0f);
  saw2.setFreq(185.0f * 1.003f);   // very tight detune — less chorus, more shimmer
  sqOsc.setFreq(185.0f * 0.5f);    // sub octave square for weight
}

void updateControl() {
  // Portamento
  currentHz += 0.15f * (targetHz - currentHz);
  saw1.setFreq(currentHz);
  saw2.setFreq(currentHz * 1.04f);
  sqOsc.setFreq(currentHz * 0.99f);

  if (notePending) {
    env.noteOff();
    env.noteOn();
    notePending = false;
  }

  env.update();
}

int updateAudio() {
  int s1  = saw1.next();
  int s2  = saw2.next();
  int sq  = sqOsc.next();

  int raw = ((s1 + s2) / 2 * 7 + sq) >> 3; 
  long e    = env.next();
  int enved = (int)((raw * e) >> 8);

  return filter.next(enved);
}

void prepBig(int lvl) {
  for(byte ix = 0; ix < 2; ix++) {
    for (byte iy = 0; iy <2 ; iy++) {
      lc[ix+1].clearDisplay(iy+1);
      lc[ix+1].setIntensity(iy+1, lvl);
    }
  }
}

void loop() {
  audioHook();  
  static unsigned long lastStep = 0;
  static short cidx = 0;
  unsigned long now = millis();

  if (digitalRead(BTN_STOP) == HIGH) {
    prepBig(5);
    running = false;
    cidx = 0;
    dir = 1;
  }
  if (digitalRead(BTN_START) == HIGH) {
    prepBig(0);
    running = true;
    cidx = 0;
  }
  num++;
    
  if (!running) {
    if (now - lastStep >= 145) {
      lastStep = now;
      if (boardState[selector] > 0) {
        displayNumber(selector>>2,selector&0x3,boardState[selector],cidx,true);
        if(cidx == 11 || cidx == -4) {
          dir = !dir;
        }
        if (!dir) {
          cidx--;
        } else {
          cidx++;
        }
        setNote((float)boardNotes[num%16]*((num>>6)%4+1)/6);
      }
    }
  } else if (now - lastStep >= 155) {
    int tmp = cidx + CHANGE;
    lastStep = now;
    for (; cidx < tmp; cidx++) {
      byte _cidx = board_off[cidx % 12];
      byte board = (_cidx& B1100 ) >> 2;
      byte row = _cidx & B0011;
      lc[board].clearDisplay(row);
      if (random(6) == 0) {
        showWhammy(board,row);
        boardState[_cidx%16] = -1;
      } else {
        int val = random(maxvalue);
        boardState[_cidx%16] = val;
        displayNumber(board,row,val,0,false);
      }
    }
    unline(selector>>2,selector%4,7);
    lc[selector>>2].setIntensity(selector%4,0);
    selector = board_off[random(12)];
    lc[selector>>2].setIntensity(selector%4,5);
    line(selector>>2,selector%4,7);

    setNote((float)boardNotes[num%16]*((num>>6)%5+2)/6 + boardState[selector]/10);
  }
  return;
  
}