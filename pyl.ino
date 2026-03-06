#include <MozziGuts.h>
#include <Oscil.h>
#include <Mozzi.h> // this makes everything work
#include <tables/saw2048_int8.h>
#include <tables/square_no_alias_2048_int8.h>
#include <ADSR.h>
#include <RollingAverage.h>
#include "LedControl.h"
#include <EventDelay.h>
#define CHANGE 3
#define CONTROL_RATE 128
#define BTN_STOP  2
#define BTN_START 13

LedControl lc[] = {
  LedControl(10, 12, 11, 4),
  LedControl(7,  12, 8, 4), // DIN, clk, cs, # of Display connected
  LedControl(5,  12, 6, 4),
  LedControl(3,  12, 4, 4)
};

volatile bool running  = true;
static int num = 0;
volatile short selector =0;

Oscil<SAW2048_NUM_CELLS, AUDIO_RATE>              saw1(SAW2048_DATA);
Oscil<SAW2048_NUM_CELLS, AUDIO_RATE>              saw2(SAW2048_DATA);
Oscil<SQUARE_NO_ALIAS_2048_NUM_CELLS, AUDIO_RATE> sqOsc(SQUARE_NO_ALIAS_2048_DATA);
ADSR<CONTROL_RATE, AUDIO_RATE>                    env;
//RollingAverage<int, 3>                            filter;

const short boardNotes[] = {
  261, 392, 311, 311,
  130, 311, 311, 392,
  130, 330, 311, 262,
  247, 220, 220, 220
};

float currentHz   = 330.0f;
float targetHz    = 330.0f;
volatile byte  maxmode = false;
bool  notePending = false;
short btnix = 0;
static byte board_off[]={0,1,2,3,4,7,8,11,12,13,14,15};
static byte dir = 1;
EventDelay noteDelay;
byte gain;

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



int maxvalue = 100;

const byte penguin[17][2] PROGMEM = {
  {0b00000000, 0b00000000},  // ................
  {0b00000011, 0b11000000},  // ##............##
  {0b00000111, 0b11100000},  // ###..........###
  {0b00001111, 0b11110000},  // ####........####
  {0b00011111, 0b11111000},  // #####......#####
  {0b00011111, 0b11111000},  // #####......#####
  {0b00011101, 0b10111000},  // #.###......###.#
  {0b00011111, 0b11111000},  // #####......#####
  {0b01111100, 0b00111110},  // ..#####..#####..
  {0b11111110, 0b01111111},  // .##############.
  {0b00011001, 0b10011000},  // #..##......##..#
  {0b00010000, 0b00001000},  // ....#......#....
  {0b00010000, 0b00001000},  // ....#......#....
  {0b00001111, 0b11110000},  // ####........####
  {0b00000010, 0b01000000},  // .#............#.
  {0b00000110, 0b01100000},  // .##..........##.
  {0b00000000, 0b00000000},  // ................
};
void line(int off, int id, int row) {
  lc[off].setRow(id, row, B11111111);
}
void unline(int off, int id, int row) {
  lc[off].setRow(id, row, 0);
}
void maxcol(int id) {
  unline(0,3,7);
  unline(0,4,7);
  lc[0].setRow(3-(id>>1),7,B11110000 >> (4 * (id%2)));
}

void showPenguin(int ox, int oy) {
  for (int y = 0; y < 17; y++) {
    byte lo = pgm_read_byte(&penguin[y][0]);
    byte hi = pgm_read_byte(&penguin[y][1]);
    int disp_y = (oy + y) >> 3;
    int row    = (oy + y) & 7;
    lc[disp_y].setRow(2, row, lo);  // left half
    lc[disp_y].setRow(1, row, hi);  // right half
  }
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

  env.setADLevels(255, 0);
  //noteDelay.set(1000); 
  env.noteOn();
}
unsigned int duration, attack, decay, sustain, release_ms;
void updateControl() {
    currentHz += 0.15f * (targetHz - currentHz);
    float play = targetHz;
    saw1.setFreq(play);

    saw2.setFreq(play * 1.04f);
    sqOsc.setFreq(play * 0.99f);

  if(noteDelay.ready()){
    // Portamento
 


    attack = 5;
    decay = map(mozziAnalogRead(A3),0,1023,10,200);
    sustain = 10;
    release_ms = 10;
    env.setTimes(attack,decay,sustain,release_ms); 
    //env.noteOn();
    
    noteDelay.start(attack + decay + sustain + release_ms);

  }
  if (notePending) {
    env.noteOn();
    notePending = false;
  }
  env.update();
  gain = env.next();
  Serial.println(gain);

}

AudioOutput_t updateAudio() {
  return MonoOutput::from8Bit((int)(gain * saw1.next()));
  int s1  = saw1.next();
  int s2  = saw2.next();
  int sq  = sqOsc.next();

  int raw = ((s1 + s2) / 2 * 7 + sq) >> 3; 
  long e    = env.next();
  int enved = (int)((raw * e) >> 8);
  return MonoOutput::from8Bit(enved*gain);

//  return filter.next(enved);
}

void prepBig(int lvl) {
  for(byte ix = 0; ix < 2; ix++) {
    for (byte iy = 0; iy <2 ; iy++) {
      lc[ix+1].clearDisplay(iy+1);
      lc[ix+1].setIntensity(iy+1, lvl);
    }
  }
}

int column = 0;
int increment(int n, byte col) {
  int place[] = {1000, 100, 10, 1};
  int digit = (n / place[col]) % 10;
  if (digit == 9)
    n -= 9 * place[col];  // wrap to 0
  else
    n += place[col];
  return n;
}

void loop() {
  audioHook();  

  static unsigned long lastStep = 0;
  static short cidx = 0;
  unsigned long now = millis();
  static long prev_value;
  static byte lvalue[2] = {0, 0};
  if (maxmode) {
    if (now - lastStep >= 80) {

      lastStep = now;
      if (maxmode == 1) {
         if (digitalRead(BTN_STOP) == LOW) {
            maxmode = 2;
         }
         return;
      }
      if (digitalRead(BTN_START)&(digitalRead(BTN_START)^lvalue[1]) == HIGH) {
        column++;
        lastStep = now;
        if (column == 4) {
          maxmode=0;
          running = true;
          cidx = 0;
          lc[0].setIntensity(3,0);
          lc[0].setIntensity(2,0);
          return;
        }
        maxcol(column);
        
      } else if (digitalRead(BTN_STOP) &(digitalRead(BTN_STOP)^lvalue[0])== HIGH) {
        lastStep = now;
        maxvalue=increment(maxvalue,column);
      }
      displayNumber(0,3,maxvalue/100,0,false);
      displayNumber(0,2,maxvalue%100,0,false);
      lvalue[0] = digitalRead(BTN_STOP);
      lvalue[1] = digitalRead(BTN_START);
    }

    return;
  }
  if (digitalRead(BTN_STOP) == HIGH) {
    prepBig(5);
    running = false;
    cidx = 0;
    dir = 1;
    if(digitalRead(BTN_START) == HIGH) {
      maxmode = 1;
      column = 0;
      for (int i=0; i < 16; i++) {
        lc[i>>2].clearDisplay(i%4);
      }
      lc[0].setIntensity(3,15);
      lc[0].setIntensity(2,15);

      maxcol(column);
      return;
    }
  } 

  if (digitalRead(BTN_START) == HIGH) {
    prepBig(0);
    maxmode = false;
    running = true;
    cidx = 0;Serial.println(num);
    
  }
 
  if (!running) {
    if (now - lastStep >= 145) {
      num++;
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
        targetHz = (float)boardNotes[num%16]*((num>>6)%4+1)/6;
        notePending = true;
      }
      else {
        showPenguin(8,abs(cidx)%3+5);
        cidx++;

        if(cidx>2) {
          cidx=-3;
        }
      }
    }
  } else if (now - lastStep >= 160) {
     num++;
    int tmp = cidx + CHANGE;
    lastStep = now;
    for (; cidx < tmp; cidx++) {
      byte _cidx = board_off[cidx % 12];
      byte board = (_cidx& B1100 ) >> 2;
      byte row = _cidx & B0011;
      lc[board].clearDisplay(row);
      if (random(3) == 0) {
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
    lc[selector>>2].setIntensity(selector%4,15);
    line(selector>>2,selector%4,7);
    
    targetHz = (float)boardNotes[num%16]*((num>>2)%2+1)/5;// + boardState[selector]/10;
    Serial.println(targetHz);
    notePending = true;
  }
  return;
  
}