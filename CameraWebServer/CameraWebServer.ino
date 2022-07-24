#include "esp_camera.h"
#include <WiFi.h>
#include <esp_task_wdt.h>

#include "FS.h"
#include "SD_MMC.h"

//
// WARNING!!! PSRAM IC required for UXGA resolution and high JPEG quality
//            Ensure ESP32 Wrover Module or other board with PSRAM is selected
//            Partial images will be transmitted if image exceeds buffer size
//

// Select camera model
//#define CAMERA_MODEL_WROVER_KIT // Has PSRAM
//#define CAMERA_MODEL_ESP_EYE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_PSRAM // Has PSRAM
//#define CAMERA_MODEL_M5STACK_V2_PSRAM // M5Camera version B Has PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_ESP32CAM // No PSRAM
#define CAMERA_MODEL_AI_THINKER // Has PSRAM
//#define CAMERA_MODEL_TTGO_T_JOURNAL // No PSRAM
#define WDT_TIMEOUT 20
#include "camera_pins.h"

#ifdef __cplusplus
  extern "C" {
 #endif

  uint8_t temprature_sens_read();

#ifdef __cplusplus
}
#endif

hw_timer_t * g_timer = NULL;
uint8_t  __thermalShutdown = 0;
void startCameraServer();
int StringSplit(String sInput, char cDelim, String sParams[], int iMaxParams);
//void setup_global_timer();
unsigned long taskDelay = 0;
unsigned long snapShotTimer = 0;

bool snapShotEnabled = false;
//void onTimer();
extern void createDir(fs::FS &fs, const char * path);
extern void readFile(fs::FS &fs, const char * path, uint8_t **ppbuf, int *pLen);
extern void snapshot_timer();

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  esp_task_wdt_init(WDT_TIMEOUT, true); //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL); //add current thread to WDT watch

  

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

 pinMode(4, OUTPUT);
#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1); // flip it back
    s->set_brightness(s, 1); // up the brightness just a bit
    s->set_saturation(s, -2); // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  s->set_framesize(s, FRAMESIZE_QVGA);

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

  if(!SD_MMC.begin())
  {
        Serial.println("Card Mount Failed");       
  }
  else
  {
    uint8_t cardType = SD_MMC.cardType();

    if(cardType == CARD_NONE)
    {
        Serial.println("No SD card attached");
        return;        
    }

    Serial.print("SD Card Type: ");
    if(cardType == CARD_MMC){
        Serial.println("MMC");
    } else if(cardType == CARD_SD){
        Serial.println("SDSC");
    } else if(cardType == CARD_SDHC){
        Serial.println("SDHC");
    } else {
        Serial.println("UNKNOWN");
    }

    char *unsecureBuffer = NULL;
    int buffer_len = 0;
    
    readFile(SD_MMC, "/wifiData.txt", (uint8_t **)&unsecureBuffer, &buffer_len);

    if (unsecureBuffer != NULL)
    {
        char params[2][80];

        Serial.printf("buffer length:%d\n\r", buffer_len);
        
        if (StringSplit((char *)unsecureBuffer, buffer_len - 2, ' ', params, 2) != 2)
        {
            Serial.println("failed to retrieve secure data");
        }
        else
        {
           Serial.printf("connecting with %s - %s\n", params[0], params[1]);
           WiFi.begin(params[0], params[1]);

           while (WiFi.status() != WL_CONNECTED) {
              delay(500);
              Serial.print(".");
            }
           Serial.println("");
           Serial.println("WiFi connected");
          
           startCameraServer();
          
           Serial.printf("Camera Ready! Use 'http://%s to connect\n\r", WiFi.localIP().toString().c_str());
      
           createDir(SD_MMC, "/pics");
      
           //setup_global_timer();   
      
           taskDelay = millis();
           snapShotTimer = millis();                    
        }
        free (unsecureBuffer);      
    }
    else
    {
        Serial.println("failed to read file");
    } //else  
    
  }//else
}

void loop()
{
  if (snapShotEnabled)
  {
    //
    // see if timer is ready
    //
    if ((millis() - snapShotTimer) > 300)
    {
      snapshot_timer();
      snapShotTimer = millis();
      esp_task_wdt_reset();
    }
  }

  if ( (millis() - taskDelay) > 5000)
  {
      float temp = (temprature_sens_read() - 32) / 1.8;
      
      Serial.print(temp);
      Serial.println(" C");
      
      if (temp > 82.0)
      {
        //
        // shut things down
        //
        sensor_t * s = esp_camera_sensor_get(); 
        s->set_colorbar(s, 0);
        digitalWrite(4, LOW );
        __thermalShutdown = 1;
      }
      else
      {
        //
        // because the temperature return is transient we cannot reliably
        // depend on it. So as soon as the temperature is below 82C
        // we allow the camera to operate
        //
        __thermalShutdown = 0;  
      }

      if (WiFi.status() == WL_CONNECTED)
      {
          //Serial.println("Resetting WDT timer");
          esp_task_wdt_reset();
      }
      taskDelay = millis();
  } 
}

int StringSplit(char *sInput, int inputLen, char cDelim, char sParams[][80], int iMaxParams)
{
    int iParamCount = 0;
    int iPosDelim, iPosCurrent = 0;

    while (*sInput && inputLen > 0)
    {
      if (*sInput != cDelim)
      {
         Serial.printf("SplitString %c , pos=%d, index=%d\n", *sInput, iPosCurrent, iParamCount);
         sParams[iParamCount][iPosCurrent] = *sInput;
         iPosCurrent++;
      }
      else
      {
         Serial.printf("SplitString end of string - length remaining %d pos=%d, index=%d", inputLen, iPosCurrent, iParamCount);
         sParams[iParamCount][iPosCurrent] = '\0';
         iParamCount++;
         iPosCurrent = 0;        
      }

      sInput++;
      inputLen--;
    }

    Serial.printf("SplitString end of string - length remaining %d, index=%d, pos=%d\n", inputLen, iParamCount, iPosCurrent);
    sParams[iParamCount][iPosCurrent] = '\0';

    iParamCount++;

    return (iParamCount);
}
