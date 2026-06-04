//LEDs and sound with ON/OFF button. No gyroscope.

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "driver/i2s.h"
#include "ON.h" 
#include "OFF.h" 
#include "HUM.h" 

// --- Hardware Pins ---
const int BUTTON_PIN = 33;
const int PIXEL_PIN  = 13; // Updated to GPIO 13
const int NUM_PIXELS = 8; // Change to your string length

// I2S Pins
#define I2S_BCK  26
#define I2S_WS   25
#define I2S_DOUT 27

// --- Objects & Config ---
Adafruit_NeoPixel strip(NUM_PIXELS, PIXEL_PIN, NEO_GRB + NEO_KHZ800);
const i2s_port_t I2S_PORT = I2S_NUM_0;
const int sampleRate = 22050; // Set to match your 16-bit PCM export

// --- State Tracking ---
volatile bool active = false; 
volatile bool trigerOff = false;
bool hasRunA = false; 

void IRAM_ATTR handleButton() {
  static unsigned long last_press = 0;
  if (millis() - last_press > 250) {
    if (!active) active = true;
    else { active = false; trigerOff = true; }
    last_press = millis();
  }
}

// =========================================================
// AUDIO + LED ENGINE
// Modes: 1=Wipe ON, 2=Flicker, 3=Wipe OFF
// =========================================================
void playAudioWithFX(const unsigned char* data, uint32_t len, int mode) {
  size_t written;
  uint32_t pos = 44; // Skip WAV header
  float volume = 0.3; // 0.1 is quiet, 1.0 is full volume

  // Temporary buffer to hold "dimmed" audio samples
  const int chunk_size = 512;
  int16_t volumeBuffer[chunk_size / 2]; 
  
  while (pos < len) {
    if (!active && mode == 2) break; // Instant interrupt for loop B

    // 1. Prepare a chunk of audio
    uint32_t step = (len - pos > chunk_size) ? chunk_size : (len - pos);
    int numSamples = step / 2;

    // 2. APPLY VOLUME REDUCTION
    for (int i = 0; i < numSamples; i++) {
      // Combine two bytes into one 16-bit signed integer (Little Endian)
      int16_t sample = (int16_t)((data[pos + (i * 2 + 1)] << 8) | data[pos + (i * 2)]);
      
      // Multiply by volume (0.0 - 1.0)
      volumeBuffer[i] = (int16_t)(sample * volume);
    }

    // 3. Write the "dimmed" buffer to I2S
    i2s_write(I2S_PORT, volumeBuffer, numSamples * 2, &written, portMAX_DELAY);
    pos += step;

    // --- LED Effects Synced to Audio Progress ---
    float progress = (float)pos / (float)len;
    
    if (mode == 1) { // WIPE ON (Function A)
      int ledToLight = progress * NUM_PIXELS;
      strip.setPixelColor(ledToLight, strip.Color(0, 200, 0)); // Green
      strip.show();
    } 
    else if (mode == 2) { // PULSING "HUM" (Function B)
      // Calculate a pulse value based on time (ms)
      // 0.025 is the speed. Higher = faster pulsing.
      float pulse = (sin(millis() * 0.025) + 1.0) / 2.0; 
      
      // Map pulse (0.0 - 1.0) to a brightness range (55 - 255)
      int brightness = 155 + (pulse * 100); 
      
      // Apply the same green color as Mode 1, but with pulsing brightness
      for(int i = 0; i < NUM_PIXELS; i++) {
        strip.setPixelColor(i, strip.Color(0, brightness, 0)); 
      }
      strip.show();
    }
    else if (mode == 3) { // WIPE OFF (Function C)
      int ledToKill = (NUM_PIXELS - 1) - (int)(progress * NUM_PIXELS);
      if (ledToKill >= 0 && ledToKill < NUM_PIXELS) {
        strip.setPixelColor(ledToKill, strip.Color(0, 0, 0));
        strip.show();
      }
    }
  }
}

void loop() {
  if (active) {
    if (!hasRunA) {
      i2s_init(); 
      playAudioWithFX(ON_wav, sizeof(ON_wav), 1); // Wipe ON
      hasRunA = true;
    }
    playAudioWithFX(HUM_wav, sizeof(HUM_wav), 2); // Flicker
  }

  if (trigerOff) {
    playAudioWithFX(OFF_wav, sizeof(OFF_wav), 3); // Wipe OFF
    i2s_driver_uninstall(I2S_PORT);
    hasRunA = false; 
    trigerOff = false;
  }
}

// =========================================================
// HARDWARE INITIALIZATION
// =========================================================
void setup() {
  strip.begin();
  strip.setBrightness(60); // (out of 255)
  strip.show(); 
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButton, FALLING);
}

void i2s_init() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = sampleRate,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false // necessary? what for?

  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };
  
  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
}
