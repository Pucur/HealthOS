#include <lvgl.h>
#include "esp_lcd_touch_axs5106l.h"
#include <Arduino_GFX_Library.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char* app_version = "v0.6";

#define ROTATION 3
#define GFX_BL 23

#define Touch_I2C_SDA 18
#define Touch_I2C_SCL 19
#define Touch_RST     20
#define Touch_INT     21

#define LEDC_FREQ             5000
#define LEDC_TIMER_10_BIT     10

unsigned long lastTouch = 0;
bool dimmed = false;

const char* ssid = "wifi";
const char* pass = "pass";
const unsigned long WIFI_RECONNECT_INTERVAL = 10000;
unsigned long lastWifiCheck = 0;
String server = "http://192.168.1.1:8000";

lv_obj_t *label_kcal;
lv_obj_t *btn_container;

struct FoodItem {
  String name;
  float kcal_per_100;
};

FoodItem foods[64];
int food_count = 0;

Arduino_DataBus *bus = new Arduino_HWSPI(15, 14, 1, 2);
Arduino_GFX *gfx = new Arduino_ST7789(
  bus, 22, 0, false,
  172, 320,
  34, 0,
  34, 0
);

uint32_t screenWidth;
uint32_t screenHeight;
uint32_t bufSize;

/* ================= LVGL 9 FIX ================= */
lv_draw_buf_t draw_buf;
lv_color_t *disp_draw_buf;
lv_display_t *disp;
/* ============================================== */

void lcd_reg_init(void) {
  static const uint8_t init_operations[] = {
    BEGIN_WRITE,
    WRITE_COMMAND_8, 0x11,
    END_WRITE,
    DELAY, 120,

    BEGIN_WRITE,
    WRITE_C8_D16, 0xDF, 0x98, 0x53,
    WRITE_C8_D8, 0xB2, 0x23,
    WRITE_COMMAND_8, 0xB7,
    WRITE_BYTES, 4, 0x00, 0x47, 0x00, 0x6F,
    WRITE_COMMAND_8, 0xBB,
    WRITE_BYTES, 6, 0x1C, 0x1A, 0x55, 0x73, 0x63, 0xF0,
    WRITE_C8_D16, 0xC0, 0x44, 0xA4,
    WRITE_C8_D8, 0xC1, 0x16,
    WRITE_COMMAND_8, 0xC3,
    WRITE_BYTES, 8, 0x7D, 0x07, 0x14, 0x06, 0xCF, 0x71, 0x72, 0x77,
    WRITE_COMMAND_8, 0xC4,
    WRITE_BYTES, 12, 0x00, 0x00, 0xA0, 0x79, 0x0B, 0x0A, 0x16, 0x79, 0x0B, 0x0A, 0x16, 0x82,
    WRITE_COMMAND_8, 0xC8,
    WRITE_BYTES, 32, 0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28, 0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00, 0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28, 0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00,
    WRITE_COMMAND_8, 0xD0,
    WRITE_BYTES, 5, 0x04, 0x06, 0x6B, 0x0F, 0x00,
    WRITE_C8_D16, 0xD7, 0x00, 0x30,
    WRITE_C8_D8, 0xE6, 0x14,
    WRITE_C8_D8, 0xDE, 0x01,
    WRITE_COMMAND_8, 0xB7,
    WRITE_BYTES, 5, 0x03, 0x13, 0xEF, 0x35, 0x35,
    WRITE_COMMAND_8, 0xC1,
    WRITE_BYTES, 3, 0x14, 0x15, 0xC0,
    WRITE_C8_D16, 0xC2, 0x06, 0x3A,
    WRITE_C8_D16, 0xC4, 0x72, 0x12,
    WRITE_C8_D8, 0xBE, 0x00,
    WRITE_C8_D8, 0xDE, 0x02,
    WRITE_COMMAND_8, 0xE5,
    WRITE_BYTES, 3, 0x00, 0x02, 0x00,
    WRITE_COMMAND_8, 0xE5,
    WRITE_BYTES, 3, 0x01, 0x02, 0x00,
    WRITE_C8_D8, 0xDE, 0x00,
    WRITE_C8_D8, 0x35, 0x00,
    WRITE_C8_D8, 0x3A, 0x05,
    WRITE_COMMAND_8, 0x2A,
    WRITE_BYTES, 4, 0x00, 0x22, 0x00, 0xCD,
    WRITE_COMMAND_8, 0x2B,
    WRITE_BYTES, 4, 0x00, 0x00, 0x01, 0x3F,
    WRITE_C8_D8, 0xDE, 0x02,
    WRITE_COMMAND_8, 0xE5,
    WRITE_BYTES, 3, 0x00, 0x02, 0x00,
    WRITE_C8_D8, 0xDE, 0x00,
    WRITE_C8_D8, 0x36, 0x00,
    WRITE_COMMAND_8, 0x21,
    END_WRITE,
    DELAY, 10,
    BEGIN_WRITE,
    WRITE_COMMAND_8, 0x29,
    END_WRITE
  };
  bus->batchOperation(init_operations, sizeof(init_operations));
}

void wifi_connect() {
  WiFi.begin(ssid, pass);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    if (millis() - start > 20000) break;
  }
}

String http_get(const String& path) {
  if (WiFi.status() != WL_CONNECTED) return "{}";
  HTTPClient http;
  http.begin(server + path);
  int code = http.GET();
  String payload = "{}";
  if (code > 0) payload = http.getString();
  http.end();
  return payload;
}

void send_food_to_server(const char* food, float amount) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(server + "/log");
  http.addHeader("Content-Type", "application/json");

  char body[128];
  snprintf(body, sizeof(body), "{\"food\":\"%s\",\"amount\":%.0f}", food, amount);
  http.POST((uint8_t*)body, strlen(body));
  http.end();

  load_kcal();
}

void add_food_button(const char* name, int index) {

  const int gap = 6;
  const int cols = 3;

  int btn_w = (screenWidth - gap * (cols + 1)) / cols;
  int btn_h = 45;

  int col = index % cols;
  int row = index / cols;

  int x = gap + col * (btn_w + gap);
  int y = gap + row * (btn_h + gap);

  lv_obj_t *btn = lv_btn_create(btn_container);
  lv_obj_set_size(btn, btn_w, btn_h);
  lv_obj_set_pos(btn, x, y);

  lv_obj_t *label = lv_label_create(btn);
  lv_label_set_text(label, name);
  lv_obj_center(label);

  lv_obj_add_event_cb(btn, food_btn_cb, LV_EVENT_LONG_PRESSED, (void*)name);
  lv_obj_set_style_bg_color(btn,lv_color_hex(0xff0000),LV_PART_MAIN | LV_STATE_PRESSED);
}

void clear_food_buttons() {
  if (btn_container) lv_obj_clean(btn_container);
}

void load_foods_from_server() {
  String json = http_get("/foods");

  StaticJsonDocument<2048> doc;
  if (deserializeJson(doc, json)) return;

  JsonArray arr = doc["foods"].as<JsonArray>();
  food_count = 0;

  clear_food_buttons();

  int idx = 0;
  for (JsonObject item : arr) {
    if (idx >= 64) break;
    foods[idx].name = item["name"].as<String>();
    foods[idx].kcal_per_100 = item["kcal_per_100"] | 0.0f;
    idx++;
  }

  food_count = idx;

for (int i = 0; i < food_count; i++) {
    add_food_button(foods[i].name.c_str(), i);
}
}

void load_kcal() {
  String json = http_get("/today");
  StaticJsonDocument<4096> doc;
  if (deserializeJson(doc, json)) return;

  float total = doc["total_kcal"] | 0.0f;

  char buf[64];
  snprintf(buf, sizeof(buf),
         "HealthOS %s || Mai kaloria: %.0f kcal",
         app_version, total);
  lv_label_set_text(label_kcal, buf);

  lv_color_t color;

  if (total <= 2000) color = lv_color_hex(0x00C853);
  else if (total <= 2900) color = lv_color_hex(0xFFD600);
  else color = lv_color_hex(0xD50000);

  lv_obj_set_style_text_color(label_kcal, color, 0);
}

/* ================= LVGL 9 FLUSH FIX ================= */
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;

#if (LV_COLOR_16_SWAP != 0)
  gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
#else
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
#endif

  lv_display_flush_ready(disp);
}
/* =================================================== */

void touchpad_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
  touch_data_t touch_data;
  bsp_touch_read();

  bool pressed = bsp_touch_get_coordinates(&touch_data);

  if (pressed) {
    data->point.x = touch_data.coords[0].x;
    data->point.y = touch_data.coords[0].y;
    data->state = LV_INDEV_STATE_PRESSED;

    lastTouch = millis();

    if (dimmed) {
      ledcWrite(GFX_BL, 800);
      dimmed = false;
    }
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }

  data->continue_reading = false;
}

void food_btn_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  const char* food = (const char*)lv_event_get_user_data(e);

  if (code == LV_EVENT_LONG_PRESSED) {
    send_food_to_server(food, 100);
  }
}

void create_home() {
  lv_obj_t *root = lv_scr_act();

  /* ===== BACKGROUND STYLE ===== */
  static lv_style_t style_bg;
  lv_style_init(&style_bg);

  lv_style_set_bg_color(&style_bg, lv_color_hex(0x1E2A3A));
  lv_style_set_bg_grad_color(&style_bg, lv_color_hex(0x0B0F14));
  lv_style_set_bg_grad_dir(&style_bg, LV_GRAD_DIR_VER);

  lv_obj_add_style(root, &style_bg, 0);

  /* ===== HEADER LABEL ===== */
  label_kcal = lv_label_create(root);

  lv_label_set_text(label_kcal, "HealthOS Loading...");
  lv_obj_align(label_kcal, LV_ALIGN_TOP_MID, 0, 3);

  /* ===== BUTTON AREA ===== */
  btn_container = lv_obj_create(root);
  lv_obj_set_size(btn_container, screenWidth, screenHeight - 20);
  lv_obj_align(btn_container, LV_ALIGN_BOTTOM_MID, 0, 0);

  lv_obj_set_style_bg_opa(btn_container, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(btn_container, 0, 0);
  lv_obj_set_style_pad_all(btn_container, 0, 0);

  lv_obj_set_scroll_dir(btn_container, LV_DIR_VER);
  lv_obj_add_flag(btn_container, LV_OBJ_FLAG_SCROLLABLE);
}

/* ================= SETUP ================= */
void setup() {
  Serial.begin(115200);

  ledcAttach(GFX_BL, LEDC_FREQ, LEDC_TIMER_10_BIT);
  ledcWrite(GFX_BL, 800);

  gfx->begin();
  lcd_reg_init();

  gfx->setRotation(ROTATION);
  gfx->fillScreen(RGB565_BLACK);

  Wire.begin(Touch_I2C_SDA, Touch_I2C_SCL);
  bsp_touch_init(&Wire, Touch_RST, Touch_INT, gfx->getRotation(), gfx->width(), gfx->height());

  lv_init();

  screenWidth = gfx->width();
  screenHeight = gfx->height();
  bufSize = screenWidth * 40;

  disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSize * sizeof(lv_color_t),
                      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

  if (!disp_draw_buf)
    disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSize * sizeof(lv_color_t),
                      MALLOC_CAP_8BIT);

  /* ================= DISPLAY ================= */
  disp = lv_display_create(screenWidth, screenHeight);
  lv_display_set_flush_cb(disp, my_disp_flush);
  lv_display_set_buffers(disp, disp_draw_buf, NULL,
                         bufSize * sizeof(lv_color_t),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  /* ========================================== */

  /* INPUT */
  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touchpad_read_cb);

  wifi_connect();

  create_home();
  load_foods_from_server();
  load_kcal();
}

static uint32_t last_tick = 0;

/* ================= LOOP ================= */
void loop() {
  unsigned long now = millis();

  if (WiFi.status() != WL_CONNECTED && now - lastWifiCheck > WIFI_RECONNECT_INTERVAL) {
    lastWifiCheck = now;
    WiFi.disconnect();
    WiFi.begin(ssid, pass);
  }



  static unsigned long last = 0;
  if (millis() - last > 30000) {
    last = millis();
    load_foods_from_server();
  }

  if (!dimmed && millis() - lastTouch > 30000) {
    ledcWrite(GFX_BL, 50);
    dimmed = true;
  }

  uint32_t noww = millis();
  lv_tick_inc(noww - last_tick);
  last_tick = noww;

  lv_timer_handler();
  delay(5);
}
