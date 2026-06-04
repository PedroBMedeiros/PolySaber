//Plays all sounds in .h library one by one.

#include "driver/i2s.h"
#include "ALLSOUNDS.h" 

// Define your available output pins
#define I2S_BCK  26  // Bit Clock
#define I2S_WS   25  // Word Select (LRC)
#define I2S_DOUT 27  // Data Out (DIN)

const i2s_port_t I2S_PORT = I2S_NUM_0;
const int sampleRate = 22050; // Set to match your 16-bit PCM export


// --------------------------------- SOUNDS ----------------------------------

const unsigned char* const strikes[] PROGMEM  = {
  SK1_wav, SK2_wav, SK3_wav, SK4_wav, SK5_wav, SK6_wav, SK7_wav, SK8_wav
};
const size_t strikes_lengths[] = {
  sizeof(SK1_wav), sizeof(SK2_wav), sizeof(SK3_wav), sizeof(SK4_wav),
  sizeof(SK5_wav), sizeof(SK6_wav), sizeof(SK7_wav), sizeof(SK8_wav)
};
int strike_time[8] = {779, 563, 687, 702, 673, 661, 666, 635};

const unsigned char* const strikes_short[] PROGMEM = {
  SKS1_wav, SKS2_wav, SKS3_wav, SKS4_wav,
  SKS5_wav, SKS6_wav, SKS7_wav, SKS8_wav
};
const size_t strikes_short_lengths[] = {
  sizeof(SKS1_wav), sizeof(SKS2_wav), sizeof(SKS3_wav), sizeof(SKS4_wav),
  sizeof(SKS5_wav), sizeof(SKS6_wav), sizeof(SKS7_wav), sizeof(SKS8_wav)
};
int strike_s_time[8] = {270, 167, 186, 250, 252, 255, 250, 238};

const unsigned char* const swings_S[] PROGMEM  = {
  SWS1_wav, SWS2_wav, SWS3_wav, SWS4_wav, SWS5_wav
};
const size_t swings_S_lengths[] = {
  sizeof(SWS1_wav), sizeof(SWS2_wav), sizeof(SWS3_wav), 
  sizeof(SWS4_wav), sizeof(SWS5_wav)
};
int swing_time_S[5] = {389, 372, 360, 366, 337};

const unsigned char* const swings_L[] PROGMEM  = {
  SWL1_wav, SWL2_wav, SWL3_wav, SWL4_wav
};
const size_t swings_L_lengths[] = {
  sizeof(SWL1_wav), sizeof(SWL2_wav), sizeof(SWL3_wav), sizeof(SWL4_wav)
};
int swing_time_L[4] = {636, 441, 772, 702};

const int STRIKES_COUNT = sizeof(strikes_lengths) / sizeof(strikes_lengths[0]);
const int STRIKES_SHORT_COUNT = sizeof(strikes_short_lengths) / sizeof(strikes_short_lengths[0]);
const int SWINGS_COUNT = sizeof(swings_S_lengths)/sizeof(swings_S_lengths[0]);
const int SWINGS_L_COUNT = sizeof(swings_L_lengths)/sizeof(swings_L_lengths[0]);

char BUFFER[10];
// --------------------------------- SOUNDS ---------------------------------



void setup() {

  Serial.begin(115200);
  i2s_init(); 
  play_all(1, 1000);
}  

void loop() {

 delay(3000);

}


// volume_shift: 0 = full volume, 1 = half, 2 = quarter, 4 = very quiet
void play_audio(const unsigned char* wav_data, size_t wav_len, uint8_t volume_shift) {
    const int CHUNK_SIZE = 512; 
    int16_t buffer[CHUNK_SIZE]; 
    size_t bytes_written;
    uint32_t offset = 44; 

    while (offset < wav_len) {
        int samples_to_read = CHUNK_SIZE;
        if (offset + (CHUNK_SIZE * 2) > wav_len) {
            samples_to_read = (wav_len - offset) / 2;
        }

        for (int i = 0; i < samples_to_read; i++) {
            int16_t sample = (int16_t)((wav_data[offset + 1] << 8) | wav_data[offset]);
            buffer[i] = sample >> volume_shift;
            offset += 2;
        }

        i2s_write(I2S_NUM_0, buffer, samples_to_read * 2, &bytes_written, portMAX_DELAY);
    }

    // --- ADD THIS TO STOP THE NOISE ---
    // This clears the internal DMA buffers so they don't loop the last chunk
    i2s_zero_dma_buffer(I2S_NUM_0); 
    
    // Optional: Completely stop the I2S clock if you aren't playing anything else
    // i2s_stop(I2S_NUM_0); 
}

void play_from_collection(const unsigned char* const collection[], const size_t sizes[], int index, uint8_t volume) {
    play_audio(collection[index], sizes[index], volume);
  // Usage:
  //play_from_collection(swings_L, swings_L_lengths, 0, 4); // Plays SWL1_wav
}

void play_all(int volume, int wait) {
  
  Serial.println("On");
  play_audio(ON_wav, ON_wav_len, volume);
  delay(wait);
  Serial.println("Hum");
  play_audio(HUM_wav, HUM_wav_len, volume);
  delay(wait);
  
  for (int i =0; i<8; i++) {
    Serial.print("Strike ");
    Serial.println(i);
    play_audio(strikes[i], strikes_lengths[i], volume);
    delay(wait);
  }
  for (int i =0; i<8; i++) {
    Serial.print("Strike short ");
    Serial.println(i);
    play_audio(strikes_short[i], strikes_short_lengths[i], volume);
    delay(wait);
  }
  for (int i =0; i<5; i++) {
    Serial.print("Swing S");
    Serial.println(i);
    play_audio(swings_S[i], swings_S_lengths[i], volume);
    delay(wait);
  }
  for (int i =0; i<4; i++) {
    Serial.print("Swing L ");
    Serial.println(i);
    play_audio(swings_L[i], swings_L_lengths[i], volume);
    delay(wait);
  }
  Serial.println("OFF");
  play_audio(OFF_wav, sizeof(OFF_wav), volume);
  delay(wait);


  
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



//void playAudioWithFX(const unsigned char* data, uint32_t len) {
//  size_t written;
//  uint32_t pos = 44; // Skip WAV header
//  float volume = 0.3; // 0.1 is quiet, 1.0 is full volume
//
//  // Temporary buffer to hold "dimmed" audio samples
//  const int chunk_size = 512;
//  int16_t volumeBuffer[chunk_size / 2]; 
//  
//  while (pos < len) {
//
//    // 1. Prepare a chunk of audio
//    uint32_t step = (len - pos > chunk_size) ? chunk_size : (len - pos);
//    int numSamples = step / 2;
//
//    // 2. APPLY VOLUME REDUCTION
//    for (int i = 0; i < numSamples; i++) {
//      // Combine two bytes into one 16-bit signed integer (Little Endian)
//      int16_t sample = (int16_t)((data[pos + (i * 2 + 1)] << 8) | data[pos + (i * 2)]);
//      
//      // Multiply by volume (0.0 - 1.0)
//      volumeBuffer[i] = (int16_t)(sample * volume);
//    }
//
//    // 3. Write the "dimmed" buffer to I2S
//    i2s_write(I2S_PORT, volumeBuffer, numSamples * 2, &written, portMAX_DELAY);
//    pos += step;
//
//  }
//}
