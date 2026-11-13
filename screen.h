#pragma once
/* Логический экран SCR_W x SCR_H (1 бит на пиксель) поверх e-paper. Интерфейс совместим с кодом
   рендера формул/таблиц/графиков: dot(), clear(), update(). update() обновляет панель:
   частичное обновление (быстро, ~0.5 с), раз в EPD_FULL_EVERY раз - полное (убирает ореолы). */
class Screen {
 public:
  void begin() {
    SPI.begin(EPD_SCK_PIN, -1, EPD_MOSI_PIN, EPD_CS_PIN);
    display.init(0, true, 2, false);              // initial=true: первое обновление - полное
    display.setRotation(0);
    clear();
  }
  void clear() { memset(fb, 0, sizeof(fb)); }
  void dot(int16_t x, int16_t y, uint8_t v) {
    if ((uint16_t)x >= SCR_W || (uint16_t)y >= SCR_H) return;
    uint16_t i = (uint16_t)y * SCR_W + x;
    if (v) fb[i >> 3] |= (1 << (i & 7)); else fb[i >> 3] &= ~(1 << (i & 7));
  }
  void autoPrintln(bool) {}                       // заглушка для совместимости
  void forceFull() { partial = 255; }
  void update(bool full = false) {
    bool doFull = full || partial >= EPD_FULL_EVERY;
    if (doFull) { display.setFullWindow(); partial = 0; }
    else { display.setPartialWindow(0, 0, display.width(), display.height()); partial++; }
    display.firstPage();
    do {
      display.fillScreen(GxEPD_WHITE);
      for (int16_t y = 0; y < SCR_H; y++)
        for (int16_t x = 0; x < SCR_W; x++) {
          uint16_t i = (uint16_t)y * SCR_W + x;
          if (fb[i >> 3] & (1 << (i & 7))) display.fillRect(x * EPD_SCALE, y * EPD_SCALE, EPD_SCALE, EPD_SCALE, GxEPD_BLACK);
        }
    } while (display.nextPage());
  }
 private:
  uint8_t fb[SCR_W * SCR_H / 8];
  uint8_t partial = 255;
};
