#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>

#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

const char* ssid = "LEPI";
const char* password = "12344321";

#define LED_PIN 4  // GPIO untuk LED eksternal

WebServer server(80);

bool isDrowsy = false;
unsigned long lastBlinkTime = 0;
bool ledState = false;

// Fungsi menangani status drowsy/neutral dari Python
void handleStatus() {
  String status = server.arg("status");

  Serial.println("===[ /status Called ]===");
  Serial.print("status: ");
  Serial.println(status);

  if (status == "drowsy") {
    isDrowsy = true;
    Serial.println("STATUS: Drowsy detected");
  } else if (status == "neutral") {
    isDrowsy = false;
    digitalWrite(LED_PIN, LOW);
    Serial.println("STATUS: Neutral (no drowsiness)");
  } else {
    Serial.println("STATUS: Unknown");
  }

  server.send(200, "text/plain", "OK");
}

// Fungsi streaming kamera bawaan
void startCameraServer();

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  // Konfigurasi kamera
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
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_VGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 10;
  config.fb_count = 2;

  // Inisialisasi kamera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  // Optional tweak sensor
  sensor_t *s = esp_camera_sensor_get();
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);
    s->set_brightness(s, 1);
    s->set_saturation(s, -2);
    s->set_gainceiling(s, (gainceiling_t)4);
  }
  s->set_framesize(s, FRAMESIZE_VGA);

  // Setup LED eksternal
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Koneksi WiFi
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Mulai server kamera
  startCameraServer();

  // Tambahkan handler /status
  server.on("/status", handleStatus);
  server.begin();
  Serial.println("Web server started");
}

void loop() {
  server.handleClient();

  if (isDrowsy) {
    unsigned long now = millis();
    if (now - lastBlinkTime >= 300) {
      lastBlinkTime = now;
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
    }
  }
}
