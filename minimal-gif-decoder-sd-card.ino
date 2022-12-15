/* Minimal GIF Decoder SD Card Version - see http://www.technoblogy.com/show?45YI

   David Johnson-Davies - www.technoblogy.com - 15th December 2022
   AVR128DA28 @ 24MHz (internal clock)
   
   CC BY 4.0
   Licensed under a Creative Commons Attribution 4.0 International license: 
   http://creativecommons.org/licenses/by/4.0/
*/

#include <SPI.h>
#include <SD.h>

typedef struct {
  int16_t rest;
  uint8_t last;
} cell_t;

// Arduino pin numbers. Change these for your display connections
int const cs = 1; // TFT display SPI chip select pin
int const dc = 2; // TFT display data/command select pin
int const sd = 0; // SD-card SPI chip select pin

// Display parameters - uncomment the line for the one you want to use

// Adafruit 1.44" 128x128 display
// int const xsize = 128, ysize = 128, xoff = 2, yoff = 1, invert = 0, rotate = 3, bgr = 1;

// AliExpress 1.44" 128x128 display
// int const xsize = 128, ysize = 128, xoff = 2, yoff = 1, invert = 0, rotate = 3, bgr = 1;

// Adafruit 0.96" 160x80 display
// int const xsize = 160, ysize = 80, xoff = 0, yoff = 24, invert = 0, rotate = 6, bgr = 1;

// AliExpress 0.96" 160x80 display
// int const xsize = 160, ysize = 80, xoff = 1, yoff = 26, invert = 1, rotate = 0, bgr = 1;

// Adafruit 1.8" 160x128 display
int const xsize = 160, ysize = 128, xoff = 0, yoff = 0, invert = 0, rotate = 6, bgr = 1;

// AliExpress 1.8" 160x128 display (red PCB)
// int const xsize = 160, ysize = 128, xoff = 0, yoff = 0, invert = 0, rotate = 0, bgr = 1;

// AliExpress 1.8" 160x128 display (blue PCB)
// int const xsize = 160, ysize = 128, xoff = 0, yoff = 0, invert = 0, rotate = 6, bgr = 0;

// Adafruit 1.14" 240x135 display
// int const xsize = 240, ysize = 135, xoff = 40, yoff = 53, invert = 1, rotate = 6, bgr = 0;

// AliExpress 1.14" 240x135 display
// int const xsize = 240, ysize = 135, xoff = 40, yoff = 52, invert = 1, rotate = 0, bgr = 0;

// Adafruit 1.3" 240x240 display
// int const xsize = 240, ysize = 240, xoff = 0, yoff = 80, invert = 1, rotate = 5, bgr = 0;

// Adafruit 1.54" 240x240 display
// int const xsize = 240, ysize = 240, xoff = 0, yoff = 80, invert = 1, rotate = 5, bgr = 0;

// AliExpress 1.54" 240x240 display
// int const xsize = 240, ysize = 240, xoff = 0, yoff = 80, invert = 1, rotate = 5, bgr = 0;

// Adafruit 1.9" 320x170 display
// int const xsize = 320, ysize = 170, xoff = 0, yoff = 35, invert = 1, rotate = 0, bgr = 0;

// Adafruit 2.0" 320x240 display
// int const xsize = 320, ysize = 240, xoff = 0, yoff = 0, invert = 1, rotate = 6, bgr = 0;

// Adafruit 2.2" 320x240 display
// int const xsize = 320, ysize = 240, xoff = 0, yoff = 0, invert = 0, rotate = 4, bgr = 1;

// AliExpress 2.4" 320x240 display
// int const xsize = 320, ysize = 240, xoff = 0, yoff = 0, invert = 0, rotate = 2, bgr = 1;

// TFT colour display **********************************************

int const CASET = 0x2A; // Define column address
int const RASET = 0x2B; // Define row address
int const RAMWR = 0x2C; // Write to display RAM

// Global - colour
int fore = 0xFFFF; // White

// Send a byte to the display
void Data (uint8_t d) {
  digitalWrite(cs, LOW);
  SPI.transfer(d);
  digitalWrite(cs, HIGH);
}

// Send a command to the display
void Command (uint8_t c) {
  digitalWrite(dc, LOW);
  Data(c);
  digitalWrite(dc, HIGH);
}

// Send a command followed by two data words
void Command2 (uint8_t c, uint16_t d1, uint16_t d2) {
  digitalWrite(dc, LOW);
  Data(c);
  digitalWrite(dc, HIGH);
  Data(d1>>8); Data(d1); Data(d2>>8); Data(d2);
}
  
void InitDisplay () {
  pinMode(dc, OUTPUT);
  pinMode(cs, OUTPUT);
  digitalWrite(cs, HIGH);  
  digitalWrite(dc, HIGH);                  // Data
  SPI.begin();
  Command(0x01);                           // Software reset
  delay(250);                              // delay 250 ms
  Command(0x36); Data(rotate<<5 | bgr<<3); // Set orientation and rgb/bgr
  Command(0x3A); Data(0x55);               // Set color mode - 16-bit color
  Command(0x20+invert);                    // Invert
  Command(0x11);                           // Out of sleep mode
  delay(150);
}

void DisplayOn () {
  Command(0x29);                           // Display on
  delay(150);
}

void ClearDisplay () {
  Command2(CASET, yoff, yoff + ysize - 1);
  Command2(RASET, xoff, xoff + xsize - 1);
  Command(0x3A); Data(0x03);               // 12-bit colour
  Command(RAMWR);
  for (int i=0; i<xsize/2; i++) {
    for (int j=0; j<ysize*3; j++) {
      Data(0);
    }
  }
  Command(0x3A); Data(0x05);               // Back to 16-bit colour
}

uint16_t Colour (int r, int g, int b) {
  return (r & 0xf8)<<8 | (g & 0xfc)<<3 | b>>3;
}

// Plot point at x,y
void PlotPoint (int x, int y) {
  Command2(CASET, yoff+y, yoff+y);
  Command2(RASET, xoff+x, xoff+x);
  Command(RAMWR); Data(fore>>8); Data(fore & 0xff);
}

// GIF Decoder **********************************************

cell_t Table[4096];
uint16_t ColourTable[256];

uint8_t Nbuf;
uint32_t Buf;
int Width, Block;
int Pixel = 0;
File Image;

int GetNBits (int n) {
  while (Nbuf < n) {
    if (Block == 0) Block = ReadByte();
    Buf = ((uint32_t)ReadByte() << Nbuf) | Buf;
    Block--; Nbuf = Nbuf + 8;
  }
  int result = ((1 << n) - 1) & Buf;
  Buf = Buf >> n; Nbuf = Nbuf - n;
  return result;
}

uint8_t FirstPixel (int c) {
  uint8_t last;
  do {
    last = Table[c].last;
    c = Table[c].rest;
  } while (c != -1);
  return last;
}

void PlotSequence (int c) {
  // Measure backtrack
  int i = 0, rest = c;
  while (rest != -1) {
    rest = Table[rest].rest;
    i++;
  }
  // Plot backwards
  Pixel = Pixel + i - 1;
  rest = c;
  while (rest != -1) {
    fore = ColourTable[Table[rest].last];
    PlotPoint (Pixel%Width, ysize - Pixel/Width - 1);
    Pixel--;
    rest = Table[rest].rest;
  }
  Pixel = Pixel + i + 1;
}

void SkipNBytes (int n) {
  for (int i=0; i<n; i++) ReadByte();
}

boolean Power2 (int x) {
  return (x & (x - 1)) == 0;
}

void OpenFile (const char* filename) {
  Image = SD.open(filename);
}

void CloseFile () {
  Image.close();
}

uint8_t ReadByte () {
  return Image.read();
}

int ReadInt () {
  return ReadByte() | ReadByte()<<8;
}

void Error (int err) {
  for (int i=0; i<err; i++) {
    digitalWrite(LED_BUILTIN, HIGH); delay(200);
    digitalWrite(LED_BUILTIN, LOW); delay(200);
  }
  for(;;);
}

void ShowGif (const uint8_t *filename) {
  OpenFile(filename);
  const char *head = "GIF89a";
  for (int p=0; head[p]; p++) if (ReadByte() != head[p]) Error(2);
  int width = ReadInt();
  ReadInt(); // Height
  uint8_t field = ReadByte();
  SkipNBytes(2); // background, and aspect
  uint8_t colbits = max(1 + (field & 7), 2);
  int colours = 1<<colbits;
  int clr = colours;
  int end = 1 + colours;
  int free = 1 + end;
  uint8_t bits = 1 + colbits;
  Width = width;
  Pixel = 0;

  // Parse colour table
  for (int c = 0; c<colours; c++) {
    ColourTable[c] = Colour(ReadByte(), ReadByte(), ReadByte());
  }
  
  // Initialise table
  for (int c = 0; c<colours; c++) {
    Table[c].rest = -1; Table[c].last = c;
  }
  
  // Parse blocks
  do {
    uint8_t header = ReadByte();
    if (header == 0x2C) { // Image block
      SkipNBytes(8);
      if (ReadByte() != 0) Error(3); // Not interlaced/local
      SkipNBytes(1);
      Nbuf = 0;
      Buf = 0;
      Block = 0;
      boolean stop = false;
      int code = -1, last = -1;
      do {
        last = code;
        code = GetNBits(bits);
        if (code == clr) {
          free = 1 + end;
          bits = 1 + colbits;
          code = -1;
        } else if (code == end) {
          stop = true;
        } else if (last == -1) {
          PlotSequence(code);
        } else if (code < free) {
          Table[free].rest = last;
          Table[free].last = FirstPixel(code);
          PlotSequence(code);
          free++;
          if (Power2(free)) bits++;   
        } else if (code == free) {
          Table[free].rest = last;
          Table[free].last = FirstPixel(last);
          PlotSequence(code);
          free++;
          if (Power2(free)) bits++;
        } 
      } while(!stop);
      if (ReadByte() != 0) Error(4);
    } else if (header == 0x21) { // Extension block
      SkipNBytes(1); // GCE
      int length = ReadByte();
      SkipNBytes(1 + length);
    } else if (header == 0x3b) { // Terminating byte
      CloseFile();
      return;
    }
  } while (true);
}
  
// Setup **********************************************

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  InitDisplay();
  ClearDisplay();
  DisplayOn();
  SD.begin(sd);
}

void loop () {
  ShowGif("cards9.gif");
  for(;;);
}
