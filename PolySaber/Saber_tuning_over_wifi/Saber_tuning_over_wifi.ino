// Sketch with LED strip, I2S speaker, and gyroscope.
// Button to turn saber on/off. Long press to change color.
// LED strip wipes on/off together with duration of corresponding sound wave.
// Control brightness of LED strip inside setup() to limit current consumption.
// Sound volume configurable with global variables.
// Strike and swing thesholds configurable with global variables. 
// Use loop2() for tuning via serial plot.
// Important: due to sound waves in .h use tools -> partition -> Huge App configuration.
// ESP32 pinout: 
// GPIO 13 pixels
// GPIO 21 MPU6050 gyro i2c SCL default address 0x68 (0x69 also possible)
// GPIO 22 MPU6050 gyro i2c SDA
// GPIO 25 I2S speaker clock
// GPIO 26 I2S speaker word select
// GPIO 27 I2S speaker data
// GPIO 33 button

#include "driver/i2s.h"
#include "ALLSOUNDS.h" 
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>

#define TUNING_MODE // comment this line to toggle tuning off

#ifdef TUNING_MODE
  #include <WiFi.h>
  #include <ESPAsyncWebServer.h>
  #include "Gyro_tuning.h" 
  AsyncWebServer server(80);
  AsyncWebSocket ws("/ws");
  const char* ssid = "KGban-guest";
  const char* password = "enjoynow";
#endif

// Saber thresholds
const float STRIKE_THRESHOLD = 19.0;
const float SWING_THRESHOLD  = 1.5;

// Pixel Pins
const uint8_t PIXEL_PIN  = 13; // GPIO 13
const uint8_t NUM_PIXELS = 8; // String length

// I2S speaker pins
const uint8_t I2S_BCK  = 26;  // Bit Clock
const uint8_t I2S_WS   = 25;  // Word Select (LRC)
const uint8_t I2S_DOUT = 27;  // Data Out (DIN)
const i2s_port_t I2S_PORT = I2S_NUM_0;
const int sampleRate = 22050; // 22KHz sampling rate
uint32_t hum_pos = 44; 
uint32_t swing_pos = 44;

// Button pin
const uint8_t BUTTON_PIN = 33;

//Gyroscope
Adafruit_MPU6050 mpu;
Adafruit_NeoPixel strip(NUM_PIXELS, PIXEL_PIN, NEO_GRB + NEO_KHZ800);

const uint32_t colorPresets[] = {
  strip.Color(47, 249, 36), // Green (Yoda/Qui-Gon/Luke)
  strip.Color(46, 103, 248),// Blue (Anakin/Obi-Wan/Luke/Rey) 
  strip.Color(187, 0, 0),   // Red (Vader/Sith)
  strip.Color(128, 0, 128), // Purple (Mace Windu)
  strip.Color(255, 165, 0), // Orange (Cal Kestis)
  strip.Color(0, 255, 255), // Cyan (Jedi: Fallen Order)
  strip.Color(255, 0, 255), // Magenta (Jedi: Fallen Order)
  strip.Color(255, 255, 0)  // Yellow (Temple Guards)
};
uint8_t colorIndex = 0;
uint32_t bladeColor = colorPresets[0]; // Set initial color
const int NUM_COLORS = sizeof(colorPresets) / sizeof(uint32_t);

bool isRunning = false; // Start in "OFF" state
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50; 

//Declaring functions for compiler
void i2s_init();
void play_audio_with_leds(const unsigned char* wav_data, size_t wav_len, uint8_t volume_shift, int mode);
void update_swing_loop(uint8_t volume_shift);
void update_led_breathing(bool isMoving);
void update_hum(uint8_t volume_shift);


// --------------------------------- SOUNDS ----------------------------------

const uint8_t HUM_VOL = 6;
const uint8_t ONOFF_VOL = 2;
const uint8_t BEEP_VOL = 2;
const uint8_t STRIKE_VOL = 3;
const uint8_t SWING_VOL = 3;

const unsigned char* const strikes[] PROGMEM  = {SK1_wav, SK2_wav, SK3_wav, SK4_wav, SK5_wav, SK6_wav, SK7_wav, SK8_wav};
const size_t strikes_lengths[] = {
  sizeof(SK1_wav), sizeof(SK2_wav), sizeof(SK3_wav), sizeof(SK4_wav),
  sizeof(SK5_wav), sizeof(SK6_wav), sizeof(SK7_wav), sizeof(SK8_wav)
};

const unsigned char* const strikes_short[] PROGMEM = {SKS1_wav, SKS2_wav, SKS3_wav, SKS4_wav,SKS5_wav, SKS6_wav, SKS7_wav, SKS8_wav};
const size_t strikes_short_lengths[] = {
  sizeof(SKS1_wav), sizeof(SKS2_wav), sizeof(SKS3_wav), sizeof(SKS4_wav),
  sizeof(SKS5_wav), sizeof(SKS6_wav), sizeof(SKS7_wav), sizeof(SKS8_wav)};

const unsigned char* const swings_S[] PROGMEM  = {SWS1_wav, SWS2_wav, SWS3_wav, SWS4_wav, SWS5_wav};
const size_t swings_S_lengths[] = {sizeof(SWS1_wav), sizeof(SWS2_wav), sizeof(SWS3_wav), sizeof(SWS4_wav), sizeof(SWS5_wav)};

const unsigned char* const swings_L[] PROGMEM  = {SWL1_wav, SWL2_wav, SWL3_wav, SWL4_wav};
const size_t swings_L_lengths[] = {sizeof(SWL1_wav), sizeof(SWL2_wav), sizeof(SWL3_wav), sizeof(SWL4_wav)};

const int STRIKES_COUNT = sizeof(strikes_lengths) / sizeof(strikes_lengths[0]);
const int STRIKES_SHORT_COUNT = sizeof(strikes_short_lengths) / sizeof(strikes_short_lengths[0]);
const int SWINGS_COUNT = sizeof(swings_S_lengths)/sizeof(swings_S_lengths[0]);
const int SWINGS_L_COUNT = sizeof(swings_L_lengths)/sizeof(swings_L_lengths[0]);

// --------------------------------- SOUNDS ---------------------------------


void setup() {
  Serial.begin(115200);
  strip.begin();           // INITIALIZE NeoPixel strip 
  strip.show();            // Turn OFF all pixels ASAP
  strip.setBrightness(60); // Set BRIGHTNESS (max = 255)
  i2s_init();   
  if (!mpu.begin()) {
    Serial.println("MPU6050 not found!");
    while (1) yield();
  }
  pinMode(BUTTON_PIN, INPUT_PULLUP);
 
  #ifdef TUNING_MODE
    WiFi.begin(ssid, password); 
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }    
    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP()); // Use this IP in the browser

    ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len){}); // {} is WebSocket cleanup logic
    server.addHandler(&ws);
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){request->send_P(200, "text/html", index_html); });
    server.begin();
    Serial.println("Tuning Mode Active");
  #else
    Serial.println("Normal Mode: WiFi Disabled");
  #endif
  
  Serial.println("System Ready. Waiting for button press to start..."); 
}



unsigned long lastTrigger = 0;
unsigned long buttonPressedTime = 0;
bool longPressTriggered = false;

void loop() {
  bool reading = digitalRead(BUTTON_PIN);

  // --- BUTTON DOWN ---
  if (reading == LOW && lastButtonState == HIGH) {
    buttonPressedTime = millis();
    longPressTriggered = false;
  }

  // --- BUTTON HELD (Long Press Detection) ---
  if (reading == LOW && !longPressTriggered) {
    if (millis() - buttonPressedTime > 1000) { // 1 second threshold
      if (isRunning) { // Only change colors if the saber is ON
        colorIndex = (colorIndex + 1) % NUM_COLORS; // Cycle 0, 1, 2...
        bladeColor = colorPresets[colorIndex];
                
        // Play the confirmation sound (BEEP)
        // Use a higher volume (shift 1 or 2) so it's clearly heard over the hum
        play_audio_with_leds(BEEP_wav, BEEP_wav_len, BEEP_VOL, 0); // Mode 0 = Play sound without changing LEDs                 
        // Visual feedback: Quick flash of the new color
        strip.fill(bladeColor);
        strip.show();
        Serial.print("Color Changed! Index: "); Serial.println(colorIndex);
        delay(100); 
      }
      longPressTriggered = true; // Prevent multiple cycles per single hold
    }
  }

  // --- BUTTON RELEASED (Short Press Check) ---
  if (reading == HIGH && lastButtonState == LOW) {
    // If it wasn't a long press, do the normal ON/OFF toggle
    if (!longPressTriggered && (millis() - lastDebounceTime > 250)) {
      isRunning = !isRunning;
      lastDebounceTime = millis();
      
      if (isRunning) {
        Serial.println("System: ON");
        play_audio_with_leds(ON_wav, ON_wav_len, ONOFF_VOL, 1);
      } else {
        Serial.println("System: OFF");
        play_audio_with_leds(OFF_wav, OFF_wav_len, ONOFF_VOL, 2);
        i2s_zero_dma_buffer(I2S_PORT);
      }
    }
  }

  lastButtonState = reading;

//  MAIN LOGIC (Only runs if ON) ---
  if (isRunning) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
  
    float accelMag = sqrt(a.acceleration.x*a.acceleration.x + a.acceleration.y*a.acceleration.y + a.acceleration.z*a.acceleration.z);
    float gyroMag = sqrt(g.gyro.x*g.gyro.x + g.gyro.y*g.gyro.y + g.gyro.z*g.gyro.z);
      // Send data as JSON to the web page
    
    #ifdef TUNING_MODE
      // Only send WebSocket data if in tuning mode
      String json = "{\"a\":" + String(accelMag) + ",\"g\":" + String(gyroMag) + "}";
      ws.textAll(json);
    #endif
      
    // PRIORITY 1: STRIKE (Still blocking, because hits should interrupt everything)
    if (accelMag > STRIKE_THRESHOLD && (millis() - lastTrigger > 600)) {
        play_audio_with_leds(strikes[random(0,8)], strikes_lengths[random(0,8)], STRIKE_VOL, 3);
        lastTrigger = millis();
    } 
    // PRIORITY 2: DYNAMIC MOTION
    else {
        if (gyroMag > SWING_THRESHOLD) { // Any significant movement
            // Map speed to volume: 
            // Faster swing = Shift 2 (Loud), Slow swing = Shift 5 (Quiet)
            int swingVol = map(gyroMag * 10, SWING_THRESHOLD *10, 80, 2, 5); // was 5, 2
            update_swing_loop(constrain(swingVol, 2, 5));
            
            // Brighten LEDs during swing
            update_led_breathing(true); 
        } 
        else {
            // No movement: play the standard hum
            update_hum(HUM_VOL); 
            update_led_breathing(false);
            swing_pos = 44; // Reset swing so it starts fresh next time
        }
    } 
  } 
}



//volume_shift: 0 = full volume, 1 = half, 2 = quarter, 4 = very quiet
void play_audio_with_leds(const unsigned char* wav_data, size_t wav_len, uint8_t volume_shift, int mode) {
    const int CHUNK_SIZE = 512; 
    int16_t buffer[CHUNK_SIZE]; 
    size_t bytes_written;
    uint32_t offset = 44; 

    while (offset < wav_len) {
        // --- 1. Audio Processing ---
        int samples_to_read = CHUNK_SIZE; // Re-declare it here!
        if (offset + (CHUNK_SIZE * 2) > wav_len) {
            samples_to_read = (wav_len - offset) / 2;
        }

        for (int i = 0; i < samples_to_read; i++) {
            int16_t sample = (int16_t)((wav_data[offset + 1] << 8) | wav_data[offset]);
            buffer[i] = sample >> volume_shift;
            offset += 2;
        }

        i2s_write(I2S_PORT, buffer, samples_to_read * 2, &bytes_written, portMAX_DELAY);

        // --- 2. NeoPixel Wipe Logic ---
        // Calculate 0.0 to 1.0 progress through the sound file
        float progress = (float)offset / (float)wav_len;
        int num_leds_changed = progress * NUM_PIXELS;

        if (mode == 1) { // WIPE IN (ON) - LEDs turn on one by one
            for(int i = 0; i < NUM_PIXELS; i++) {
                if(i < num_leds_changed) strip.setPixelColor(i, bladeColor); 
                else strip.setPixelColor(i, 0); // Off
            }
        } 
        else if (mode == 2) { // WIPE OUT (OFF): Retreat from Tip to Base
            // 'progress' goes from 0.0 to 1.0 based on the WAV file offset
            float progress = (float)offset / (float)wav_len;
            int leds_to_keep_on = NUM_PIXELS - (progress * NUM_PIXELS);
        
            // Loop from the tip (last pixel) down to the base (0)
            for (int i = NUM_PIXELS - 1; i >= 0; i--) {
                if (i < leds_to_keep_on) {
                    strip.setPixelColor(i, bladeColor); 
                } else {
                    strip.setPixelColor(i, 0); // Turn Off
                }
            }
        }
        if (mode == 3) { // CLASH MODE
        // Randomly choose between White and Green every audio chunk
        if (random(0, 10) > 5) {
            strip.fill(strip.Color(255, 255, 255)); // White
        } else {
            strip.fill(strip.Color(0, 255, 0));     // Back to Green
        }
    strip.show();
}
        strip.show(); // Push the colors to the strip
    }
    i2s_zero_dma_buffer(I2S_PORT); 
}

void update_led_breathing(bool isMoving) {
    // If moving, make the breath faster (100.0 instead of 150.0)
    float speed = isMoving ? 100.0 : 150.0;
    float breath = (sin(millis() / speed) + 1.0) / 2.0; 
    
    float flicker = 0.9 + (sin(millis() / 20.0) * 0.1); 

    // If moving, make it brighter (base 40, range 180) 
    // If idle, keep it dim (base 10, range 60)
    uint8_t base = isMoving ? 40 : 10;
    uint8_t range = isMoving ? 180 : 60;
    
    // Brightness multiplier (0 to 255)
    uint8_t brightness = (base + (breath * range)) * flicker; 
    
    // Extract R, G, B components from the global bladeColor
    uint8_t r = (uint8_t)((bladeColor >> 16 & 0xFF));
    uint8_t g = (uint8_t)((bladeColor >> 8 & 0xFF));
    uint8_t b = (uint8_t)((bladeColor & 0xFF));
    // Scale each component by our brightness (bright / 255.0)
    uint8_t finalR = (r * brightness) / 255;
    uint8_t finalG = (g * brightness) / 255;
    uint8_t finalB = (b * brightness) / 255;
    
    for(int i = 0; i < NUM_PIXELS; i++) {
      strip.setPixelColor(i, strip.Color(finalR, finalG, finalB));         
    }
    strip.show();
}


void update_hum(uint8_t volume_shift) {
    size_t bytes_written;
    const int CHUNK_SIZE = 512; // Matches play_audio
    int16_t buffer[CHUNK_SIZE]; 

    for (int i = 0; i < CHUNK_SIZE; i++) {
        // Loop-back logic
        if (hum_pos + 1 >= HUM_wav_len) {
            hum_pos = 44; 
        }

        int16_t sample = (int16_t)((HUM_wav[hum_pos + 1] << 8) | HUM_wav[hum_pos]);
        buffer[i] = sample >> volume_shift;
        hum_pos += 2;
    }

    i2s_write(I2S_PORT, buffer, CHUNK_SIZE * 2, &bytes_written, portMAX_DELAY);
}


void update_swing_loop(uint8_t volume_shift) {
    size_t bytes_written;
    const int SAMPLES = 256; 
    int16_t buffer[SAMPLES * 2]; 

    for (int i = 0; i < SAMPLES; i++) {
        // Loop back if at end of the swing sample
        if (swing_pos + 1 >= SWL1_wav_len) swing_pos = 44; 

        int16_t sample = (int16_t)((SWL1_wav[swing_pos + 1] << 8) | SWL1_wav[swing_pos]);
        int16_t quiet_sample = sample >> volume_shift;

        buffer[i * 2] = quiet_sample;     
        buffer[i * 2 + 1] = quiet_sample; 
        swing_pos += 2;
    }
    i2s_write(I2S_PORT, buffer, SAMPLES * 4, &bytes_written, 0);
}

void i2s_init() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 22050, // change to define
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .dma_buf_count = 8,
    .dma_buf_len = 512,// has also been 1024 for testing
    .use_apll = false,          // CHANGE: Turn off APLL for 22kHz stability
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };
  
  // Use the global I2S_PORT variable
  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
  // Force a clock update to be sure
  i2s_set_clk(I2S_PORT, 22050, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);
}
