#include "config.h"
#include <WiFi.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <driver/i2s.h>
#include <soc/i2s_reg.h>
#include "time.h"
static bool irq = false;

TTGOClass *ttgo;
TFT_eSPI *tft;

const char *ssid     = "your_ap_ssid";
const char *password = "your_ap_password";
// ip address of the machine running the simple sound server
const char *tcp_addr = "your_simple_sound_server_ip_address";
// your simple_sound server port number, make sure it matches whatever is in mic_server.py
const int tcp_port   = 3334; 

int seconds_per_pass = 10;
int passes = 5;

// forward defs
static boolean I2SSetup();
static TaskHandle_t recordTaskHandle = NULL;
static void record(uint16_t limit_seconds);
static void record_task(void *pvParameters);

void setup()
{
  Serial.begin(115200);
  ttgo = TTGOClass::getWatch();
  
  ttgo->begin();
  
  ttgo->openBL();
  
  ttgo->setBrightness(128);       // 0~255
  
  
  tft = ttgo->tft;
  
  tft->setTextColor(TFT_GREEN, TFT_BLACK);
  
  //connect to WiFi
  
  Serial.printf("Connecting to %s \n", ssid);
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    tft->print(".");
  }
  tft->println(" CONNECTED\n");
  Serial.println(" CONNECTED");
  
  // setup for short press deep sleep
  pinMode(AXP202_INT, INPUT_PULLUP);
  attachInterrupt(AXP202_INT, [] {
				irq = true;
			      }, FALLING);
  //!Clear IRQ unprocessed  first
  ttgo->power->enableIRQ(AXP202_PEK_SHORTPRESS_IRQ, true);
  ttgo->power->clearIRQ();

  // Start a task and pin it to the core, from which the actual record operation
  // is run. This seems to eliminate the dropped data
  BaseType_t xReturned;
  xReturned =
    xTaskCreatePinnedToCore(record_task,
			    "RecordTask",     /* Text name for the task. */
			    8192,             /* Stack size in words, not bytes. */
			    NULL,             /* Parameter passed into the task. */
			    1,                /* Task Priority */
			    &recordTaskHandle,/* Where to put created task's handle.*/
			    1);               /* core to pin to, don't use 0 */
  
  if(xReturned != pdPASS ) {
    // must be too little memory
    Serial.println("could not start record_task");
  } else {
    Serial.println("started record_task");
  }
}
static void record_task( void * pvParameters )
{
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
  recordTaskHandle = NULL;
  vTaskDelete(NULL);
}
void loop()
{
  if (!irq) {
    delay(1);
    return;
  }
  // button pressed, see if short, then do deep sleep
  irq = false;
  ttgo->power->readIRQ();
  if (ttgo->power->isPEKShortPressIRQ()) {
    ttgo->power->clearIRQ();
    //ttgo->displaySleep();
    ttgo->powerOff();
    // PEK KEY  Wakeup source
    esp_sleep_enable_ext1_wakeup(GPIO_SEL_35, ESP_EXT1_WAKEUP_ALL_LOW);
    esp_deep_sleep_start();
  } else {
    ttgo->power->clearIRQ();
  }
}

#define TCP_SAMPLE_SIZE 2048
#define BYTES_PER_SAMPLE sizeof(uint16_t)
//#define BYTES_PER_SAMPLE sizeof(uint32_t)
#define TCP_BUFFER_SIZE TCP_SAMPLE_SIZE * BYTES_PER_SAMPLE 
// This will set the size of the DMA buffers in samples, not bytes.
// The i2s_config will setup 4 of these. If you don't process
// data fast enough, calling i2s_read often enough to keep ahead
// of the sample stream from the microphone, you will loose data.
#define DMA_SAMPLE_COUNT 1024

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
  i2s_bits_per_sample_t bits_per = I2S_BITS_PER_SAMPLE_32BIT;
  if (BYTES_PER_SAMPLE == sizeof(uint16_t)) {
    bits_per = I2S_BITS_PER_SAMPLE_16BIT;
  }
  i2s_config = {
		.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
		.sample_rate =  sample_rate,
		.bits_per_sample = bits_per,
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
  uint16_t buf_len = TCP_BUFFER_SIZE;
  char *buf = (char *)calloc(buf_len, sizeof(char));
  int total_bytes_read = 0;
  int total_samples_read = 0;
  uint16_t lcount = 0;
  unsigned long start_time = millis();
  boolean timesup = false;
  boolean error_exit = false;
  int no_data_reads = 0;
  WiFiClient client;
  unsigned long timeout;
  Serial.printf("Starting read loop\n");
  if (!client.connect(tcp_addr, tcp_port)) {
    Serial.println("Connection to host failed");
    error_exit = true;
    return;
  }
  Serial.println("TCP connection open");
  timeout = millis();
  // server sends "go", and we wait for it, thus reducing the
  // chance that the first few sends block on this device because
  // the server is not ready and does not ack the packets. If that
  // happens enough, the data from the mic could get overwritten
  // by DMA while wait for the send.
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.printf("Client Timeout waiting for TCP socket to say go");
      error_exit = true;
      break;
    }
  }
  // read the "go" message so that the read data is cleared,
  // thus allowing a similar sync step at the end.
  size_t junk_size = 2;
  uint8_t junk[junk_size];
  client.read(junk, junk_size);
  // send sample rate
  Serial.println("TCP go signal read done, sending samle rate");
  // force it to big endian
  uint8_t sbuf[4];
  sbuf[0] = sample_rate >> 24;
  sbuf[1] = sample_rate >> 16;
  sbuf[2] = sample_rate >> 8;
  sbuf[3] = sample_rate;
  client.write(sbuf, 4);
  start_time = millis();
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
      delay(1); // allow yield if other same priority task is ready
    }
    if (timesup || error_exit) {
      break;
    }
    lcount++;
    total_bytes_read += bytes_read;
    uint32_t samples_read = bytes_read / BYTES_PER_SAMPLE;
    total_samples_read += samples_read;
    client.write(buf, bytes_read);

    if (millis() - start_time > limit_seconds * 1000) {
      timesup = true;
      break;
    }
  }
  // There is no explicit close for the client, or therefore the socket,
  // it gets done as part of destructor when the client object goes out of scope.
  // We do need to make sure the data is all sent before that happens.
  client.flush();
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

