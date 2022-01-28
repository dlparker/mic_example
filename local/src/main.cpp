#include "config.h"
#include <driver/i2s.h>
#include <soc/i2s_reg.h>
#include "time.h"

TTGOClass *ttgo;
TFT_eSPI *tft;

int seconds_per_pass = 5;
int passes = 5;

// forward defs
static boolean I2SSetup();
static void record(uint16_t limit_seconds);

void setup()
{
    Serial.begin(115200);
    ttgo = TTGOClass::getWatch();

    ttgo->begin();

    ttgo->openBL();

    ttgo->setBrightness(128);       // 0~255


    tft = ttgo->tft;

    tft->setTextColor(TFT_GREEN, TFT_BLACK);

    tft->print("Starting run of ");
    tft->print(passes);
    tft->print(" of ");
    tft->print(seconds_per_pass);
    tft->print(" each ");
    tft->println();
    Serial.printf("Starting run of %d passes of %d seconds each \n", passes, seconds_per_pass);
    for (int i = 0; i < passes; i++) {
      Serial.printf("pass %d\n", i);
      I2SSetup();
      Serial.printf("pass %d setup done\n", i);
      record(seconds_per_pass);
      Serial.printf("pass %d record done\n", i);
      tft->print("pass done ");
      tft->println();
    }
}

void loop()
{
  delay(1);
}


#define BUFFER_SIZE 1024
//#define DMA_SAMPLE_COUNT (BUFFER_SIZE/8)
#define DMA_SAMPLE_COUNT BUFFER_SIZE

// TWATCH 2020 V3 PDM microphone pin
#define MIC_DATA_PIN            2
#define MIC_CLOCK_PIN           0

static int sample_rate = 32000;

static boolean I2SSetup() 
{
  esp_err_t err;
  i2s_config_t i2s_config;
  // Phillips type microphone SPH0645 gives 18 bits of data, but it is a signed form, so reading 32 bits works.
  // Many examples on the web use 16 bits, but they miss some quality and have to fiddle the data. This is best.
  i2s_config = {
		.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
		.sample_rate =  sample_rate,
		.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
		.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
		.communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
		.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
		.dma_buf_count = 4,
		.dma_buf_len = DMA_SAMPLE_COUNT,
  };
      

  i2s_pin_config_t i2s_pin_cfg;
  i2s_pin_cfg.bck_io_num   = I2S_PIN_NO_CHANGE;
  i2s_pin_cfg.ws_io_num    = MIC_CLOCK_PIN;
  i2s_pin_cfg.data_out_num = I2S_PIN_NO_CHANGE;
  i2s_pin_cfg.data_in_num  = MIC_DATA_PIN;

  err = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    tft->print(" !!!! Failed installing driver !!!");
    Serial.printf(" ! Failed installing driver: %d\n", err);
    return false;
  }
  // see https://reversatronics.blogspot.com/2020/06/i2s-microphones-on-esp32-how-high-can-i.html
  // and Manuel's comment at https://hackaday.io/project/162059-street-sense/log/160705-new-i2s-microphone
  REG_SET_BIT(I2S_TIMING_REG(I2S_NUM_0), BIT(9));          // I2S_RX_SD_IN_DELAY to make SD read delay to catch MSB
  REG_SET_BIT(I2S_CONF_REG(I2S_NUM_0), I2S_RX_MSB_SHIFT);  // Phillips I2S - WS changes a cycle earlier

  err = i2s_set_pin(I2S_NUM_0, &i2s_pin_cfg);
  if (err != ESP_OK) {
    tft->print(" !!!! Failed setting pin config !!!");
    Serial.printf(" ! Failed setting pin: %d\n", err);
    return false;
  }
  Serial.printf("sample rate is %d\n", sample_rate);
  Serial.println(" + I2S driver installed.");
  return true;
}

static void record(uint16_t limit_seconds) {
  uint16_t buf_len = 1024;
  char *buf = (char *)calloc(buf_len, sizeof(char));
  int total_bytes_read = 0;
  uint32_t total_samples_read = 0;
  uint16_t lcount = 0;
  unsigned long start_time = millis();
  boolean timesup = false;
  boolean error_exit = false;
  int no_data_reads = 0;
  Serial.printf("Starting read loop\n");
  while (true) {
    size_t bytes_read = 0;
    int no_data_loops = 0;
    while(bytes_read == 0) {
      i2s_read(I2S_NUM_0, buf, buf_len, &bytes_read, 0);
      if (bytes_read > 0) {
	break;
      }
      no_data_reads++; // just a counter to report how much this happens
      if (no_data_loops++ > 50) {
	if (millis() - start_time > limit_seconds * 1000) {
	  timesup = true;
	  break;
	}
	no_data_loops = 0;
      }
      delay(1); // allow yeild if other same priority task is ready
    }
    if (timesup || error_exit) {
      break;
    }
    lcount++;
    total_bytes_read += bytes_read;
    uint32_t samples_read = bytes_read / 4;
    total_samples_read += samples_read;

    if (millis() - start_time > limit_seconds * 1000) {
      timesup = true;
      break;
    }
  }
  if (timesup) {
    Serial.printf("timer done after %d seconds\n", limit_seconds);
  }
  if (error_exit) {
    Serial.printf("exit triggered by error\n");
  }
  if (start_time != 0) {
    Serial.printf("%d bytes, %d samples done in %d loops\n", total_bytes_read, total_samples_read, lcount);
    float calced = (float)total_samples_read/(float)sample_rate;
    Serial.printf("%f seconds calculated by samples_read/sample rate\n", calced);
    Serial.printf("%d reads with no data ready\n", no_data_reads);
  }
  i2s_driver_uninstall(I2S_NUM_0);
  free(buf);
}

