#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <SPI.h>
#include <AnimatedGIF.h>
#include <SPIFFS.h>

// Pin definitions for ESP32-C3 Super Mini
// Avoid GPIO2, GPIO8, GPIO9 - these are strapping pins on the C3
// and can interfere with boot mode selection if driven at reset.
#define TFT_SCLK  4   // SPI Clock
#define TFT_MOSI  6   // SPI Data
#define TFT_CS    7   // Chip Select
#define TFT_DC    3   // Data/Command
#define TFT_RST   1   // Reset

// Create display object (uses the default SPI bus, remapped in setup())
Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_RST);

// Create GIF object
AnimatedGIF gif;

// File handle for GIF
File gifFile;

// Display dimensions
#define DISPLAY_WIDTH  240
#define DISPLAY_HEIGHT 240

// Buffer for pixel data (optional, for better performance)
uint16_t lineBuffer[DISPLAY_WIDTH];

// GIF draw callback function
void GIFDraw(GIFDRAW *pDraw) {
  uint8_t *s;
  uint16_t *d, *usPalette, usTemp[320];
  int x, y, iWidth;

  usPalette = pDraw->pPalette;
  y = pDraw->iY + pDraw->y; // current line

  s = pDraw->pPixels;
  // Apply the new pixels to the main image
  if (pDraw->ucHasTransparency) { // if transparency used
    uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
    int x, iCount;
    pEnd = s + pDraw->iWidth;
    x = 0;
    iCount = 0; // count non-transparent pixels
    while(x < pDraw->iWidth) {
      c = ucTransparent-1;
      d = usTemp;
      while (c != ucTransparent && s < pEnd) {
        c = *s++;
        if (c == ucTransparent) { // done, stop
          s--; // back up to treat it like transparent
        } else { // opaque
          *d++ = usPalette[c];
          iCount++;
        }
      } // while looking for opaque pixels
      if (iCount) { // any opaque pixels?
        tft.startWrite();
        tft.setAddrWindow(pDraw->iX + x, y, iCount, 1);
        tft.writePixels(usTemp, iCount);
        tft.endWrite();
        x += iCount;
        iCount = 0;
      }
      // no, look for a run of transparent pixels
      c = ucTransparent;
      while (c == ucTransparent && s < pEnd) {
        c = *s++;
        if (c == ucTransparent)
          x++;
        else
          s--;
      }
    }
  } else { // no transparency, just copy them all
    s = pDraw->pPixels;
    // Translate the 8-bit pixels through the RGB565 palette
    for (x=0; x<pDraw->iWidth; x++)
      usTemp[x] = usPalette[*s++];
    tft.startWrite();
    tft.setAddrWindow(pDraw->iX, y, pDraw->iWidth, 1);
    tft.writePixels(usTemp, pDraw->iWidth);
    tft.endWrite();
  }
} // GIFDraw()

// Function to open GIF file
void * GIFOpenFile(const char *fname, int32_t *pSize) {
  gifFile = SPIFFS.open(fname);
  if (gifFile) {
    *pSize = gifFile.size();
    return (void *)&gifFile;
  }
  return NULL;
}

// Function to close GIF file
void GIFCloseFile(void *pHandle) {
  File *f = static_cast<File *>(pHandle);
  if (f != NULL)
    f->close();
}

// Function to read GIF file
int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen) {
  int32_t iBytesRead;
  iBytesRead = iLen;
  File *f = static_cast<File *>(pFile->fHandle);
  // Note: If you read a file all the way to the last byte, seek() stops working
  if ((pFile->iSize - pFile->iPos) < iLen)
    iBytesRead = pFile->iSize - pFile->iPos - 1; // <-- ugly work-around
  if (iBytesRead <= 0)
    return 0;
  iBytesRead = (int32_t)f->read(pBuf, iBytesRead);
  pFile->iPos = f->position();
  return iBytesRead;
}

// Function to seek in GIF file
int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition) {
  int i = micros();
  File *f = static_cast<File *>(pFile->fHandle);
  f->seek(iPosition);
  pFile->iPos = (int32_t)f->position();
  i = micros() - i;
  return pFile->iPos;
}

void setup() {
  Serial.begin(115200);
  // Note: on the C3 Super Mini, make sure "USB CDC On Boot" is set to
  // "Enabled" in Tools menu, otherwise Serial output won't show up
  // over the USB port.

  // Explicitly map the SPI pins - the C3 has no fixed VSPI/HSPI like
  // the classic ESP32, so pins must be assigned via the GPIO matrix.
  SPI.begin(TFT_SCLK, -1 /* MISO not used */, TFT_MOSI, TFT_CS);

  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS initialization failed!");
    return;
  }
  Serial.println("SPIFFS initialized successfully!");

  // Initialize the display
  tft.begin();
  tft.setRotation(0);
  tft.fillScreen(0x0000); // Black background

  Serial.println("Display initialized successfully!");

  // Initialize GIF decoder
  gif.begin(LITTLE_ENDIAN_PIXELS);
}

void loop() {
  // Open the GIF file (replace with your GIF filename)
  // The GIF should be uploaded to SPIFFS with the name "animation.gif"
  if (gif.open("/animation.gif", GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw)) {
    Serial.printf("Successfully opened GIF; Canvas size = %d x %d\n", gif.getCanvasWidth(), gif.getCanvasHeight());

    // Center the GIF on the round display if it's smaller than 240x240
    int xOffset = (DISPLAY_WIDTH - gif.getCanvasWidth()) / 2;
    int yOffset = (DISPLAY_HEIGHT - gif.getCanvasHeight()) / 2;

    // Play the GIF
    while (gif.playFrame(true, NULL)) {
      // The frame is automatically drawn by the GIFDraw callback
      // Add a small delay if the GIF plays too fast
      // delay(10); // Uncomment and adjust if needed
    }

    // Close the GIF file
    gif.close();

    // Small delay before restarting the loop
    delay(100);
  } else {
    Serial.println("Failed to open GIF file!");
    Serial.println("Make sure 'animation.gif' is uploaded to SPIFFS");
    delay(5000); // Wait 5 seconds before retrying
  }
}