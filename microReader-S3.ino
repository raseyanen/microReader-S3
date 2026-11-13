/*
  microReader-S3 — электронная шпаргалка на ESP32-S3 Super Mini + e-paper WeAct 1.54" (200x200)
  Файлы .txt / .md (текст, формулы, таблицы, графики) / .tex. Файлы заливаются по USB:
  устройство открывается на ПК как USB-накопитель (удержание центра джойстика в меню).

  Настройки Arduino IDE (ядро esp32 2.0.14+ или 3.x):
    Board:               ESP32S3 Dev Module
    USB Mode:            USB-OTG (TinyUSB)      <-- обязательно, иначе накопитель не появится
    USB CDC On Boot:     Disabled
    USB Firmware MSC On Boot: Disabled
    Flash Size:          4MB (32Mb)
    PSRAM:               Disabled (если у вашей платы нет PSRAM, иначе будет перезагрузка)
    Partition Scheme:    любая - используется partitions.csv из папки скетча (раздел ffat)
  Библиотеки: GxEPD2 (+ Adafruit GFX), EncButton.
  Прошивка по USB: зажать BOOT, коротко нажать RESET, отпустить BOOT, затем Upload
  (в режиме USB-OTG автосброс недоступен).
*/
#include "config.h"
#include <Arduino.h>
#include <strings.h>
#include <SPI.h>
#include <FFat.h>
#include <Preferences.h>
#include <EncButton.h>
#include <GxEPD2_BW.h>
#include "driver/rtc_io.h"
#include "esp_sleep.h"

GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(GxEPD2_154_D67(EPD_CS_PIN, EPD_DC_PIN, EPD_RST_PIN, EPD_BUSY_PIN));

#include "screen.h"
Screen oled;                       // логический экран (имя сохранено для совместимости с кодом рендера)

Button jUp(JOY_UP_PIN), jDn(JOY_DN_PIN), jLf(JOY_LF_PIN), jRt(JOY_RT_PIN), jOk(JOY_OK_PIN);
Button bA(BTN_A_PIN), bB(BTN_B_PIN);
#include "input.h"
KeyGroup up(&jUp, &jLf, &bA);      // вверх / влево / кнопка A
KeyGroup down(&jDn, &jRt, &bB);    // вниз / вправо / кнопка B
KeyGroup ok(&jOk);                 // центр джойстика

/* ============ Глобальные переменные ============ */
String selectedFile = "";                 // имя выбранного файла
char fileList[MAX_FILES][FILE_NAME_MAX];  // читаемые файлы
int16_t fileCount = 0;                    // их количество
int16_t cursor = 0;                       // курсор меню
uint32_t uiTimer = 0;                     // таймер бездействия
uint32_t resumeVal = 0;                   // страница/позиция для продолжения чтения после сна
int16_t texClipTop = 0;                   // отсечка рисования сверху (используется рендером формул)
bool fsOk = false;

/* ============ Прототипы функций из других вкладок ============ */
void checkFileSystem(void);
void drawMainMenu(void);
void fileReadError(void);
void uiMessage(const char* l1, const char* l2, const char* l3);
void enterToReadTxtFile(void);
void enterToReadMdFile(void);
void enterToReadTexFile(void);
void usbMode(void);
void waitOkRelease(void);
void sleepNow(char kind, uint32_t arg);
void writeDemoFiles(void);
// static-функции рендера (определены в texMath.ino), объявлены заранее - порядок вкладок не важен
static uint16_t texU8Next(const char*& s);
static uint8_t texU8Len(const char* s);
static void texDrawCp(uint16_t cp, int16_t x, int16_t y);
static void texChar5(char ch, int16_t x, int16_t y);
static void texChar3(char ch, int16_t x, int16_t y);
static void texHLine(int16_t x0, int16_t x1, int16_t y);
static void texVLine(int16_t x, int16_t y0, int16_t y1);

/* ================= Глубокий сон ================= */
// Страница остаётся на экране (e-paper не требует питания). Разбудить - нажатие центра джойстика.
// kind: 0 - просто сон; 'T' / 'M' / 'X' - в каком читателе были (txt / md / tex), arg - страница или позиция
void sleepNow(char kind, uint32_t arg) {
  Preferences p;
  p.begin("mr", false);
  p.putUChar("kind", (uint8_t)kind);
  p.putUInt("arg", arg);
  p.putString("file", selectedFile);
  p.end();
  display.hibernate();
  esp_sleep_enable_ext0_wakeup((gpio_num_t)JOY_OK_PIN, 0);     // пробуждение по низкому уровню на центре
  rtc_gpio_pullup_en((gpio_num_t)JOY_OK_PIN);
  rtc_gpio_pulldown_dis((gpio_num_t)JOY_OK_PIN);
  esp_deep_sleep_start();
}

// после отпускания центра: иначе главный цикл увидит удержание ещё раз
void waitOkRelease(void) {
  uint32_t t = millis();
  while (!digitalRead(JOY_OK_PIN) && millis() - t < 5000) { delay(5); yield(); }
  ok.tick();
  ok.tick();
}

/* ================= Открытие файла ================= */
static bool endsWithI(const char* s, const char* ext) {
  size_t a = strlen(s), b = strlen(ext);
  return a >= b && strcasecmp(s + a - b, ext) == 0;
}

static void openByName(void) {
  uiTimer = millis();
  const char* n = selectedFile.c_str();
  if (endsWithI(n, ".md")) enterToReadMdFile();
  else if (endsWithI(n, ".tex")) enterToReadTexFile();
  else enterToReadTxtFile();
}

/* ================= setup / loop ================= */
void setup() {
  Serial.begin(115200);                         // при USB CDC On Boot: Disabled - это UART0 (GPIO43/44)
  const uint8_t pins[] = {JOY_UP_PIN, JOY_DN_PIN, JOY_LF_PIN, JOY_RT_PIN, JOY_OK_PIN, BTN_A_PIN, BTN_B_PIN};
  for (uint8_t i = 0; i < sizeof(pins); i++) pinMode(pins[i], INPUT_PULLUP);
  jOk.setHoldTimeout(800);
  jUp.setStepTimeout(350); jDn.setStepTimeout(350); jLf.setStepTimeout(350); jRt.setStepTimeout(350);
  bA.setStepTimeout(350);  bB.setStepTimeout(350);

  oled.begin();                                 // e-paper
  bool woke = (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0);

  if (!FFat.begin(false)) {                     // раздел ffat не отформатирован - форматируем
    uiMessage("Формат ФС...", "первый запуск", "");
    fsOk = FFat.format() && FFat.begin(false);
  } else fsOk = true;
  Serial.println(fsOk ? F("ffat ok") : F("ffat ОШИБКА: проверьте partitions.csv"));

  if (!fsOk) {
    uiMessage("ОШИБКА ФС", "partitions.csv?", "см. README");
    delay(5000);
    sleepNow(0, 0);
  }

  if (!digitalRead(JOY_OK_PIN)) {               // центр зажат при включении - сразу режим USB
    waitOkRelease();
    usbMode();
  }

  checkFileSystem();
  if (fileCount == 0) {                         // пустой диск - кладём примеры
    writeDemoFiles();
    checkFileSystem();
  }

  uiTimer = millis();
  if (woke) {                                   // проснулись - продолжаем чтение
    Preferences p;
    p.begin("mr", true);
    uint8_t kind = p.getUChar("kind", 0);
    resumeVal = p.getUInt("arg", 0);
    String f = p.getString("file", "");
    p.end();
    if (kind && f.length()) {
      for (int16_t i = 0; i < fileCount; i++) if (f == fileList[i]) cursor = i;
      selectedFile = f;
      openByName();
      return;
    }
  }
  drawMainMenu();
}

void loop() {
  up.tick(); ok.tick(); down.tick();

  if (fileCount > 0) {
    if (up.click() || up.step()) {
      uiTimer = millis();
      if (cursor > 0) { cursor--; drawMainMenu(); }
    } else if (down.click() || down.step()) {
      uiTimer = millis();
      if (cursor < fileCount - 1) { cursor++; drawMainMenu(); }
    }
    if (ok.click()) {
      selectedFile = fileList[cursor];
      openByName();
    }
  }

  if (ok.hold()) {                              // удержание центра - USB-накопитель
    waitOkRelease();
    usbMode();                                  // не возвращается (перезагрузка)
  }

  if (millis() - uiTimer > SLEEP_MS) sleepNow(0, 0);
  yield();
}
