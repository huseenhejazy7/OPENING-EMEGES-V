#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

#include <SD.h>
#include <SPI.h>

#define SD_CS    4  // Chip select line for SD card
#define TFT_CS  10  // Chip select line for TFT display
#define TFT_DC   8  // Data/command line for TFT
#define TFT_RST  9  // Reset line for TFT (or connect to +5V)

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

File root;

File myFile;
void setup(void) {
  
  delay(500);
  Serial.begin(9600);
  delay(2000); 
  Serial.println("==ALM ALTOKNOLOGIA==");
  Serial.println("- setup() -");
  
 //The following two lines replace "TFTscreen.begin();" => avoids that red and blue (?) are swapped/interchanged  
  tft.initR(INITR_BLACKTAB);   
  tft.setRotation(1); // set display orientation 
   
  Serial.println("tft: " 
                  + String(tft.width()) + " : " 
                  + String(tft.height()));
  tft.fillScreen(ST7735_BLACK);
  tft.setTextWrap(true);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(0, 0);
  tft.print("Arduino nano RP2040 Connect");
  Serial.println("Initializing SD card...");
  if (!SD.begin(SD_CS)){
    Serial.println("SD.begin() failed!");
  }
  else{
    Serial.println("SD.begin() Success....OK..");
    root = SD.open("/");
    printDirectory(root, 0);
  }
  Serial.println("\n- End of setup() -\n");
}
const int NO_OF_BMP = 13;
char* bmpFiles[NO_OF_BMP] = {"/1.bmp","/2.bmp","/3.bmp","/4.bmp","/5.bmp", "/6.bmp", "/7.bmp", "/8.bmp", "/9.bmp", "/10.bmp", "/11.bmp", "/12.bmp"};

void loop() {
  for(int i=0; i<NO_OF_BMP; i++){
    bmpDraw(bmpFiles[i], 0, 0);
    delay(2000);
  }
}
void printDirectory(File dir, int numTabs) {
  while (true) {
    File entry =  dir.openNextFile();
    if (! entry) {
      // no more files
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('\t');
    }
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
      printDirectory(entry, numTabs + 1);
    } else {
      // files have sizes, directories do not
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}
#define BUFFPIXEL 20
void bmpDraw(char *filename, uint8_t x, uint8_t y) {
  File     bmpFile;
  int      bmpWidth, bmpHeight;   // W+H in pixels
  uint8_t  bmpDepth;              // Bit depth (currently must be 24)
  uint32_t bmpImageoffset;        // Start of image data in file
  uint32_t rowSize;               // Not always = bmpWidth; may have padding
  uint8_t  sdbuffer[3*BUFFPIXEL]; // pixel buffer (R+G+B per pixel)
  uint8_t  buffidx = sizeof(sdbuffer); // Current position in sdbuffer
  boolean  goodBmp = false;       // Set to true on valid header parse
  boolean  flip    = true;        // BMP is stored bottom-to-top
  int      w, h, row, col;
  uint8_t  r, g, b;
  uint32_t pos = 0, startTime = millis();
  if((x >= tft.width()) || (y >= tft.height())) return;
  Serial.println();
  Serial.print("Loading image '");
  Serial.print(filename);
  Serial.println('\'');
  // Open requested file on SD card
  if ((bmpFile = SD.open(filename)) == NULL) {
    Serial.print("File not found");
    return;
  }
  if(read16(bmpFile) == 0x4D42) { // BMP signature
    Serial.print("File size: "); Serial.println(read32(bmpFile));
    (void)read32(bmpFile); // Read & ignore creator bytes
    bmpImageoffset = read32(bmpFile); // Start of image data
    Serial.print("Image Offset: "); Serial.println(bmpImageoffset, DEC);
    // Read DIB header
    Serial.print("Header size: "); Serial.println(read32(bmpFile));
    bmpWidth  = read32(bmpFile);
    bmpHeight = read32(bmpFile);
    if(read16(bmpFile) == 1) { // # planes -- must be '1'
      bmpDepth = read16(bmpFile); // bits per pixel
      Serial.print("Bit Depth: "); Serial.println(bmpDepth);
      if((bmpDepth == 24) && (read32(bmpFile) == 0)) { // 0 = uncompressed
        goodBmp = true; // Supported BMP format -- proceed!
        Serial.print("Image size: ");
        Serial.print(bmpWidth);
        Serial.print('x');
        Serial.println(bmpHeight);
        rowSize = (bmpWidth * 3 + 3) & ~3;
        if(bmpHeight < 0) {
          bmpHeight = -bmpHeight;
          flip      = false;
        }
        w = bmpWidth;
        h = bmpHeight;
        if((x+w-1) >= tft.width())  w = tft.width()  - x;
        if((y+h-1) >= tft.height()) h = tft.height() - y;
        tft.startWrite();
        tft.setAddrWindow(x, y, w, h);
        for (row=0; row<h; row++) { // For each scanline...
          if(flip) // Bitmap is stored bottom-to-top order (normal BMP)
            pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
          else     // Bitmap is stored top-to-bottom
            pos = bmpImageoffset + row * rowSize;
          if(bmpFile.position() != pos) { // Need seek?
            tft.endWrite();
            bmpFile.seek(pos);
            buffidx = sizeof(sdbuffer); // Force buffer reload
          }
          for (col=0; col<w; col++) { // For each pixel...        
            if (buffidx >= sizeof(sdbuffer)) { // Indeed
              bmpFile.read(sdbuffer, sizeof(sdbuffer));
              buffidx = 0; // Set index to beginning
              tft.startWrite();
            }
            b = sdbuffer[buffidx++];
            g = sdbuffer[buffidx++];
            r = sdbuffer[buffidx++];
            tft.pushColor(tft.color565(r,g,b));
          } // end pixel
        } // end scanline
        tft.endWrite();
        Serial.print("Loaded in ");
        Serial.print(millis() - startTime);
        Serial.println(" ms");
      } // end goodBmp
    }
  }
  bmpFile.close();
  if(!goodBmp) Serial.println("BMP format not recognized.");
}
uint16_t read16(File f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}
uint32_t read32(File f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}
