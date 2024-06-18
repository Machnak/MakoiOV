#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <LittleFS.h>
#include <vector>

// NeoPixel configuration
#define PIN1 18
#define PIN2 19
#define NUMPIXELS 36

Adafruit_NeoPixel strip1(NUMPIXELS, PIN1, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip2(NUMPIXELS, PIN2, NEO_GRB + NEO_KHZ800);

// MPU6050 configuration
Adafruit_MPU6050 mpu;
#define MPU_ADDR 0x68

// Buffer for pixel data
uint8_t* pixelData = nullptr;
int imageHeight = 0;  // Variable to store the height of the image

// Image list and index
std::vector<String> imageList;
int currentImageIndex = 0;

// Display duration for each image
unsigned long imageDisplayDuration = 5000;  // Default display duration (5 seconds)
unsigned long lastImageChangeTime = 0;

// Parsed INI file parameters
int comets = 3;
int link_1a = -2, link_1b = -2, link_1j = -2, link_1k = -2;
int limit_1a = -2, limit_1b = -2, limit_1j = -2, limit_1k = -2;

void initMPU6050() {
  if (!mpu.begin(MPU_ADDR)) {
    Serial.println("Failed to find MPU6050 chip");
    while (1) {
      delay(10);
    }
  }
  Serial.println("MPU6050 Found!");
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
}

void initLittleFS() {
  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed");
    return;
  }
  Serial.println("LittleFS mount successful");
}

bool loadBMP(const char* filename) {
  File bmpFile = LittleFS.open(filename, "r");
  if (!bmpFile) {
    Serial.println("Failed to open BMP file");
    return false;
  }

  // Read BMP header
  uint8_t header[54];
  if (bmpFile.read(header, 54) != 54) {
    Serial.println("Failed to read BMP header");
    bmpFile.close();
    return false;
  }

  // Parse BMP width and height from the header
  int width = *(int*)&header[18];
  int height = *(int*)&header[22];
  imageHeight = height;

  // Check if the width fits the strip configuration
  if (width != 36) {
    Serial.println("Unsupported image width");
    bmpFile.close();
    return false;
  }

  // Parse BMP pixel data offset
  int dataOffset = *(int*)&header[10];

  // Calculate the size of the pixel data
  int imageSize = width * height * 3;  // 3 bytes per pixel (24-bit BMP)

  // Allocate memory for pixel data if not already done
  if (pixelData != nullptr) {
    delete[] pixelData;
  }
  pixelData = new uint8_t[imageSize];

  // Seek to the start of pixel data
  bmpFile.seek(dataOffset);

  // Read pixel data
  if (bmpFile.read(pixelData, imageSize) != imageSize) {
    Serial.println("Failed to read BMP pixel data");
    bmpFile.close();
    return false;
  }

  bmpFile.close();
  return true;
}

void displayBMP() {
  if (pixelData == nullptr) {
    Serial.println("Pixel data not loaded");
    return;
  }

  // Process pixel data and send to NeoPixel strips line by line
  for (int y = 0; y < imageHeight; y++) {
    for (int x = 0; x < 36; x++) {
      int pixelIndex = ((imageHeight - 1 - y) * 36 + x) * 3;  // BMP data is bottom-up
      uint8_t blue = pixelData[pixelIndex];
      uint8_t green = pixelData[pixelIndex + 1];
      uint8_t red = pixelData[pixelIndex + 2];

      // Convert to NeoPixel format and set pixel color for both strips
      strip1.setPixelColor(x, strip1.Color(red, green, blue));
      strip2.setPixelColor(x, strip2.Color(red, green, blue));
    }
    strip1.show();
    strip2.show();
  }
}

void parseINI(const char* filename) {
  File iniFile = LittleFS.open(filename, "r");
  if (!iniFile) {
    Serial.println("INI file not found, skipping");
    return;
  }

  while (iniFile.available()) {
    String line = iniFile.readStringUntil('\n');
    line.trim();

    if (line.startsWith("comets=")) {
      comets = line.substring(7).toInt();
    } else if (line.startsWith("link_1a=")) {
      link_1a = line.substring(8).toInt();
    } else if (line.startsWith("link_1b=")) {
      link_1b = line.substring(8).toInt();
    } else if (line.startsWith("link_1j=")) {
      link_1j = line.substring(8).toInt();
    } else if (line.startsWith("link_1k=")) {
      link_1k = line.substring(8).toInt();
    } else if (line.startsWith("limit_1a=")) {
      limit_1a = line.substring(9).toInt();
    } else if (line.startsWith("limit_1b=")) {
      limit_1b = line.substring(9).toInt();
    } else if (line.startsWith("limit_1j=")) {
      limit_1j = line.substring(9).toInt();
    } else if (line.startsWith("limit_1k=")) {
      limit_1k = line.substring(9).toInt();
    }
  }

  iniFile.close();
}

bool loadImageList() {
  File listFile = LittleFS.open("/imagelist.txt", "r");
  if (!listFile) {
    Serial.println("Failed to open imagelist.txt");
    return false;
  }

  while (listFile.available()) {
    String line = listFile.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      imageList.push_back(line);
    }
  }

  listFile.close();
  return !imageList.empty();
}

void setup() {
  Serial.begin(115200);
  strip1.begin();
  strip2.begin();
  strip1.show();  // Initialize all pixels to 'off'
  strip2.show();  // Initialize all pixels to 'off'

  initMPU6050();
  initLittleFS();

  if (!loadImageList()) {
    Serial.println("Failed to load image list");
    return;
  }

  if (!loadBMP(imageList[currentImageIndex].c_str())) {
    Serial.println("Failed to load BMP");
  }

  // Parse the corresponding INI file for the first image if it exists
  String iniFilename = String(imageList[currentImageIndex]);
  iniFilename.replace(".bmp", ".ini");
  parseINI(iniFilename.c_str());
}

void loop() {
  static unsigned long lastFrameTime = micros();
  const unsigned long frameDuration = 3333;  // 300 FPS

  // Read sensor data
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // Use sensor data (example: use the accelerometer's X-axis to adjust the image display duration)
  // Here, we're mapping the X-axis acceleration to a duration between 1000 and 5000 ms
  imageDisplayDuration = map(a.acceleration.x, -10, 10, 1000, 5000);
  imageDisplayDuration = constrain(imageDisplayDuration, 1000, 5000);

  if (micros() - lastFrameTime >= frameDuration) {
    lastFrameTime += frameDuration;

    // Move to the next image if necessary
    unsigned long currentTime = millis();
    if (currentTime - lastImageChangeTime >= imageDisplayDuration) {
      currentImageIndex = (currentImageIndex + 1) % imageList.size();
      if (!loadBMP(imageList[currentImageIndex].c_str())) {
        Serial.println("Failed to load BMP");
      }

      // Optionally parse the corresponding INI file for the current image if it exists
      String iniFilename = String(imageList[currentImageIndex]);
      iniFilename.replace(".bmp", ".ini");
      parseINI(iniFilename.c_str());

      lastImageChangeTime = currentTime;
    }

    // Display the current BMP image
    displayBMP();
  }
}
